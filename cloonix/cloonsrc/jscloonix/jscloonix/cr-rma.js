// cr-rma.js — header-level "Delete all" button.

(function () {
  "use strict";

  function mkBtn(label, bg) {
    var b = document.createElement("button");
    b.textContent = label;
    b.style.cssText =
      "padding:3px 12px;background:" + bg + ";color:#e6edf3;border:1px solid #1f4287;" +
      "border-radius:4px;cursor:pointer;font:inherit;";
    b.addEventListener("mousedown", function (e) { e.stopPropagation(); });
    return b;
  }

  function confirmBox(message, onYes) {
    var overlay = document.createElement("div");
    overlay.style.cssText =
      "position:fixed;inset:0;background:#000a;z-index:10000;" +
      "display:flex;align-items:center;justify-content:center;";
    overlay.addEventListener("mousedown", function (e) { e.stopPropagation(); });
    var box = document.createElement("div");
    box.style.cssText =
      "min-width:300px;max-width:80vw;background:#16213e;border:1px solid #5a1e1e;" +
      "border-radius:6px;box-shadow:0 8px 32px #000c;color:#e6edf3;" +
      "font:13px system-ui,sans-serif;padding:16px;display:flex;" +
      "flex-direction:column;gap:14px;";
    var msg = document.createElement("div");
    msg.textContent = message;
    box.appendChild(msg);
    var row = document.createElement("div");
    row.style.cssText = "display:flex;gap:8px;justify-content:flex-end;";
    var noB = mkBtn("Cancel", "#21262d");
    var yesB = mkBtn("Delete all", "#5a1e1e");
    row.appendChild(noB); row.appendChild(yesB);
    box.appendChild(row);
    overlay.appendChild(box);
    function destroy() {
      if (overlay.parentElement) overlay.parentElement.removeChild(overlay);
    }
    noB.addEventListener("click", destroy);
    yesB.addEventListener("click", function () { destroy(); onYes(); });
    document.body.appendChild(overlay);
  }

  function failed(why) {
    console.error("Delete all failed:", why);
    try {
      if (typeof confirmBox === "function")
        confirmBox("Delete all failed: " + why, function () {});
      else
        alert("Delete all failed: " + why);
    } catch (e) {}
  }

  function runRma() {
    fetch("/ctl/rma", { method: "POST", cache: "no-store" })
      .then(function (r) {
        if (!r.ok) failed("the server answered " + r.status);
      })
      .catch(function (e) { failed(String(e)); });

    try {
      if (typeof panels_close_all === "function") panels_close_all();
    } catch (e) { console.error("panels_close_all failed", e); }
  }

  function installButton() {
    var header = document.getElementById("header");
    if (!header) return;
    if (document.getElementById("rma-btn")) return;
    var btn = document.createElement("button");
    btn.textContent = "Delete all";
    btn.id = "rma-btn";
    btn.style.cssText =
      "margin-left:8px;padding:4px 14px;background:#5a1e1e;color:#e6edf3;" +
      "border:1px solid #1f4287;border-radius:4px;cursor:pointer;font:inherit;";
    btn.addEventListener("mouseenter", function () { btn.style.background = "#7a2828"; });
    btn.addEventListener("mouseleave", function () { btn.style.background = "#5a1e1e"; });
    btn.addEventListener("click", function () {
      confirmBox("Delete all cloonix objects (rma)? This cannot be undone.",
                 runRma);
    });
    header.appendChild(btn);
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", installButton);
  else
    installButton();
})();
