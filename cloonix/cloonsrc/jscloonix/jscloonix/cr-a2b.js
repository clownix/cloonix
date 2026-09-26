// Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later

(function () {
  "use strict";

  var CMDS = [
    { cmd:"loss",     label:"Loss",     min:0, max:10000, step:10,  unit:"/10000",
      help:"Packet loss. 0 = none, 10000 = everything dropped." },
    { cmd:"delay",    label:"Delay",    min:0, max:2000,  step:5,   unit:"ms",
      help:"One-way delay added to every packet." },
    { cmd:"rate",     label:"Rate",     min:0, max:100000,step:100, unit:"ko/s",
      help:"Throughput cap in kilo-octets/s. 0 = unlimited." },
    { cmd:"silentms", label:"Silent",   min:0, max:2000,  step:10,  unit:"ms",
      help:"Bursts: the wire closed for this long, then open for as long. 0 = steady." },
  ];

  // One panel per a2b name; re-opening focuses the existing one.
  var g_a2b_panels = {};

  // Coalesced sender: at most one POST in flight per (name,dir,cmd); while
  // one is pending the latest value is stashed and fired when the first
  // returns.
  var g_inflight = {};   // key -> true while a POST is outstanding
  var g_pending  = {};   // key -> latest value waiting to be sent

  function sendKey(name, dir, cmd) { return name + "|" + dir + "|" + cmd; }

  function flush(name, dir, cmd) {
    var key = sendKey(name, dir, cmd);
    if (g_inflight[key]) return;            // a send is running; value stashed
    if (!(key in g_pending)) return;        // nothing to send
    var val = g_pending[key];
    delete g_pending[key];
    g_inflight[key] = true;
    fetch("/ctl/cnf-a2b", {
      method:"POST", headers:{"Content-Type":"application/json"},
      body: JSON.stringify({ name:name, dir:dir, cmd:cmd, val:val })
    })
    .then(function (r) {
      if (!r.ok) r.text().then(function (t) {
        console.error("cnf-a2b error:", t); });
    })
    .catch(function (e) { console.error("cnf-a2b network error:", e); })
    .then(function () {                      // finally: send the latest stash
      g_inflight[key] = false;
      flush(name, dir, cmd);
    });
  }

  function sendA2b(name, dir, cmd, val) {
    var key = sendKey(name, dir, cmd);
    g_pending[key] = val|0;
    flush(name, dir, cmd);
  }

  function mkBtn(text, onClick) {
    var b = document.createElement("button");
    b.textContent = text;
    b.style.cssText =
      "flex:1;padding:5px 0;background:#0f3460;color:#e6edf3;border:1px solid #1f4287;" +
      "border-radius:4px;cursor:pointer;font:inherit;";
    b.addEventListener("click", onClick);
    return b;
  }

  function makeDraggable(panel, handle) {
    var sx = 0, sy = 0, ox = 0, oy = 0, drag = false;
    handle.addEventListener("mousedown", function (e) {
      drag = true; sx = e.clientX; sy = e.clientY;
      var rc = panel.getBoundingClientRect();
      ox = rc.left; oy = rc.top;
      panel.style.right = "auto";
      panel.style.left = ox + "px";
      panel.style.top  = oy + "px";
      e.preventDefault();
    });
    window.addEventListener("mousemove", function (e) {
      if (!drag) return;
      panel.style.left = (ox + e.clientX - sx) + "px";
      panel.style.top  = (oy + e.clientY - sy) + "px";
    });
    window.addEventListener("mouseup", function () { drag = false; });
  }

  // Build the four-slider column for one direction.
  function buildDirColumn(name, dir) {
    var col = document.createElement("div");
    col.style.cssText = "flex:1;min-width:0;";

    var hd = document.createElement("div");
    hd.textContent = dir ? "B to A" : "A to B";
    hd.style.cssText =
      "font-weight:600;color:#7fb2ff;margin:0 0 8px;padding-bottom:4px;" +
      "border-bottom:1px solid #1f4287;";
    col.appendChild(hd);

    CMDS.forEach(function (def) {
      var row = document.createElement("div");
      row.style.cssText = "margin-bottom:11px;";

      var top = document.createElement("div");
      top.style.cssText =
        "display:flex;justify-content:space-between;align-items:baseline;";
      var lab = document.createElement("label");
      lab.textContent = def.label;
      lab.style.cssText = "font-weight:500;";
      var valEl = document.createElement("span");
      valEl.style.cssText =
        "font-variant-numeric:tabular-nums;color:#7fb2ff;font-size:11px;";
      valEl.textContent = "0 " + def.unit;
      top.appendChild(lab); top.appendChild(valEl);
      row.appendChild(top);

      var sl = document.createElement("input");
      sl.type = "range";
      sl.min = def.min; sl.max = def.max; sl.step = def.step;
      sl.value = 0;
      sl.style.cssText = "width:100%;margin:4px 0 2px;accent-color:#3498db;";
      sl.addEventListener("input", function () {
        var v = +sl.value;
        valEl.textContent = v + " " + def.unit;
        sendA2b(name, dir, def.cmd, v);
      });
      row.appendChild(sl);

      var hlp = document.createElement("div");
      hlp.textContent = def.help;
      hlp.style.cssText = "color:#8aa0c0;font-size:10px;line-height:1.3;";
      row.appendChild(hlp);

      col.appendChild(row);
    });

    return col;
  }

  function closeA2bPanel(name) {
    var p = g_a2b_panels[name];
    if (p && p.parentElement) p.parentElement.removeChild(p);
    delete g_a2b_panels[name];
  }

  function openA2bPanel(name) {
    if (g_a2b_panels[name]) {                 // already open — bring to front
      g_a2b_panels[name].style.zIndex = 10000;
      return;
    }
    var p = document.createElement("div");
    p.id = "a2b-panel-" + name;
    p.style.cssText =
      "position:fixed;top:60px;right:16px;width:420px;z-index:9999;" +
      "background:#0b1f3a;color:#e6edf3;border:1px solid #1f4287;border-radius:8px;" +
      "box-shadow:0 8px 28px rgba(0,0,0,.55);font:12px/1.4 system-ui,sans-serif;" +
      "user-select:none;";

    // header (draggable)
    var hd = document.createElement("div");
    hd.style.cssText =
      "display:flex;align-items:center;justify-content:space-between;" +
      "padding:8px 10px;background:#0f3460;border-radius:8px 8px 0 0;cursor:move;";
    hd.innerHTML = "<span style='font-weight:600'>\u21c4 a2b shaping \u2014 " +
                   name + "</span>";
    var x = document.createElement("span");
    x.textContent = "\u2715";
    x.style.cssText = "cursor:pointer;padding:0 4px;opacity:.8;";
    x.addEventListener("click", function () { closeA2bPanel(name); });
    hd.appendChild(x);
    p.appendChild(hd);
    makeDraggable(p, hd);

    // body: two direction columns side by side
    var body = document.createElement("div");
    body.style.cssText =
      "display:flex;gap:16px;padding:12px;max-height:70vh;overflow:auto;";
    body.appendChild(buildDirColumn(name, 0));
    body.appendChild(buildDirColumn(name, 1));
    p.appendChild(body);

    // footer: reset both directions to zero
    var foot = document.createElement("div");
    foot.style.cssText =
      "display:flex;gap:8px;padding:10px 12px;border-top:1px solid #1f4287;";
    var resetBtn = mkBtn("Reset all", function () {
      // Zero every slider and push the zeros to the a2b process.
      p.querySelectorAll("input[type=range]").forEach(function (sl) {
        sl.value = 0;
        sl.dispatchEvent(new Event("input"));
      });
    });
    foot.appendChild(resetBtn);
    p.appendChild(foot);

    document.body.appendChild(p);
    g_a2b_panels[name] = p;
  }

  // Expose to the context menu (cr-topo-menu.js).
  window.openA2bPanel  = openA2bPanel;
  window.closeA2bPanel = closeA2bPanel;
})();
