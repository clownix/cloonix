// cr-play.js — Demo panel (the cloonix commands of a demo) and its Comments
// panel (the "#story# " lines).

(function () {
  "use strict";

  let g_demo_seq = 1;   // monotonic id so multiple demo panels can coexist
  var STORY_MARK = "#story# ";

  // header button
  function installButton() {
    var header = document.getElementById("header");
    if (!header) return;
    if (document.getElementById("demo-btn")) return;
    var btn = document.createElement("button");
    btn.textContent = "Demo";
    btn.id = "demo-btn";
    btn.style.cssText =
      "margin-left:16px;padding:4px 14px;background:#0f3460;color:#e6edf3;" +
      "border:1px solid #1f4287;border-radius:4px;cursor:pointer;font:inherit;";
    btn.addEventListener("mouseenter", function () { btn.style.background = "#1f4287"; });
    btn.addEventListener("mouseleave", function () { btn.style.background = "#0f3460"; });
    btn.addEventListener("click", function () {
      // One Demo panel: the button brings back the one open, a running demo
      // and its log with it, rather than a second empty panel on top.
      for (var k in g_demo_panels) {
        var st = g_demo_panels[k];
        if (!/^demo_\d+$/.test(k) || !st) continue;
        if (st.transferred && st.popup && !st.popup.closed) {
          st.popup.focus();
          return;
        }
        if (st.panel) {
          var hs = g_demo_panels[k + "_history"];
          if (hs && hs.panel && !hs.transferred) raisePanel(hs.panel);
          raisePanel(st.panel);
          if (st.showList) st.showList();
          return;
        }
      }
      openDemoPanel();
    });
    header.appendChild(btn);
  }

  // the on-canvas demo panel
  function openDemoPanel() {
    var key = "demo_" + (g_demo_seq++);
    var title = "Demo — script player";

    var state = { panel: null, ws: null, term: null, fit: null,
                  transferred: false, popup: null,
                  sound: true, voice: "", clip: null };

    function close() {
      voiceStop();
      if (hist.tick) clearInterval(hist.tick);
      historyClose();
      try { if (state.ws) state.ws.close(); } catch (e) {}
      try { if (state.term) state.term.dispose(); } catch (e) {}
      try { if (state.popup && !state.popup.closed) state.popup.close(); } catch (e) {}
      try { if (state.panel && state.panel.parentElement)
              state.panel.parentElement.removeChild(state.panel); } catch (e) {}
      delete g_demo_panels[key];
    }

    var shell = makePanelShell(title, close, 560, 460, state);
    var panel = shell.panel, bar = shell.bar;
    state.panel = panel;

    // Register BEFORE wiring so raise/lower/transfer see it.
    // close belongs ON the state, not only in the closure.
    state.close = close;
    g_demo_panels[key] = state;

    // controls row
    var ctrl = document.createElement("div");
    ctrl.style.cssText =
      "display:flex;flex-wrap:wrap;gap:6px;align-items:center;padding:6px 8px;" +
      "background:#0d1117;border-bottom:1px solid #30363d;flex-shrink:0;" +
      "font:12px system-ui,sans-serif;color:#e6edf3;";

    function mkBtn(label, bg) {
      var b = document.createElement("button");
      b.textContent = label;
      b.style.cssText =
        "padding:3px 12px;background:" + bg + ";color:#e6edf3;border:1px solid #1f4287;" +
        "border-radius:4px;cursor:pointer;font:inherit;";
      b.addEventListener("mousedown", function (e) { e.stopPropagation(); });
      return b;
    }
    var playBtn   = mkBtn("Play", "#0f3460");
    var cmdsBtn   = mkBtn("Commands", "#0d3321");
    var clearBtn  = mkBtn("Clear Screen", "#21262d");
    var histBtn   = mkBtn("Comments on", "#21262d");
    var soundBtn  = mkBtn("sound off", "#21262d");
    var stopBtn   = mkBtn("stop and del", "#5a1e1e");

    var stateLbl = document.createElement("span");
    stateLbl.textContent = "idle";
    stateLbl.style.cssText = "margin-left:auto;opacity:.7;";

    ctrl.appendChild(playBtn);
    ctrl.appendChild(cmdsBtn);
    ctrl.appendChild(clearBtn);
    ctrl.appendChild(histBtn);
    ctrl.appendChild(soundBtn);
    ctrl.appendChild(stopBtn);
    ctrl.appendChild(stateLbl);
    panel.appendChild(ctrl);

    // xterm log (fills the whole lower area)
    var termHost = document.createElement("div");
    termHost.style.cssText = "flex:1 1 auto;min-height:0;background:#0d1117;";
    panel.appendChild(termHost);

    var term = new Terminal({
      convertEol: true, cursorBlink: false, fontSize: 13,
      theme: { background: "#0d1117",
               selectionBackground: "#2f6feb",
               selectionForeground: "#ffffff",
               selectionInactiveBackground: "#26456e" }
    });
    var fit = new FitAddon.FitAddon();
    term.loadAddon(fit);
    term.open(termHost);
    try { fit.fit(); } catch (e) {}
    state.term = term; state.fit = fit;
    state.focus = function () { focusTerm(state.term); };
    // A key typed in the log while a demo runs goes to the demo: one text
    // frame per key, which the server hands to the demo's stdin as a line.
    term.onData(function (data) {
      if (state.ws && state.ws.readyState === WebSocket.OPEN)
        state.ws.send(data);
    });
    state.panelRef = state;
    // After re-docking from a tab, refit the xterm to the restored panel
    // size.
    state.onReturn = function () {
      setTimeout(function () { try { fit.fit(); } catch (e) {} }, 50);
    };

    // Refit when the panel is resized (makePanelShell's grip changes height).
    try {
      var ro = new ResizeObserver(function () { try { fit.fit(); } catch (e) {} });
      ro.observe(termHost);
    } catch (e) {}

    // title-bar right-click menu: Move to new tab / Close
    bar.addEventListener("contextmenu", function (e) {
      e.preventDefault(); e.stopPropagation();
      var old = document.getElementById("panel-bar-menu");
      if (old) old.remove();
      var menu = document.createElement("div");
      menu.id = "panel-bar-menu";
      menu.style.cssText =
        "position:fixed;left:" + e.clientX + "px;top:" + e.clientY + "px;" +
        "background:#16213e;border:1px solid #0f3460;border-radius:4px;" +
        "font-family:monospace;font-size:12px;color:#e0e0e0;z-index:9999;" +
        "min-width:180px;box-shadow:0 4px 16px #000c;padding:4px 0;";
      function mitem(label, action) {
        var el = document.createElement("div");
        el.textContent = label;
        el.style.cssText = "padding:7px 18px;cursor:pointer;white-space:nowrap;";
        el.addEventListener("mouseenter", function () { el.style.background = "#0f3460"; });
        el.addEventListener("mouseleave", function () { el.style.background = ""; });
        el.addEventListener("mouseup", function () { menu.remove(); action(); });
        menu.appendChild(el);
      }
      mitem("Move to new tab", function () {
        try { transferPanelToTab(panel, title, g_demo_panels, key); }
        catch (err) { try { fit.fit(); } catch (e) {} }
      });
      mitem("Close panel", function () { close(); });
      document.body.appendChild(menu);
      {
        var M = 6, vw = window.innerWidth, vh = window.innerHeight;
        var r = menu.getBoundingClientRect();
        var left = e.clientX, top = e.clientY;
        if (left + r.width  > vw - M) left = Math.max(M, vw - r.width  - M);
        if (top  + r.height > vh - M) top  = Math.max(M, vh - r.height - M);
        menu.style.left = left + "px"; menu.style.top = top + "px";
      }
      var dismiss = function (ev) {
        if (!menu.contains(ev.target)) {
          menu.remove(); document.removeEventListener("mousedown", dismiss);
        }
      };
      document.addEventListener("mousedown", dismiss);
    });

    // Comments panel: About (the file docstring), then Steps (story text, folded
    // commands, stderr, end), rebuilt from the kept events when reopened.
    // userOff: the reader closed the panel, the next Play leaves it closed
    var hist = { key: key + "_history", state: null, name: "",
                 about: null, events: [], t0: 0, tick: null, ui: null,
                 userOff: false };

    function el(tag, css, text) {
      var e = document.createElement(tag);
      if (css) e.style.cssText = css;
      if (text !== undefined) e.textContent = text;
      return e;
    }

    function fmtClock(sec) {
      sec = Math.max(0, Math.floor(sec));
      var m = Math.floor(sec / 60), s = sec % 60;
      return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
    }

    // The docstring of a demo: its first line is the title, the rest is the
    // network as ascii art, the acts, the image, the packet to catch.
    function parseAbout(script) {
      var m = /^(?:#![^\n]*\n)?\s*[rR]?"""([\s\S]*?)"""/.exec(script || "");
      if (!m) return null;
      var lines = m[1].replace(/\s+$/, "").split("\n");
      return { title: lines[0].trim(), lines: lines.slice(1) };
    }

    function copyText(text, row) {
      try {
        navigator.clipboard.writeText(text).then(function () {
          var old = row.style.background;
          row.style.background = "#1f6f3f";
          setTimeout(function () { row.style.background = old; }, 350);
        });
      } catch (e) {}
    }

    // The command of an ssh line, its quotes off, and what follows them.
    function splitCmd(t) {
      t = t.trim();
      var q = /^(["'])((?:(?!\1)[\s\S])*)\1\s*(.*)$/.exec(t);
      return q ? { cmd: q[2], note: q[3] } : { cmd: t, note: "" };
    }

    // A sniff filter (pcap_filter.c subset) as a webshark display filter; null
    // if untranslatable (portrange): the capture opens unfiltered.
    function sniffToDisplay(text) {
      var tk = (text || "").replace(/([()])/g, " $1 ").trim().split(/\s+/)
                           .filter(function (w) { return w; });
      var pos = 0;
      var PROTO = { ip: "ip", ip6: "ipv6", arp: "arp", tcp: "tcp", udp: "udp",
                    icmp: "icmp", icmp6: "icmpv6", igmp: "igmp" };
      var IPV4 = /^\d{1,3}(\.\d{1,3}){3}(\/\d{1,2})?$/;
      function peek() { return tk[pos]; }
      function eat(w) { if (tk[pos] === w) { pos++; return true; } return false; }
      function fail() { throw new Error("untranslated"); }
      function num() {
        var t = tk[pos++];
        if (!/^\d+$/.test(t || "")) fail();
        return t;
      }
      function prim(proto) {
        var dir = "";
        if (eat("src")) dir = "src"; else if (eat("dst")) dir = "dst";
        var t = peek();
        if (t === "host" || t === "net") {
          pos++;
          var a = tk[pos++];
          if (!IPV4.test(a || "")) fail();
          return "ip." + (dir || "addr") + " == " + a.replace(/\/32$/, "");
        }
        if (t === "port") {
          pos++;
          var n = num(), f = dir ? dir + "port" : "port";
          if (proto === "tcp" || proto === "udp") return proto + "." + f + " == " + n;
          return "(tcp." + f + " == " + n + " || udp." + f + " == " + n + ")";
        }
        if (t === "less" || t === "greater") {
          pos++;
          return "frame.len " + (t === "less" ? "<=" : ">=") + " " + num();
        }
        fail();
      }
      function primary() {
        var t = peek();
        if ((t === "ip" || t === "ip6") && tk[pos + 1] === "proto") {
          pos += 2;
          return (t === "ip" ? "ip.proto == " : "ipv6.nxt == ") + num();
        }
        if (t && PROTO[t]) {
          pos++;
          var n = peek();
          if (["src", "dst", "host", "net", "port", "less", "greater"]
                .indexOf(n) >= 0) {
            var p = prim(t);
            return /^(tcp|udp)\./.test(p) ? p : "(" + PROTO[t] + " && " + p + ")";
          }
          return PROTO[t];
        }
        return prim("");
      }
      function factor() {
        if (eat("not") || eat("!")) return "!" + factor();
        if (eat("(")) {
          var e = expr();
          if (!eat(")")) fail();
          return e;
        }
        return primary();
      }
      function term() {
        var parts = [factor()];
        for (;;) {
          if (eat("and") || eat("&&")) { parts.push(factor()); continue; }
          var t = peek();
          if (t === undefined || t === "or" || t === "||" || t === ")") break;
          parts.push(factor());
        }
        return parts.length > 1 ? "(" + parts.join(" && ") + ")" : parts[0];
      }
      function expr() {
        var parts = [term()];
        while (eat("or") || eat("||")) parts.push(term());
        return parts.length > 1 ? "(" + parts.join(" || ") + ")" : parts[0];
      }
      if (!tk.length) return null;
      try {
        var out = expr();
        if (pos !== tk.length) return null;
        return out.replace(/^\((.*)\)$/, function (all, inner) {
          // the outer brackets only when they close at the end
          var d = 0;
          for (var i = 0; i < inner.length; i++) {
            if (inner[i] === "(") d++;
            else if (inner[i] === ")" && --d < 0) return all;
          }
          return inner;
        });
      } catch (e) { return null; }
    }

    // A cloonix line a web user cannot type: ssh opens the node terminal, sniff
    // the cable capture; null otherwise.
    function webRow(text) {
      var ind = /^\s*/.exec(text)[0];
      var m = /^\s*(?:\$\s+)?cloonix\s+cli\s+\S+\s+ssh\s+(\S+)\s+(.+)$/.exec(text);
      if (m) {
        var node = m[1], sc = splitCmd(m[2]), cmd = sc.cmd;
        var r = el("div", "color:#79c0ff;cursor:pointer;border-radius:3px;",
                   ind + "terminal of " + node + ":  " + cmd +
                   (sc.note ? "      " + sc.note : ""));
        r.title = "click: a terminal of " + node +
                  " opens and runs this command, also put in the clipboard";
        r.addEventListener("click", function () {
          copyText(cmd, r);
          var n = g_nodes.find(function (x) { return x.name === node; });
          if (n && g_net) openSshPanel(node, g_net, n, "ssh", cmd);
        });
        return r;
      }
      m = /^\s*(?:\$\s+)?cloonix\s+cli\s+\S+\s+sniff\s+(\S+)(?:\s+eth(\d+))?(?:\s+(.*))?$/.exec(text);
      if (m) {
        var who = m[1], eth = m[2];
        var filt = (m[3] || "").replace(/^--(?:dump|bytes)\b\s*/, "");
        filt = splitCmd(filt).cmd;
        var what = eth !== undefined ? who + " eth" + eth : "the air " + who;
        var disp = sniffToDisplay(filt);
        var c = el("div", "color:#79c0ff;cursor:pointer;border-radius:3px;",
                   ind + "capture of " + what + (filt ? ",  " + filt : ""));
        c.title = "click: the capture of " + what + " opens" +
                  (disp ? ", filtered on " + disp : "");
        c.addEventListener("click", function () {
          // as the sniff verb does, a capture not armed is armed first
          if (eth !== undefined) {
            var num = parseInt(eth, 10);
            var n = g_nodes.find(function (x) { return x.name === who; });
            if (!n) return;
            if (n.etype && n.etype[num] === "eths")
              openWebsharkPanel(who, num, n, undefined, disp);
            else
              snfSet(who, num, 1, function () {
                websharkWhenSniffable(who, num, disp); });
          } else {
            var l = g_lans.find(function (x) { return x.name === who; });
            if (!l) return;
            if (g_air_snf.has(who)) openAirWebshark(who, l, disp);
            else snfSet(who, 0, 1, function () {
                   websharkWhenSniffable(who, null, disp); });
          }
        });
        return c;
      }
      m = /^\s*(?:\$\s+)?cloonix\s+cli\s+\S+\s+cnf\s+(\S+)\s+eth(\d+)\s+sniff=on\s*$/.exec(text);
      if (m)
        return el("div", "color:#8b949e;", ind + "eth" + m[2] + " of " + m[1] +
                  ": right-click it in the canvas, \"Sniff on\"");
      return null;
    }

    // One line of text, styled by what it is.
    function textRow(text) {
      if (text === "") return el("div", "height:.7em;");
      var web = webRow(text);
      if (web) return web;
      var cmd = /^\s*(?:\$\s+)?(cloonix\s.*)$/.exec(text);
      if (cmd) {
        var c = el("div", "color:#79c0ff;cursor:copy;border-radius:3px;", text);
        c.title = "click to copy this command";
        c.addEventListener("click", function () { copyText(cmd[1], c); });
        return c;
      }
      if (/^\s+\.\s/.test(text)) return el("div", "color:#6e7681;", text);
      if (/^-{3,}\s*$/.test(text.trim())) return el("div", "color:#484f58;", text);
      if (/^[A-Z][A-Z0-9 ,'-]{5,}$/.test(text.trim()))
        return el("div", "color:#e3b341;font-weight:600;", text);
      if (/^\s*ACT\s+\d+/.test(text))
        return el("div", "color:#d2a8ff;", text);
      return el("div", "", text);
    }

    // The button names what a click does: open or close the Comments panel.
    function historyBtnSync() {
      histBtn.textContent = hist.state ? "Comments off" : "Comments on";
    }

    function historyClose() {
      var hs = hist.state;
      hist.state = null; hist.ui = null;
      historyBtnSync();
      canvasHighlight(null);
      if (!hs) return;
      try { if (hs.panel && hs.panel.parentElement)
              hs.panel.parentElement.removeChild(hs.panel); } catch (e) {}
      delete g_demo_panels[hist.key];
    }

    function historyStatus() {
      var ui = hist.ui;
      if (!ui) return;
      var n = ui.sections.length - 1;          // the introduction is not a step
      var last = hist.events.length ? hist.events[hist.events.length - 1] : null;
      if (last && last.k === "end") {
        ui.status.textContent = (last.ok ? "done" : "failed") + " · " +
          n + " step" + (n === 1 ? "" : "s") + " · " + fmtClock(last.at);
        ui.status.style.color = last.ok ? "#3fb950" : "#f85149";
      } else if (hist.t0) {
        ui.status.textContent = "running · step " + n + " · " +
          fmtClock((Date.now() - hist.t0) / 1000);
        ui.status.style.color = "#e3b341";
      } else {
        ui.status.textContent = "";
      }
    }

    function sectionMark(sec, state) {
      var c = { run: "#e3b341", ok: "#3fb950", ko: "#f85149" }[state];
      sec.badge.style.background = c;
      sec.chip.style.borderColor = c;
      sec.chip.style.color = (state === "run") ? "#0d1117" : c;
      sec.chip.style.background = (state === "run") ? c : "transparent";
    }

    function sectionNew(title, when) {
      var sec = sectionMake(title, when);
      sec.title = title;
      historyTitle();
      return sec;
    }

    function sectionMake(title, when) {
      var ui = hist.ui, idx = ui.sections.length;
      var prev = idx ? ui.sections[idx - 1] : null;
      if (prev) sectionMark(prev, "ok");
      var box = el("div", "margin:0 0 14px 0;");
      var head = el("div",
        "display:flex;align-items:baseline;gap:8px;margin-bottom:4px;" +
        "padding-bottom:3px;border-bottom:1px solid #21262d;" +
        "font-family:system-ui,sans-serif;white-space:normal;");
      var badge = el("span",
        "flex-shrink:0;min-width:20px;padding:1px 6px;border-radius:10px;" +
        "color:#0d1117;font-size:11px;font-weight:700;text-align:center;",
        idx ? String(idx) : "i");
      var act = /^(act\s+\d+)\s*:?\s*(.*)$/i.exec(title);
      var ttl = el("span", "flex:1;color:#e6edf3;font-size:13px;font-weight:600;");
      if (act) {
        ttl.appendChild(el("span", "color:#d2a8ff;text-transform:uppercase;" +
                                   "margin-right:8px;", act[1]));
        ttl.appendChild(document.createTextNode(act[2]));
      } else ttl.textContent = title;
      head.appendChild(badge); head.appendChild(ttl);
      if (when) head.appendChild(el("span", "color:#6e7681;font-size:11px;", when));
      var body = el("div", "");
      box.appendChild(head); box.appendChild(body);
      ui.steps.appendChild(box);
      var chip = el("span",
        "cursor:pointer;padding:0 7px;border:1px solid;border-radius:10px;" +
        "font:600 11px system-ui,sans-serif;line-height:17px;",
        idx ? String(idx) : "i");
      chip.title = title;
      var sec = { box: box, body: body, badge: badge, chip: chip,
                  cmds: null, cmdList: null, cmdCount: 0, names: new Set() };
      // the mouse on a step outlines on the canvas the items it acted on
      [chip, head].forEach(function (e) {
        e.addEventListener("mouseenter", function () { canvasHighlight(sec.names); });
        e.addEventListener("mouseleave", function () { canvasHighlight(null); });
      });
      chip.addEventListener("click", function () {
        historyTab("steps");
        ui.follow = false;
        ui.steps.scrollTop = box.offsetTop - ui.steps.offsetTop - 4;
      });
      ui.index.appendChild(chip);
      ui.sections.push(sec);
      sectionMark(sec, "run");
      return sec;
    }

    function sectionCmd(sec, line) {
      if (!sec.cmds) {
        sec.cmds = el("details", "margin-top:6px;color:#8b949e;");
        sec.cmdSum = el("summary", "cursor:pointer;font:11px system-ui,sans-serif;");
        sec.cmdList = el("div", "margin:4px 0 0 10px;");
        sec.cmds.appendChild(sec.cmdSum); sec.cmds.appendChild(sec.cmdList);
        sec.box.appendChild(sec.cmds);
      }
      sec.cmdCount++;
      cmdNames(line).forEach(function (n) { sec.names.add(n); });
      sec.cmdSum.textContent = sec.cmdCount + " cloonix command" +
        (sec.cmdCount === 1 ? "" : "s") + ", as typed in a shell";
      sec.cmdList.appendChild(textRow(line.replace(/^\s+/, "")));
    }

    // Draw one event. Called live, and for every kept event on a reopen.
    function historyDraw(ev) {
      var ui = hist.ui;
      if (!ui) return;
      var cur = ui.sections[ui.sections.length - 1];
      if (ev.k === "story") {
        var m = /^\[\s*(\d+(?:\.\d+)?)s\]\s*(.*)$/.exec(ev.t);
        var n = m ? null : /^(\d+)\.\s+(.*)$/.exec(ev.t);
        if (m)      sectionNew(m[2], fmtClock(parseFloat(m[1])));
        else if (n) sectionNew(n[2], "");
        else        cur.body.appendChild(textRow(ev.t));
      } else if (ev.k === "cmd") {
        sectionCmd(cur, ev.t);
      } else if (ev.k === "note") {
        cur.body.appendChild(el("div",
          "color:" + (ev.bad ? "#ff7b72" : "#e3b341") + ";white-space:pre-wrap;",
          ev.t));
      } else if (ev.k === "subend") {
        // all.py: 0 done; 3 (EXIT_UNAVAILABLE of demolib) cannot run here,
        // anything else a failure: either stops the run
        var out = (ev.rc === 0) ? "ok" : (ev.rc === 3) ? "na" : "ko";
        sectionMark(cur, out === "ok" ? "ok" : "ko");
        ui.steps.appendChild(el("div",
          "margin:4px 0 10px 0;padding:8px 10px;border-radius:4px;" +
          "white-space:normal;font:12px system-ui,sans-serif;border:1px solid " +
          { ok: "#238636;color:#3fb950;", na: "#9e6a03;color:#e3b341;",
            ko: "#da3633;color:#ff7b72;" }[out],
          { ok: "End of demo " + ev.path + ".",
            na: ev.path + " cannot run here, the reason above: the run stops, " +
                "the network stays for inspection.",
            ko: ev.path + " failed, exit " + ev.rc + ", the reason above in " +
                "red: the run stops, the network stays for inspection." }[out]));
        historyTitle();
      } else if (ev.k === "end") {
        sectionMark(cur, ev.ok ? "ok" : "ko");
        var endBox = el("div",
          "margin:4px 0 10px 0;padding:8px 10px;border-radius:4px;" +
          "white-space:normal;font:12px system-ui,sans-serif;" +
          "border:1px solid " + (ev.ok ? "#238636" : "#da3633") + ";" +
          "color:" + (ev.ok ? "#3fb950" : "#ff7b72") + ";",
          ev.ok
            ? "The demo went to its end; its network keeps running. A blue " +
              "line above acts on a click: a terminal line runs in the " +
              "terminal of its node, a capture line opens the capture of its " +
              "cable, a cloonix line is copied for a shell."
            : ev.stopped
            ? "The demo was stopped, everything deleted."
            : "The demo stopped on a failure, the reason in red above and in " +
              "the log of the Demo panel; the machines stay for inspection.");
        ui.steps.appendChild(endBox);
      }
      if (ui.follow) ui.steps.scrollTop = ui.steps.scrollHeight;
      historyStatus();
    }

    function historyTab(which) {
      var ui = hist.ui;
      if (!ui) return;
      ui.steps.style.display = (which === "steps") ? "block" : "none";
      ui.head.style.display = (which === "steps") ? "block" : "none";
      ui.index.style.display = (which === "steps") ? "flex" : "none";
      ui.about.style.display = (which === "about") ? "block" : "none";
      [["steps", ui.tabSteps], ["about", ui.tabAbout]].forEach(function (p) {
        p[1].style.color = (p[0] === which) ? "#e6edf3" : "#8b949e";
        p[1].style.borderBottomColor = (p[0] === which) ? "#f78166" : "transparent";
      });
    }

    // all.py: the demo it plays and its step in the title, "#demo# n/N path
    // line" starting a demo, "#demoend# path rc" ending it.
    function historyTitle() {
      var st = hist.state, ui = hist.ui;
      if (!st || !ui) return;
      var lbl = st.panel.firstElementChild.querySelectorAll("span")[1];
      if (!lbl) return;
      var t = "Comments" + (hist.name ? " — " + hist.name : "");
      if (hist.sub) {
        t += " · " + hist.sub.n + "/" + hist.sub.of + " " + hist.sub.path;
        var n = ui.sections.length - 1;
        if (hist.sub.ended) t += " · end of demo";
        else if (n > 0) t += " · step " + n + ": " + ui.sections[n].title;
      }
      lbl.textContent = t;
    }

    function headDraw() {
      var ui = hist.ui;
      if (!ui) return;
      var a = hist.sub ? hist.subAbout : hist.about;
      var where = hist.sub ? hist.sub.n + "/" + hist.sub.of + " " +
                             hist.sub.path : (hist.name || "demo");
      ui.head.textContent = "";
      ui.head.appendChild(el("div", "font-weight:600;font-size:13px;color:#e6edf3;",
        where));
      var what = a && a.title ? a.title : hist.sub ? hist.sub.line : "";
      if (what)
        ui.head.appendChild(el("div", "margin-top:3px;color:#c9d1d9;", what));
    }

    function historyFill() {
      var ui = hist.ui, a = hist.about;
      ui.steps.textContent = ""; ui.index.textContent = ""; ui.about.textContent = "";
      ui.sections = []; ui.follow = true;
      headDraw();
      if (a) {
        ui.about.appendChild(el("div",
          "font:600 14px system-ui,sans-serif;color:#e6edf3;white-space:normal;" +
          "margin-bottom:8px;", a.title));
        a.lines.forEach(function (t) { ui.about.appendChild(textRow(t)); });
      } else {
        ui.about.appendChild(el("div", "color:#8b949e;white-space:normal;",
          "No docstring in this file."));
      }
      sectionNew("start", "");
      hist.events.forEach(historyDraw);
      historyStatus();
      historyTitle();
    }

    function historyOpen() {
      if (hist.state) { raisePanel(hist.state.panel); return; }
      var W = 660, H = 600;
      var hs = { panel: null, transferred: false, popup: null };
      var shell = makePanelShell("Comments", function () {
        hist.userOff = true;
        historyClose();
      }, W, H, hs);
      hs.panel = shell.panel;
      hs.close = historyClose;
      hist.state = hs;
      historyBtnSync();
      g_demo_panels[hist.key] = hs;
      var wrapW = shell.panel.parentElement.clientWidth;
      var left = panel.offsetLeft + panel.offsetWidth + 10;
      if (left + W > wrapW) left = Math.max(0, wrapW - W);
      shell.panel.style.left = left + "px";
      shell.panel.style.top = panel.offsetTop + "px";

      var grip = shell.panel.lastChild;
      function add(e) { shell.panel.insertBefore(e, grip); return e; }
      var tabs = add(el("div",
        "display:flex;align-items:center;gap:2px;padding:0 8px;flex-shrink:0;" +
        "background:#0d1117;border-bottom:1px solid #30363d;" +
        "font:12px system-ui,sans-serif;"));
      function tab(label, which) {
        var t = el("span", "cursor:pointer;padding:6px 10px;" +
                           "border-bottom:2px solid transparent;", label);
        t.addEventListener("mousedown", function (e) { e.stopPropagation(); });
        t.addEventListener("click", function () { historyTab(which); });
        tabs.appendChild(t);
        return t;
      }
      var ui = { sections: [], follow: true };
      ui.tabSteps = tab("Steps", "steps");
      ui.tabAbout = tab("About", "about");
      ui.tabSteps.title = "the run, step by step, each step with its cloonix " +
                          "commands folded";
      ui.tabAbout.title = "the whole description of the demo";
      var save = el("span", "cursor:pointer;padding:2px 8px;margin-left:8px;" +
                            "border:1px solid #30363d;border-radius:4px;" +
                            "color:#8b949e;font-size:11px;", "Save");
      save.title = "download these comments as a markdown file";
      save.addEventListener("mousedown", function (e) { e.stopPropagation(); });
      save.addEventListener("click", historySave);
      tabs.appendChild(save);
      ui.status = el("span", "margin-left:auto;font-size:11px;");
      tabs.appendChild(ui.status);
      ui.index = add(el("div",
        "display:flex;flex-wrap:wrap;gap:4px;padding:5px 8px;flex-shrink:0;" +
        "border-bottom:1px solid #21262d;background:#0d1117;"));
      // the description of the demo stays at the top, the steps scroll under
      ui.head = add(el("div",
        "flex-shrink:0;max-height:30%;overflow:auto;padding:8px 12px;" +
        "border-bottom:1px solid #30363d;background:#111a2e;" +
        "white-space:normal;font:12px system-ui,sans-serif;color:#c9d1d9;"));
      var pane = "flex:1 1 auto;min-height:0;overflow-y:scroll;padding:10px 12px;" +
                 "white-space:pre;color:#c9d1d9;scrollbar-width:thin;" +
                 "scrollbar-color:#484f58 #0d1117;" +
                 "font:12px 'SF Mono','Fira Code',monospace;line-height:1.4;";
      ui.steps = add(el("div", pane));
      ui.about = add(el("div", pane));
      [ui.steps, ui.about, ui.index, ui.head].forEach(function (p) {
        p.addEventListener("mousedown", function (e) { e.stopPropagation(); });
      });
      // The view follows the run until the reader scrolls up, and again once
      // he is back at the bottom.
      ui.steps.addEventListener("scroll", function () {
        ui.follow = ui.steps.scrollTop + ui.steps.clientHeight >=
                    ui.steps.scrollHeight - 24;
      });
      hist.ui = ui;
      historyFill();
      historyTab("steps");
    }

    // The comments as markdown: About, then each step, its text, its
    // commands and how the demo ended, the worked example to keep.
    function historyMarkdown() {
      var out = [], a = hist.about, cmds = [];
      function flush() {
        if (cmds.length) out.push("", "```sh", cmds.join("\n"), "```");
        cmds = [];
      }
      out.push("# " + (a ? a.title : (hist.name || "demo")), "");
      if (hist.name) out.push("Demo: `" + hist.name + "`", "");
      if (a && a.lines.length) out.push("```", a.lines.join("\n"), "```", "");
      hist.events.forEach(function (ev) {
        if (ev.k === "story") {
          var m = /^\[\s*(\d+(?:\.\d+)?)s\]\s*(.*)$/.exec(ev.t);
          var n = m ? null : /^(\d+)\.\s+(.*)$/.exec(ev.t);
          if (m || n) {
            flush();
            out.push("", "## " + (m ? m[2] + "  (" + fmtClock(parseFloat(m[1])) +
                                      ")" : n[2]), "");
          } else
            out.push(ev.t.replace(/\s+$/, "") ? "    " + ev.t : "");
        } else if (ev.k === "cmd") {
          cmds.push(ev.t.replace(/^\s+/, ""));
        } else if (ev.k === "note") {
          out.push("> " + ev.t);
        } else if (ev.k === "end") {
          flush();
          out.push("", "**" + (ev.ok ? "done" : ev.stopped ? "stopped"
                                              : "failed") +
                   "** after " + fmtClock(ev.at));
        }
      });
      flush();
      return out.join("\n") + "\n";
    }

    function historySave() {
      var blob = new Blob([historyMarkdown()], { type: "text/markdown" });
      var a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = (hist.name || "demo").replace(/\.py$/, "").replace(/\//g, "_") +
                   "_comments.md";
      document.body.appendChild(a);
      a.click();
      setTimeout(function () { URL.revokeObjectURL(a.href); a.remove(); }, 0);
    }

    function historyStart(name, script) {
      hist.name = name || "";
      hist.sub = null;
      hist.about = parseAbout(script);
      hist.events = [];
      hist.stopped = false;
      hist.t0 = Date.now();
      if (hist.tick) clearInterval(hist.tick);
      hist.tick = setInterval(historyStatus, 1000);
      if (hist.state) historyFill(); else if (!hist.userOff) historyOpen();
      historyTab("steps");
    }

    function historyEvent(ev) {
      hist.events.push(ev);
      historyDraw(ev);
    }

    function historyEnd(ok) {
      if (!hist.t0) return;
      // all.py said the failure at the end of its demo already
      if (!ok && !hist.stopped && !hist.sub) voiceFailure(hist.name);
      if (hist.tick) { clearInterval(hist.tick); hist.tick = null; }
      historyEvent({ k: "end", ok: ok, stopped: hist.stopped,
                     at: (Date.now() - hist.t0) / 1000 });
      hist.t0 = 0;
      historyStatus();
    }

    // The voice: the phrase of demos/voice for the demo started, made into
    // audio/<area>/<demo>.ogg by debug/demo_voice.py.
    function voicePlay() {
      voiceStop();
      if (!state.sound || !state.voice) return;
      var a = new Audio(state.voice);
      state.clip = a;
      a.addEventListener("error", function () {
        historyEvent({ k: "note", bad: true, t: "no voice clip " + state.voice });
      });
      a.play().catch(function (e) {
        if (e.name !== "AbortError" && !a.error)
          historyEvent({ k: "note", bad: true,
                         t: "the browser refused the voice (" + e.name + ")" });
      });
    }

    function voiceStop() {
      if (state.clip) { state.clip.pause(); state.clip = null; }
      try { if (window.speechSynthesis) window.speechSynthesis.cancel(); }
      catch (e) {}
    }

    // Why the demo failed, said: the recorded clip, then the first line of
    // the reason by the voice of the browser when it has an english one.
    function failureReason() {
      var notes = hist.events.filter(function (e) { return e.k === "note"; });
      var bad = notes.filter(function (e) { return e.bad; });
      var first = (bad[0] || notes[0] || { t: "" }).t;
      return first.split("\n")[0].replace(/[_*#]/g, " ").trim().slice(0, 240);
    }

    function voiceFailure(demo) {
      voiceStop();
      if (!state.sound) return;
      var reason = failureReason();
      var a = new Audio("/audio/demo_failed.ogg");
      state.clip = a;
      a.addEventListener("ended", function () {
        var ss = window.speechSynthesis;
        if (!ss || !reason || state.clip !== a) return;
        var vs = ss.getVoices().filter(function (v) { return /^en/i.test(v.lang); });
        if (!vs.length) return;
        var u = new SpeechSynthesisUtterance(
          (demo ? demo.replace(/\.py$/, "").replace(/[\/_]/g, " ") + ": " : "") +
          reason);
        u.voice = vs.filter(function (v) { return /en-GB/i.test(v.lang); })[0] || vs[0];
        u.rate = 0.95;
        ss.speak(u);
      });
      a.play().catch(function () {});
    }

    // One play frame: story, "cloonix ..."/"tar ..." command (log and fold),
    // server verdict, else demo stderr or a key request.
    function playLine(s) {
      if (s.indexOf(STORY_MARK) === 0) {
        var st = s.slice(STORY_MARK.length).replace(/\r?\n$/, "");
        var dm = /^#demo# (\d+)\/(\d+) (\S+)\s*(.*)$/.exec(st);
        var de = /^#demoend# (\S+) (-?\d+)$/.exec(st);
        if (dm) {
          // a demo of all.py: its own steps from 1, its own Save
          hist.sub = { n: dm[1], of: dm[2], path: dm[3], line: dm[4],
                       ended: false };
          hist.subAbout = null;
          hist.events = [];
          if (hist.ui) historyFill();
          (function (sub) {
            var cut = sub.path.lastIndexOf("/");
            fetch("/files/get?name=" + encodeURIComponent(sub.path.slice(cut + 1)) +
                  "&dir=" + encodeURIComponent(cut >= 0 ? "demos/" +
                                               sub.path.slice(0, cut) : "demos"),
                  { cache: "no-store" })
              .then(function (r) { return r.ok ? r.text() : null; })
              .then(function (txt) {
                if (txt == null || hist.sub !== sub) return;
                hist.subAbout = parseAbout(txt);
                headDraw();
              })
              .catch(function () {});
          })(hist.sub);
          state.voice = "/audio/" + dm[3].replace(/\.py$/, "") + ".ogg";
          voicePlay();
        } else if (de && hist.sub) {
          hist.sub.ended = true;
          historyEvent({ k: "subend", path: de[1], rc: parseInt(de[2], 10) });
          var rc = parseInt(de[2], 10);
          if (rc !== 0) voiceFailure(de[1]);
        } else
          historyEvent({ k: "story", t: st });
        return;
      }
      writeLine(s);
      var t = s.replace(/\r?\n$/, "");
      if (/^(?:cloonix|tar) /.test(t))   historyEvent({ k: "cmd", t: t });
      else if (t.indexOf("[KO stopped") === 0) hist.stopped = true;
      else if (t.indexOf("[DONE") === 0) historyEnd(/ok=1 ko=0/.test(t));
      else if (t.indexOf("[KO") === 0)   historyEvent({ k: "note", bad: true, t: t });
      else if (t.indexOf(">>> ") === 0 || !t.trim()) return;
      else historyEvent({ k: "note", bad: /Traceback|Error|failed|could not|did not/i.test(t),
                          t: t.replace(/press Enter here/, "press Enter in the log of the Demo panel") });
    }

    // colourised log writer
    function writeLine(s) {
      if (s.indexOf(">>> ") === 0)        term.write("\x1b[36m" + s + "\x1b[0m\r\n"); // cyan: echoed cmd
      else if (s === "[OK]")              term.write("\x1b[32m" + s + "\x1b[0m\r\n"); // green
      else if (s.indexOf("[KO") === 0)    term.write("\x1b[31m" + s + "\x1b[0m\r\n"); // red
      else if (s.indexOf("[ABORT") === 0) term.write("\x1b[33m" + s + "\x1b[0m\r\n"); // yellow
      else if (s.indexOf("[DONE") === 0)  term.write("\x1b[1m"  + s + "\x1b[0m\r\n"); // bold
      else if (s.indexOf("[wait") === 0)  term.write("\x1b[35m" + s + "\x1b[0m\r\n"); // magenta
      else if (s.indexOf("[mode") === 0)  term.write("\x1b[90m" + s + "\x1b[0m\r\n"); // grey
      else                                term.write(s.replace(/\n/g, "\r\n"));
    }

    function runScript(script, dir, name) {
      if (!script || !script.trim()) { writeLine("\x1b[33m(nothing to play)\x1b[0m"); return; }
      // Positions of an earlier run that never got their item are dropped;
      // the fit is asked again, at the bottom of the view (g_demo_view).
      g_saved_positions = {};
      g_demo_view = true;
      g_has_fitted = false;
      g_fit_anim = null;
      // Default is stop-on-error (the server aborts on the first failure).

      historyStart(name, script);
      var area = (dir || "").replace(/^demos\/?/, "");
      // all.py has no phrase of its own: each demo it plays says its own
      state.voice = (!name || /^PLAYS_THE_PACKAGES\s*=\s*True\b/m.test(script))
                    ? "" : "/audio/" + (area ? area + "/" : "") +
                           name.replace(/\.py$/, "") + ".ogg";
      voicePlay();
      playBtn.disabled = true;
      stateLbl.textContent = "playing…";
      term.write("\x1b[90m── connecting ──\x1b[0m\r\n");

      var proto = (location.protocol === "https:") ? "wss://" : "ws://";
      var ws;
      try { ws = new WebSocket(proto + location.host + "/play"); }
      catch (e) { writeLine("\x1b[31m[ws open failed]\x1b[0m"); playBtn.disabled = false; return; }
      state.ws = ws;
      var hdr = "";
      if (name) hdr = "#!cloonix-play-dir " + (dir || "") + "\n" +
                      "#!cloonix-play-name " + name + "\n";
      ws.onopen    = function () { ws.send(hdr + script); };
      ws.onmessage = function (ev) { playLine(ev.data); };
      ws.onerror   = function () { writeLine("\x1b[31m[ws error]\x1b[0m"); };
      ws.onclose   = function () {
        g_demo_view = false;
        historyEnd(false);               // no verdict came: the link was lost
        playBtn.disabled = false;
        stateLbl.textContent = "idle";
        state.ws = null;
        term.write("\x1b[90m── disconnected ──\x1b[0m\r\n");
      };
    }

    // The picker shows demos/packages: the packages of the chapter "Learning
    // with cloonix", in its order, each demo with its line of that chapter.
    function browseOpen(opts) {
      var overlay = document.createElement("div");
      overlay.style.cssText =
        "position:fixed;inset:0;background:#000a;z-index:10000;" +
        "display:flex;align-items:center;justify-content:center;";
      overlay.addEventListener("mousedown", function (e) { e.stopPropagation(); });

      var box = document.createElement("div");
      box.style.cssText =
        "min-width:380px;max-width:80vw;background:#16213e;border:1px solid #0f3460;" +
        "border-radius:6px;box-shadow:0 8px 32px #000c;color:#e6edf3;" +
        "font:13px system-ui,sans-serif;padding:14px;display:flex;" +
        "flex-direction:column;gap:10px;";
      overlay.appendChild(box);

      var titleEl = document.createElement("div");
      titleEl.textContent = opts.view ? "Demos -- one is playing, the list only"
                                      : "Open demo";
      titleEl.style.cssText = "font-weight:600;";
      box.appendChild(titleEl);

      var listHost = document.createElement("div");
      listHost.style.cssText =
        "max-height:70vh;overflow:auto;border:1px solid #30363d;border-radius:4px;" +
        "background:#0d1117;min-height:40px;";
      box.appendChild(listHost);

      var btnRow = document.createElement("div");
      btnRow.style.cssText = "display:flex;gap:8px;justify-content:flex-end;";
      var cancelB = mkBtn(opts.view ? "Close" : "Cancel", "#21262d");
      btnRow.appendChild(cancelB);
      box.appendChild(btnRow);

      function destroy() {
        if (overlay.parentElement) overlay.parentElement.removeChild(overlay);
      }
      cancelB.addEventListener("click", destroy);

      function note(text, color) {
        var n = document.createElement("div");
        n.textContent = text;
        n.style.cssText = "padding:10px;color:" + color + ";";
        listHost.appendChild(n);
      }

      function packageRow(num, title) {
        var h = document.createElement("div");
        h.textContent = "Package " + num + " -- " + title;
        h.style.cssText =
          "padding:8px 10px 5px;font-weight:600;color:#7fb0ff;" +
          "border-bottom:1px solid #30363d;background:#111a2e;" +
          "position:sticky;top:0;";
        listHost.appendChild(h);
      }

      function demoRow(path, text) {
        var cut = path.lastIndexOf("/");
        var dir = cut >= 0 ? "demos/" + path.slice(0, cut) : "demos";
        var name = path.slice(cut + 1);
        var r = document.createElement("div");
        r.style.cssText =
          "padding:6px 10px 6px 22px;display:flex;gap:14px;" +
          "border-bottom:1px solid #1a2233;" +
          (opts.view ? "" : "cursor:pointer;");
        var p = document.createElement("span");
        p.textContent = path;
        p.style.cssText = "font-family:monospace;flex:0 0 200px;";
        var t = document.createElement("span");
        t.textContent = text;
        t.style.cssText = "opacity:.75;";
        r.appendChild(p); r.appendChild(t);
        if (!opts.view) {
          r.addEventListener("mouseenter", function () { r.style.background = "#0f3460"; });
          r.addEventListener("mouseleave", function () { r.style.background = ""; });
          r.addEventListener("click", function () { destroy(); opts.onPick(name, dir); });
        }
        listHost.appendChild(r);
      }

      fetch("/files/get?name=packages&dir=demos", { cache: "no-store" })
        .then(function (r) { return r.ok ? r.text() : null; })
        .then(function (txt) {
          var nb = 0;
          if (txt == null) { note("(demos/packages is missing on the server)", "#f66"); return; }
          txt.split(/\r?\n/).forEach(function (ln) {
            var m;
            if (!ln.trim() || ln.charAt(0) === "#") return;
            if ((m = /^(\d+)\s+(.+)$/.exec(ln))) { packageRow(m[1], m[2].trim()); return; }
            if ((m = /^(\S+\.py)\s*(.*)$/.exec(ln))) { demoRow(m[1], m[2].trim()); nb++; }
          });
          if (!nb) note("(no demo in demos/packages)", "#f66");
        })
        .catch(function () { note("(could not read demos/packages)", "#f66"); });

      document.body.appendChild(overlay);
    }

    function pickAndPlay() {
      browseOpen({
        onPick: function (name, dir) {
          var q = "name=" + encodeURIComponent(name) +
                  "&dir=" + encodeURIComponent(dir || "");
          fetch("/files/get?" + q, { cache: "no-store" })
            .then(function (r) { return r.ok ? r.text() : null; })
            .then(function (txt) {
              if (txt == null) { writeLine("\x1b[31m[could not load " + name + "]\x1b[0m"); return; }
              runScript(txt, dir, name);
            })
            .catch(function () { writeLine("\x1b[31m[could not load " + name + "]\x1b[0m"); });
        }
      });
    }
    playBtn.addEventListener("click", pickAndPlay);
    // The header button on an open panel: its list, only to read while a
    // demo plays.
    state.showList = function () {
      if (playBtn.disabled) browseOpen({ view: true });
      else pickAndPlay();
    };

    clearBtn.addEventListener("click", function () { term.clear(); });
    histBtn.addEventListener("click", function () {
      hist.userOff = !!hist.state;
      if (hist.state) historyClose(); else historyOpen();
    });
    // Server kills the running demos and erases the net; the panel stays
    // (header Delete all also closes the panels).
    stopBtn.addEventListener("click", function () {
      voiceStop();
      fetch("/ctl/rma", { method: "POST", cache: "no-store" })
        .then(function (r) {
          if (!r.ok) writeLine("\x1b[31m[stop and del: the server answered " +
                               r.status + "]\x1b[0m");
        })
        .catch(function (e) {
          writeLine("\x1b[31m[stop and del failed: " + e + "]\x1b[0m");
        });
    });
    soundBtn.addEventListener("click", function () {
      state.sound = !state.sound;
      soundBtn.textContent = state.sound ? "sound off" : "sound on";
      if (state.sound) voicePlay(); else voiceStop();
    });

    // The commands that rebuild the net as it is, from the server: dump commands.
    cmdsBtn.addEventListener("click", function () {
      fetch("/ctl/commands", { cache: "no-store" })
        .then(function (r) {
          return r.text().then(function (t) { return { ok: r.ok, t: t }; });
        })
        .then(function (a) {
          if (!a.ok) {
            writeLine("\x1b[31m[commands: " + a.t.trim() + "]\x1b[0m");
            return;
          }
          var lines = a.t.split("\n").filter(function (l) { return l.trim(); });
          term.write("\x1b[32m── the commands of the net as it is ──\x1b[0m\r\n");
          if (!lines.length) writeLine("\x1b[33m(the net is empty)\x1b[0m");
          lines.forEach(function (l) {
            term.write("\x1b[36m" + l + "\x1b[0m\r\n");
          });
        })
        .catch(function (e) {
          writeLine("\x1b[31m[commands failed: " + e + "]\x1b[0m");
        });
    });

    raisePanel(panel);
  }

  // expose for menu wiring / testing
  window.openDemoPanel = openDemoPanel;

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", installButton);
  else
    installButton();
})();
