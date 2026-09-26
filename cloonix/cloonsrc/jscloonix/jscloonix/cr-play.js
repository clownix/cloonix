// cr-play.js — on-canvas "Demo / script player" panel, and its History popup.
// The log of the panel carries the cloonix commands of the demo; the lines a
// demo marks "#story# " are its history, the description of its steps, and
// go to the popup instead.

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
    btn.addEventListener("click", function () { openDemoPanel(); });
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
    var histBtn   = mkBtn("History", "#21262d");
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

    // History popup. The log above carries the cloonix commands; this popup
    // explains the demo: what it is about (About, the docstring of the file),
    // then the run step by step (Steps): the text the demo marks "#story# ",
    // the commands each step ran, folded, what the demo said on stderr, and
    // how it ended. Everything received is kept as a list of events, so the
    // popup closed and opened again is rebuilt from it.
    var hist = { key: key + "_history", state: null, name: "",
                 about: null, events: [], t0: 0, tick: null, ui: null };

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

    // One line of text, styled by what it is.
    function textRow(text) {
      if (text === "") return el("div", "height:.7em;");
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

    function historyClose() {
      var hs = hist.state;
      hist.state = null; hist.ui = null;
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
      chip.addEventListener("click", function () {
        historyTab("steps");
        ui.follow = false;
        ui.steps.scrollTop = box.offsetTop - ui.steps.offsetTop - 4;
      });
      ui.index.appendChild(chip);
      var sec = { box: box, body: body, badge: badge, chip: chip,
                  cmds: null, cmdList: null, cmdCount: 0 };
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
      sec.cmdSum.textContent = sec.cmdCount + " cloonix command" +
        (sec.cmdCount === 1 ? "" : "s") + " run for this, as typed in a shell";
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
      } else if (ev.k === "end") {
        sectionMark(cur, ev.ok ? "ok" : "ko");
        var endBox = el("div",
          "margin:4px 0 10px 0;padding:8px 10px;border-radius:4px;" +
          "white-space:normal;font:12px system-ui,sans-serif;" +
          "border:1px solid " + (ev.ok ? "#238636" : "#da3633") + ";" +
          "color:" + (ev.ok ? "#3fb950" : "#ff7b72") + ";",
          ev.ok
            ? "The demo went to its end. The network is left running: the " +
              "commands in blue above can be clicked, copied, and tried in a shell."
            : ev.stopped
            ? "The demo was stopped and everything was deleted."
            : "The demo stopped on a failure. What it said is in red above " +
              "and in the log of the Demo panel; the machines are left as " +
              "they were for inspection.");
        ui.steps.appendChild(endBox);
      }
      if (ui.follow) ui.steps.scrollTop = ui.steps.scrollHeight;
      historyStatus();
    }

    function historyTab(which) {
      var ui = hist.ui;
      if (!ui) return;
      ui.steps.style.display = (which === "steps") ? "block" : "none";
      ui.index.style.display = (which === "steps") ? "flex" : "none";
      ui.about.style.display = (which === "about") ? "block" : "none";
      [["steps", ui.tabSteps], ["about", ui.tabAbout]].forEach(function (p) {
        p[1].style.color = (p[0] === which) ? "#e6edf3" : "#8b949e";
        p[1].style.borderBottomColor = (p[0] === which) ? "#f78166" : "transparent";
      });
    }

    function historyFill() {
      var ui = hist.ui, a = hist.about;
      ui.steps.textContent = ""; ui.index.textContent = ""; ui.about.textContent = "";
      ui.sections = []; ui.follow = true;
      var lbl = hist.state.panel.firstElementChild.querySelectorAll("span")[1];
      if (lbl) lbl.textContent = "History" + (hist.name ? " — " + hist.name : "");
      if (a) {
        ui.about.appendChild(el("div",
          "font:600 14px system-ui,sans-serif;color:#e6edf3;white-space:normal;" +
          "margin-bottom:8px;", a.title));
        a.lines.forEach(function (t) { ui.about.appendChild(textRow(t)); });
      } else {
        ui.about.appendChild(el("div", "color:#8b949e;white-space:normal;",
          "This file has no docstring: nothing is known about it before it runs."));
      }
      var intro = sectionNew(a ? a.title : (hist.name || "demo"), "");
      intro.body.appendChild(el("div",
        "color:#8b949e;white-space:normal;font:12px system-ui,sans-serif;" +
        "margin-bottom:6px;",
        "About holds the network, the acts and the packet to catch. Here the " +
        "run is told step by step; each step folds the cloonix commands it " +
        "ran, the same lines the log of the Demo panel shows."));
      hist.events.forEach(historyDraw);
      historyStatus();
    }

    function historyOpen() {
      if (hist.state) { raisePanel(hist.state.panel); return; }
      var W = 660, H = 520;
      var hs = { panel: null, transferred: false, popup: null };
      var shell = makePanelShell("History", historyClose, W, H, hs);
      hs.panel = shell.panel;
      hs.close = historyClose;
      hist.state = hs;
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
      ui.status = el("span", "margin-left:auto;font-size:11px;");
      tabs.appendChild(ui.status);
      ui.index = add(el("div",
        "display:flex;flex-wrap:wrap;gap:4px;padding:5px 8px;flex-shrink:0;" +
        "border-bottom:1px solid #21262d;background:#0d1117;"));
      var pane = "flex:1 1 auto;min-height:0;overflow:auto;padding:10px 12px;" +
                 "white-space:pre;color:#c9d1d9;" +
                 "font:12px 'SF Mono','Fira Code',monospace;line-height:1.4;";
      ui.steps = add(el("div", pane));
      ui.about = add(el("div", pane));
      [ui.steps, ui.about, ui.index].forEach(function (p) {
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

    function historyStart(name, script) {
      hist.name = name || "";
      hist.about = parseAbout(script);
      hist.events = [];
      hist.stopped = false;
      hist.t0 = Date.now();
      if (hist.tick) clearInterval(hist.tick);
      hist.tick = setInterval(historyStatus, 1000);
      if (hist.state) historyFill(); else historyOpen();
      historyTab("steps");
    }

    function historyEvent(ev) {
      hist.events.push(ev);
      historyDraw(ev);
    }

    function historyEnd(ok) {
      if (!hist.t0) return;
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
    }

    // One frame of the play websocket: marked, the history; "cloonix ..." or
    // "tar ...", a
    // command, for the log and for the fold of the current step; a verdict
    // of the server; anything else is the demo on stderr or asking for a key.
    function playLine(s) {
      if (s.indexOf(STORY_MARK) === 0) {
        historyEvent({ k: "story",
                       t: s.slice(STORY_MARK.length).replace(/\r?\n$/, "") });
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
      state.voice = name ? "/audio/" + (area ? area + "/" : "") +
                           name.replace(/\.py$/, "") + ".ogg" : "";
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
      titleEl.textContent = "Open demo";
      titleEl.style.cssText = "font-weight:600;";
      box.appendChild(titleEl);

      var listHost = document.createElement("div");
      listHost.style.cssText =
        "max-height:70vh;overflow:auto;border:1px solid #30363d;border-radius:4px;" +
        "background:#0d1117;min-height:40px;";
      box.appendChild(listHost);

      var btnRow = document.createElement("div");
      btnRow.style.cssText = "display:flex;gap:8px;justify-content:flex-end;";
      var cancelB = mkBtn("Cancel", "#21262d");
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
          "padding:6px 10px 6px 22px;cursor:pointer;display:flex;gap:14px;" +
          "border-bottom:1px solid #1a2233;";
        var p = document.createElement("span");
        p.textContent = path;
        p.style.cssText = "font-family:monospace;flex:0 0 200px;";
        var t = document.createElement("span");
        t.textContent = text;
        t.style.cssText = "opacity:.75;";
        r.appendChild(p); r.appendChild(t);
        r.addEventListener("mouseenter", function () { r.style.background = "#0f3460"; });
        r.addEventListener("mouseleave", function () { r.style.background = ""; });
        r.addEventListener("click", function () { destroy(); opts.onPick(name, dir); });
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

    playBtn.addEventListener("click", function () {
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
    });

    clearBtn.addEventListener("click", function () { term.clear(); });
    histBtn.addEventListener("click", historyOpen);
    // Every demo going on is killed by the server, then the net is erased;
    // the panel stays for the next demo. Delete all of the header does the
    // same and closes the panels.
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
