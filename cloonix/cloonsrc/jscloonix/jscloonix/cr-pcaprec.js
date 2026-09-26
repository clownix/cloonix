// cr-pcaprec.js — on-canvas "PcapRec" main-record panel.

(function () {
  "use strict";

  let g_rec_seq = 1;   // monotonic id so multiple panels could coexist

  // Sniffable: a kvm/cnt eth of type "eths" (not vhost).
  function collectSniffable() {
    var nodes = (typeof g_nodes !== "undefined" && g_nodes) ? g_nodes : [];
    var out = [];
    nodes.forEach(function (nd) {
      if (nd.type !== "kvm" && nd.type !== "cnt") return;
      var n = nd.nb_eth || 0;
      for (var i = 0; i < n; i++) {
        var et = (nd.etype && nd.etype[i]) ? nd.etype[i] : "eths";
        if (et === "ethv") continue;            // vhost-only — no sniff
        if (et === "wlan") continue;            // the wifi air is one capture
        out.push({ name: nd.name, num: i });
      }
    });
    return out;
  }

  // header button
  function installButton() {
    var header = document.getElementById("header");
    if (!header) return;
    if (document.getElementById("pcaprec-btn")) return;
    var btn = document.createElement("button");
    btn.textContent = "PcapRec";
    btn.id = "pcaprec-btn";
    btn.style.cssText =
      "margin-left:8px;padding:4px 14px;background:#0f3460;color:#e6edf3;" +
      "border:1px solid #1f4287;border-radius:4px;cursor:pointer;font:inherit;";
    btn.addEventListener("mouseenter", function () { btn.style.background = "#1f4287"; });
    btn.addEventListener("mouseleave", function () { btn.style.background = "#0f3460"; });
    btn.addEventListener("click", function () { openPcaprecPanel(); });
    header.appendChild(btn);
  }

  // the on-canvas record panel
  function openPcaprecPanel() {
    var key = "pcaprec_" + (g_rec_seq++);
    var title = "PcapRec — main record";

    // Remembered browse directory, relative to the browse root, which is the
    // share directory /usr/share/cloonix.
    var g_browse_dir = "pcap";

    var state = { panel: null, ws: null, transferred: false, popup: null };

    function close() {
      try { if (state.ws) state.ws.close(); } catch (e) {}
      try { if (state.popup && !state.popup.closed) state.popup.close(); } catch (e) {}
      try { if (state.panel && state.panel.parentElement)
              state.panel.parentElement.removeChild(state.panel); } catch (e) {}
      delete g_demo_panels[key];
    }

    var shell = makePanelShell(title, close, 460, 460, state);
    var panel = shell.panel, bar = shell.bar;
    state.panel = panel;
    state.panelRef = state;

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
    var recBtn  = mkBtn("Record", "#0d3321");
    var stopBtn = mkBtn("StopRecord", "#5a1e1e");
    stopBtn.disabled = true;

    var stateLbl = document.createElement("span");
    stateLbl.textContent = "idle";
    stateLbl.style.cssText = "margin-left:auto;opacity:.7;";

    ctrl.appendChild(recBtn);
    ctrl.appendChild(stopBtn);
    ctrl.appendChild(stateLbl);
    panel.appendChild(ctrl);

    // directory line
    var dirLine = document.createElement("div");
    dirLine.style.cssText =
      "padding:5px 10px;background:#0d1117;border-bottom:1px solid #1a2233;" +
      "font:11px monospace;color:#7fb0ff;flex-shrink:0;white-space:nowrap;" +
      "overflow:hidden;text-overflow:ellipsis;";
    dirLine.textContent = "dir: /usr/share/cloonix/" + g_browse_dir;
    panel.appendChild(dirLine);

    // interface list (lower part)
    var listHost = document.createElement("div");
    listHost.style.cssText =
      "flex:1 1 auto;min-height:0;overflow:auto;background:#0d1117;" +
      "font:12px monospace;color:#e6edf3;padding:4px 0;";
    panel.appendChild(listHost);

    var emptyMsg = document.createElement("div");
    emptyMsg.style.cssText = "padding:10px;opacity:.6;";
    emptyMsg.textContent = "(press Record to start capturing all interfaces)";
    listHost.appendChild(emptyMsg);

    // rows: key "<name>_<num>" -> { row, countEl, pkts }
    var rows = {};

    function rowKey(name, num) { return name + "_" + num; }

    function buildRows(ifaces) {
      listHost.textContent = "";
      rows = {};
      if (!ifaces.length) {
        var none = document.createElement("div");
        none.style.cssText = "padding:10px;color:#f66;";
        none.textContent = "(no sniffable interfaces in the current topology)";
        listHost.appendChild(none);
        return;
      }
      ifaces.forEach(function (it) {
        var row = document.createElement("div");
        row.style.cssText =
          "display:flex;align-items:center;gap:8px;padding:5px 12px;" +
          "border-bottom:1px solid #1a2233;";
        var nameEl = document.createElement("span");
        nameEl.textContent = it.name + "_eth" + it.num;
        nameEl.style.cssText = "flex:1;color:#e6edf3;";
        var pcapEl = document.createElement("span");
        // The instance number is only known from the server's "started" reply.
        pcapEl.textContent = it.file ? it.file
                                     : (it.name + "_eth" + it.num + "_?.pcap");
        pcapEl.style.cssText = "color:#6e7681;font-size:11px;";
        var countEl = document.createElement("span");
        countEl.textContent = "0";
        countEl.style.cssText =
          "min-width:64px;text-align:right;color:#3fb950;font-weight:600;";
        row.appendChild(nameEl);
        row.appendChild(pcapEl);
        row.appendChild(countEl);
        listHost.appendChild(row);
        rows[rowKey(it.name, it.num)] = { row: row, countEl: countEl, pkts: 0 };
      });
    }

    function setCount(name, num, pkts) {
      var r = rows[rowKey(name, num)];
      if (!r) return;
      r.pkts = pkts;
      r.countEl.textContent = "" + pkts;
    }

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
        catch (err) {}
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

    function browseDir(onPick) {
      var curDir = g_browse_dir;

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
      titleEl.textContent = "Choose record directory";
      titleEl.style.cssText = "font-weight:600;";
      box.appendChild(titleEl);

      var crumbs = document.createElement("div");
      crumbs.style.cssText =
        "font-family:monospace;font-size:12px;opacity:.85;display:flex;" +
        "flex-wrap:wrap;align-items:center;gap:2px;";
      box.appendChild(crumbs);

      var listBox = document.createElement("div");
      listBox.style.cssText =
        "max-height:46vh;overflow:auto;border:1px solid #30363d;border-radius:4px;" +
        "background:#0d1117;min-height:40px;";
      box.appendChild(listBox);

      var btnRow = document.createElement("div");
      btnRow.style.cssText = "display:flex;gap:8px;justify-content:flex-end;";
      var cancelB = mkBtn("Cancel", "#21262d");
      var okB = mkBtn("Use this directory", "#0d3321");
      btnRow.appendChild(cancelB);
      btnRow.appendChild(okB);
      box.appendChild(btnRow);

      function destroy() {
        if (overlay.parentElement) overlay.parentElement.removeChild(overlay);
      }
      cancelB.addEventListener("click", destroy);
      okB.addEventListener("click", function () {
        g_browse_dir = curDir;
        destroy();
        onPick(curDir);
      });

      function row(label, kind, onClick) {
        var el = document.createElement("div");
        el.textContent = (kind === "dir" ? "📁 " : kind === "up" ? "⬆ " : "📄 ") + label;
        el.style.cssText =
          "padding:7px 10px;border-bottom:1px solid #1a2233;font-family:monospace;" +
          (kind === "file" ? "opacity:.5;" : "cursor:pointer;");
        if (kind !== "file") {
          el.addEventListener("mouseenter", function () { el.style.background = "#0f3460"; });
          el.addEventListener("mouseleave", function () { el.style.background = ""; });
          el.addEventListener("click", onClick);
        }
        return el;
      }

      function renderCrumbs() {
        crumbs.textContent = "";
        var parts = curDir ? curDir.split("/") : [];
        function crumb(label, target) {
          var c = document.createElement("span");
          c.textContent = label;
          c.style.cssText = "cursor:pointer;color:#7fb0ff;";
          c.addEventListener("click", function () { load(target); });
          crumbs.appendChild(c);
        }
        crumb("/usr/share/cloonix", "");
        var acc = "";
        parts.forEach(function (p) {
          var sep = document.createElement("span");
          sep.textContent = " / "; sep.style.opacity = ".5";
          crumbs.appendChild(sep);
          acc = acc ? acc + "/" + p : p;
          crumb(p, acc);
        });
      }

      function load(dir) {
        curDir = dir || "";
        renderCrumbs();
        listBox.textContent = "";
        fetch("/files/list?dir=" + encodeURIComponent(curDir), { cache: "no-store" })
          .then(function (r) { return r.ok ? r.json() : { dirs: [], files: [] }; })
          .then(function (j) {
            var dirs  = (j && j.dirs)  ? j.dirs  : [];
            if (curDir) {
              var up = curDir.indexOf("/") >= 0
                ? curDir.slice(0, curDir.lastIndexOf("/")) : "";
              listBox.appendChild(row("..", "up", function () { load(up); }));
            }
            dirs.forEach(function (d) {
              listBox.appendChild(row(d, "dir", function () {
                load(curDir ? curDir + "/" + d : d);
              }));
            });
            // A directory chooser: files are not listed.
            if (!dirs.length) {
              var empty = document.createElement("div");
              empty.textContent = "(no sub-folders — Use this directory to record here)";
              empty.style.cssText = "padding:10px;opacity:.6;";
              listBox.appendChild(empty);
            }
          })
          .catch(function () {
            var err = document.createElement("div");
            err.textContent = "(could not list directory)";
            err.style.cssText = "padding:10px;color:#f66;";
            listBox.appendChild(err);
          });
      }

      load(curDir);
      document.body.appendChild(overlay);
    }

    // record control over the /rec WebSocket
    function startRecord(relDir) {
      var ifaces = collectSniffable();
      if (!ifaces.length) {
        buildRows([]);   // shows the "no sniffable interfaces" message
        return;
      }
      buildRows(ifaces);

      var proto = (location.protocol === "https:") ? "wss://" : "ws://";
      var ws;
      try { ws = new WebSocket(proto + location.host + "/rec"); }
      catch (e) { stateLbl.textContent = "ws error"; return; }
      state.ws = ws;

      recBtn.disabled = true;
      stateLbl.textContent = "starting…";

      ws.onopen = function () {
        ws.send(JSON.stringify({ cmd: "start", dir: relDir, ifaces: ifaces }));
      };
      ws.onmessage = function (ev) {
        var m;
        try { m = JSON.parse(ev.data); } catch (e) { return; }
        if (m.type === "started") {
          // The server echoes the interfaces it actually grabbed.
          if (m.ifaces) buildRows(m.ifaces);
          stopBtn.disabled = false;
          stateLbl.textContent = "recording";
        } else if (m.type === "count") {
          setCount(m.name, m.num, m.pkts);
        } else if (m.type === "stopped") {
          stateLbl.textContent = "saved";
        } else if (m.type === "error") {
          stateLbl.textContent = "error: " + (m.msg || "");
          recBtn.disabled = false;
          stopBtn.disabled = true;
        }
      };
      ws.onerror = function () { stateLbl.textContent = "ws error"; };
      ws.onclose = function () {
        state.ws = null;
        recBtn.disabled = false;
        stopBtn.disabled = true;
        if (stateLbl.textContent === "recording")
          stateLbl.textContent = "stopped";
      };
    }

    function stopRecord() {
      stopBtn.disabled = true;
      stateLbl.textContent = "stopping…";
      try {
        if (state.ws && state.ws.readyState === 1)
          state.ws.send(JSON.stringify({ cmd: "stop" }));
      } catch (e) {}
      // stop first so files are flushed, then close after a short grace.
      setTimeout(function () { try { if (state.ws) state.ws.close(); } catch (e) {} }, 400);
    }

    // wire controls
    recBtn.addEventListener("click", function () {
      browseDir(function (curDir) {
        dirLine.textContent = "dir: /usr/share/cloonix/" + (curDir || "");
        // curDir is relative to the share root the server joins it to.
        startRecord(curDir || "");
      });
    });

    stopBtn.addEventListener("click", function () { stopRecord(); });

    raisePanel(panel);
  }

  // expose for menu wiring / testing
  window.openPcaprecPanel = openPcaprecPanel;

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", installButton);
  else
    installButton();
})();
