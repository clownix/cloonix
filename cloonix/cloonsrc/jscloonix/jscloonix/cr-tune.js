// Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later

(function () {
  "use strict";

  // Default snapshot so Reset can restore the validated values.
  var DEFAULTS = {
    REPEL:          1000000,
    SPRING:         0.9,
    DAMP:           0.88,
    VEL_CAP_F:      0.7,
    L0_F:           1.15,
    SPRING_MUL_MAX: 12,
    SETTLE_KE:      0.008,
  };

  // Slider definitions: key into g_tune, label, min, max, step, and an
  // optional log flag for wide-range values (REPEL spans 5 decades).
  var SLIDERS = [
    { key:"REPEL",          label:"Repulsion",            min:50000, max:5000000, step:10000, log:true,
      help:"How hard nodes push each other apart. Higher = wider, more open web." },
    { key:"SPRING",         label:"Link attraction",      min:0.05,  max:3.0,     step:0.05,
      help:"How strongly a link pulls its node and LAN together. Higher = tighter links." },
    { key:"SPRING_MUL_MAX", label:"Long-link pull",       min:1,     max:30,      step:1,
      help:"Extra pull on over-stretched links. Higher = fewer crossings, snappier untangle." },
    { key:"DAMP",           label:"Damping (less brake →)",min:0.70,  max:0.95,    step:0.01,
      help:"Velocity kept each step. Higher = faster motion; too high (>0.90) oscillates." },
    { key:"VEL_CAP_F",      label:"Max speed",            min:0.2,   max:2.0,     step:0.05,
      help:"Speed limit per step (×node radius). Higher = quicker but can overshoot." },
    { key:"L0_F",           label:"Link length",          min:0.6,   max:3.0,     step:0.05,
      help:"Natural link length (×node radius). Higher = more spread-out layout." },
    { key:"SETTLE_KE",      label:"Settle threshold",     min:0.001, max:0.1,     step:0.001, log:true,
      help:"How still it must get before it stops. Higher = stops sooner, rougher." },
  ];

  var g_panel = null;

  function wakeLayout() {
    try {
      if (typeof reviveLayout === "function") { reviveLayout(120); return; }
    } catch (e) {}
    try {
      if (typeof g_layout_settled !== "undefined") g_layout_settled = false;
      if (typeof g_layout_wake    !== "undefined") g_layout_wake    = 120;
    } catch (e) {}
  }

  function fmt(v) {
    if (v >= 100000) return (v/1000000).toFixed(2) + "M";
    if (v >= 1000)   return Math.round(v).toString();
    if (v >= 1)      return (Math.round(v*100)/100).toString();
    return (Math.round(v*1000)/1000).toString();
  }

  function toSlider(def, val) {
    if (def.log) {
      var lo = Math.log(def.min), hi = Math.log(def.max);
      return (Math.log(Math.max(def.min, Math.min(def.max, val))) - lo) / (hi - lo) * 1000;
    }
    return (val - def.min) / (def.max - def.min) * 1000;
  }
  function fromSlider(def, pos) {
    var t = pos / 1000;
    if (def.log) {
      var lo = Math.log(def.min), hi = Math.log(def.max);
      return Math.exp(lo + t * (hi - lo));
    }
    var v = def.min + t * (def.max - def.min);
    // snap to step for linear sliders
    return Math.round(v / def.step) * def.step;
  }

  function installButton() {
    var header = document.getElementById("header");
    if (!header) return;
    if (document.getElementById("tune-btn")) return;
    var btn = document.createElement("button");
    btn.textContent = "Tune";
    btn.id = "tune-btn";
    btn.style.cssText =
      "margin-left:8px;padding:4px 14px;background:#0f3460;color:#e6edf3;" +
      "border:1px solid #1f4287;border-radius:4px;cursor:pointer;font:inherit;";
    btn.addEventListener("mouseenter", function () { btn.style.background = "#1f4287"; });
    btn.addEventListener("mouseleave", function () { btn.style.background = "#0f3460"; });
    btn.addEventListener("click", function () { togglePanel(); });
    header.appendChild(btn);
  }

  function togglePanel() {
    if (g_panel) { closePanel(); return; }
    openPanel();
  }

  function closePanel() {
    if (g_panel && g_panel.parentElement) g_panel.parentElement.removeChild(g_panel);
    g_panel = null;
  }

  function openPanel() {
    if (typeof g_tune === "undefined") {
      alert("Tuning is unavailable (g_tune not found).");
      return;
    }
    var p = document.createElement("div");
    p.id = "tune-panel";
    p.style.cssText =
      "position:fixed;top:60px;right:16px;width:300px;z-index:9999;" +
      "background:#0b1f3a;color:#e6edf3;border:1px solid #1f4287;border-radius:8px;" +
      "box-shadow:0 8px 28px rgba(0,0,0,.55);font:12px/1.4 system-ui,sans-serif;" +
      "user-select:none;";

    // header (draggable)
    var hd = document.createElement("div");
    hd.style.cssText =
      "display:flex;align-items:center;justify-content:space-between;" +
      "padding:8px 10px;background:#0f3460;border-radius:8px 8px 0 0;cursor:move;";
    hd.innerHTML = "<span style='font-weight:600'>⚙ Layout tuning</span>";
    var x = document.createElement("span");
    x.textContent = "✕";
    x.style.cssText = "cursor:pointer;padding:0 4px;opacity:.8;";
    x.addEventListener("click", closePanel);
    hd.appendChild(x);
    p.appendChild(hd);
    makeDraggable(p, hd);

    // body
    var body = document.createElement("div");
    body.style.cssText = "padding:10px 12px;max-height:70vh;overflow:auto;";
    p.appendChild(body);

    var rows = {};
    SLIDERS.forEach(function (def) {
      var row = document.createElement("div");
      row.style.cssText = "margin-bottom:12px;";

      var top = document.createElement("div");
      top.style.cssText = "display:flex;justify-content:space-between;align-items:baseline;";
      var lab = document.createElement("label");
      lab.textContent = def.label;
      lab.style.cssText = "font-weight:500;";
      var valEl = document.createElement("span");
      valEl.style.cssText = "font-variant-numeric:tabular-nums;color:#7fb2ff;";
      valEl.textContent = fmt(g_tune[def.key]);
      top.appendChild(lab); top.appendChild(valEl);
      row.appendChild(top);

      var sl = document.createElement("input");
      sl.type = "range";
      sl.min = 0; sl.max = 1000; sl.step = 1;
      sl.value = toSlider(def, g_tune[def.key]);
      sl.style.cssText = "width:100%;margin:4px 0 2px;accent-color:#3498db;";
      sl.addEventListener("input", function () {
        var v = fromSlider(def, +sl.value);
        g_tune[def.key] = v;
        valEl.textContent = fmt(v);
        wakeLayout();
      });
      row.appendChild(sl);

      var hlp = document.createElement("div");
      hlp.textContent = def.help;
      hlp.style.cssText = "color:#8aa0c0;font-size:11px;";
      row.appendChild(hlp);

      body.appendChild(row);
      rows[def.key] = { slider: sl, valEl: valEl, def: def };
    });

    // footer buttons
    var foot = document.createElement("div");
    foot.style.cssText =
      "display:flex;gap:8px;padding:10px 12px;border-top:1px solid #1f4287;";

    var resetBtn = mkBtn("Reset", function () {
      Object.keys(DEFAULTS).forEach(function (k) { g_tune[k] = DEFAULTS[k]; });
      SLIDERS.forEach(function (def) {
        rows[def.key].slider.value = toSlider(def, g_tune[def.key]);
        rows[def.key].valEl.textContent = fmt(g_tune[def.key]);
      });
      wakeLayout();
    });

    var rerunBtn = mkBtn("Re-run", function () {
      try {
        if (typeof g_last_topo !== "undefined" && g_last_topo &&
            typeof applyTopo === "function") {
          applyTopo(g_last_topo, false);
        } else {
          wakeLayout();
        }
      } catch (e) { wakeLayout(); }
    });
    rerunBtn.style.background = "#14502a";
    rerunBtn.style.borderColor = "#1f7a42";

    foot.appendChild(resetBtn);
    foot.appendChild(rerunBtn);
    p.appendChild(foot);

    document.body.appendChild(p);
    g_panel = p;
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
      var r = panel.getBoundingClientRect();
      ox = r.left; oy = r.top;
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

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", installButton);
  else
    installButton();
})();
