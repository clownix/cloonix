// cr-topo-panels.js — click/dblclick handling and the floating panel manager
// (SSH, console, VNC and webshark panels, raise/lower, tab transfer).

// Click / dblclick handlers
function handleClick(mx, my) {
  if (g_selected_lan) {
    const hit = ethAt(mx, my);
    // a wlan on an air, an eth on a lan; a new name takes either
    const kind = lanKind(g_selected_lan);
    if (hit && ((kind === "") ||
                ((kind === "air") === isWlan(hit.node, hit.ethIdx)))) {
      const { node, ethIdx } = hit;
      const already = g_edges.find(e=>e.name===node.name&&e.num===ethIdx&&e.lan===g_selected_lan);
      if (!already) post("/ctl/add-edge",{name:node.name,num:ethIdx,lan:g_selected_lan});
      else          post("/ctl/del-edge",{name:node.name,num:ethIdx,lan:g_selected_lan});
      g_selected_lan = null;   // deselect immediately — edge will confirm via topo push
      draw(); return;
    }
    const lhit = lanAt(mx, my);
    if (lhit) {
      g_selected_lan = (g_selected_lan===lhit.name) ? null : lhit.name;
      draw(); return;
    }
    g_selected_lan = null; draw(); return;
  }
}

function handleDblClick(mx, my) {
  // Double-click on an air: its capture; on a lan: select/deselect
  const lhit = lanAt(mx, my);
  if (lhit && isAir(lhit.name)) {
    if (g_air_snf.has(lhit.name)) { openAirWebshark(lhit.name, lhit); return; }
    const sc = worldToScreen(lhit.x, lhit.y);
    const tip = document.createElement("div");
    tip.textContent = `the air ${lhit.name} is not captured, right-click for "Sniff on"`;
    tip.style.cssText = `position:fixed; left:${sc.x+10}px; top:${sc.y-24}px;
      background:#7e5109; color:#e0e0e0; font:11px monospace;
      padding:3px 8px; border-radius:3px; pointer-events:none; z-index:9000;`;
    document.body.appendChild(tip);
    setTimeout(()=>tip.remove(), 2000);
    return;
  }
  if (lhit) {
    g_selected_lan = (g_selected_lan===lhit.name) ? null : lhit.name;
    draw(); return;
  }
  // Double-click on eth interface: open webshark capture panel (eths only)
  const ehit = ethAt(mx, my);
  if (ehit) {
    const { node, ethIdx } = ehit;
    if (isWlan(node, ethIdx)) {
      // a wlan is captured with its air: the capture of the air opens
      const air = airOf(node, ethIdx);
      if (air && g_air_snf.has(air)) {
        openAirWebshark(air, g_lans.find(l => l.name === air));
        return;
      }
      const ep = ethPos(node, ethIdx);
      const sc = worldToScreen(ep.x, ep.y);
      const tip = document.createElement("div");
      tip.textContent = air ? `wlan${wlanIdx(node, ethIdx)}: its air ${air} is ` +
                              `not captured, right-click for "Sniff on"`
                            : `wlan${wlanIdx(node, ethIdx)}: on no air`;
      tip.style.cssText = `position:fixed; left:${sc.x+10}px; top:${sc.y-24}px;
        background:#7e5109; color:#e0e0e0; font:11px monospace;
        padding:3px 8px; border-radius:3px; pointer-events:none; z-index:9000;`;
      document.body.appendChild(tip);
      setTimeout(()=>tip.remove(), 2000);
    } else if (node.etype && node.etype[ethIdx] === "ethv") {
      // ethv = no sniff — show brief tooltip instead
      const ep = ethPos(node, ethIdx);
      const sc = worldToScreen(ep.x, ep.y);
      const tip = document.createElement("div");
      tip.textContent = `eth${ethIdx}: not sniffable, right-click for "Sniff on"`;
      tip.style.cssText = `position:fixed; left:${sc.x+10}px; top:${sc.y-24}px;
        background:#4a235a; color:#e0e0e0; font:11px monospace;
        padding:3px 8px; border-radius:3px; pointer-events:none; z-index:9000;`;
      document.body.appendChild(tip);
      setTimeout(()=>tip.remove(), 2000);
    } else {
      openWebsharkPanel(node.name, ethIdx, node);
    }
    return;
  }
  const nhit = nodeAt(mx, my);
  if (nhit && g_net) {
    openSshPanel(nhit.name, g_net, nhit, "ssh");
  }
}

let g_ssh_panels = {};       // panelKey → { panel, term, ws, fitAddon, anchorNode }
let g_demo_panels = {};      // panelKey → { panel, close, transferred, popup } — script player panels
let g_ssh_seq = 1;           // monotonic id for unlimited ssh panels

const CANVAS_Z = 400;
const PANEL_Z_BG = 100;      // baseline z-index for backgrounded panels
let g_panel_z = CANVAS_Z;    // monotonic z-index for panels raised to front

function raisePanel(panel) {
  g_panel_z += 1;
  if (g_panel_z > 900) g_panel_z = CANVAS_Z + 1;
  panel.style.zIndex = g_panel_z;
}

// Keyboard to an xterm: its window needs the focus first (an iframe panel
// keeps it, xterm prevents the click default).
function focusTerm(term) {
  const doc = (term.element && term.element.ownerDocument) || document;
  try {
    const a = doc.activeElement;
    if (a && a.tagName === "IFRAME") a.blur();
  } catch(_){}
  try { doc.defaultView.focus(); } catch(_){}
  try { term.focus(); } catch(_){}
}

// The keyboard stays with the panel clicked last: a focus fallen on nothing,
// or taken by an iframe nobody clicked (a vnc reconnecting), goes back to it.
let g_kbd_panel = null;  // { panel, focus }

function kbdKept(a) {
  if (!a || a === document.body) return false;
  if (a.tagName === "IFRAME")
    return !a._kbdHooked || (g_kbd_panel && g_kbd_panel.panel.contains(a));
  return a.matches("input,textarea,select,[contenteditable]");
}

function kbdCheck() {
  setTimeout(() => {
    const k = g_kbd_panel;
    if (!k || !k.panel.isConnected || k.panel.ownerDocument !== document) return;
    if (!document.hasFocus() || kbdKept(document.activeElement)) return;
    k.focus();
  }, 0);
}
document.addEventListener("focusout", kbdCheck, true);
window.addEventListener("focus", kbdCheck);

function lowerAllPanels() {
  const lower = p => { if (p && p.panel) p.panel.style.zIndex = PANEL_Z_BG; };
  try { Object.values(g_ssh_panels).forEach(lower); } catch(e){}
  try { Object.values(g_vnc_panels).forEach(lower); } catch(e){}
  try { Object.values(g_webshark_panels).forEach(lower); } catch(e){}
  try { Object.values(g_demo_panels).forEach(lower); } catch(e){}
  try { Object.values(g_traffic_panels).forEach(lower); } catch(e){}
  g_panel_z = CANVAS_Z;   // next raise goes just above the canvas
}

function reapDeadPanels() {
  const live = {};
  try { g_nodes.forEach(n => { live[n.name] = true; }); } catch(e){ return; }
  // the wifi lan anchors the capture of the air
  try { g_lans.forEach(l => { live[l.name] = true; }); } catch(e){}
  const reap = registry => {
    Object.values(registry).forEach(p => {
      if (p && p.vmKey && !live[p.vmKey]) {
        try { p.close(); } catch(e){}
      }
    });
  };
  reap(g_ssh_panels);
  reap(g_vnc_panels);
  reap(g_webshark_panels);
  try { reap(g_traffic_panels); } catch(e){}
}

function panels_close_all() {
  const shut = registry => {
    try {
      Object.keys(registry).forEach(k => {
        const p = registry[k];
        let closed = false;
        try { if (p && p.close) { p.close(); closed = true; } } catch(e) {}
        // An entry with no close of its own must still leave the screen.
        if (!closed) {
          try {
            const el = p && (p.panel || p.outer);
            if (el && el.parentElement) el.parentElement.removeChild(el);
            if (p && p.popup && !p.popup.closed) p.popup.close();
            if (p && p.ws) p.ws.close();
          } catch(e) {}
        }
        delete registry[k];
      });
    } catch(e) {}
  };
  shut(g_ssh_panels);
  shut(g_vnc_panels);
  shut(g_webshark_panels);
  shut(g_demo_panels);
  try { shut(g_traffic_panels); } catch(e) {}
}

// Return the topmost floating panel whose rectangle contains the given screen
// point, or null.
function panelAtScreen(clientX, clientY) {
  let best = null, bestZ = -1;
  const consider = p => {
    if (!p || !p.panel || p.transferred) return;
    const el = p.panel;
    const rc = el.getBoundingClientRect();
    if (clientX >= rc.left && clientX <= rc.right &&
        clientY >= rc.top  && clientY <= rc.bottom) {
      const z = parseInt(el.style.zIndex || PANEL_Z_BG, 10);
      if (z >= bestZ) { bestZ = z; best = el; }
    }
  };
  try { Object.values(g_ssh_panels).forEach(consider); } catch(e){}
  try { Object.values(g_vnc_panels).forEach(consider); } catch(e){}
  try { Object.values(g_webshark_panels).forEach(consider); } catch(e){}
  try { Object.values(g_demo_panels).forEach(consider); } catch(e){}
  try { Object.values(g_traffic_panels).forEach(consider); } catch(e){}
  return best;
}

// Panel border under the canvas at this point if nothing covers it:
// { panel, dir } or null (elementsFromPoint, paint order).
function panelBorderAtScreen(clientX, clientY) {
  const r = canvas.getBoundingClientRect();
  const w = screenToWorld(clientX - r.left, clientY - r.top);
  if (nodeAt(w.x, w.y) || lanAt(w.x, w.y) || ethAt(w.x, w.y)) return null;
  const els = document.elementsFromPoint(clientX, clientY);
  const k = els.indexOf(canvas);
  const el = (k >= 0) ? els[k + 1] : null;
  if (!el || !el.dataset || !el.dataset.rdir) return null;
  return { panel: el.parentElement, dir: el.dataset.rdir };
}

function panelBarAtScreen(clientX, clientY) {
  let best = null, bestZ = -1;
  const consider = p => {
    if (!p || !p.panel || p.transferred) return;
    const bar = p.panel.firstElementChild;   // makePanelShell appends bar first
    if (!bar) return;
    const rc = bar.getBoundingClientRect();
    if (clientX >= rc.left && clientX <= rc.right &&
        clientY >= rc.top  && clientY <= rc.bottom) {
      const z = parseInt(p.panel.style.zIndex || PANEL_Z_BG, 10);
      if (z >= bestZ) { bestZ = z; best = p.panel; }
    }
  };
  try { Object.values(g_ssh_panels).forEach(consider); } catch(e){}
  try { Object.values(g_vnc_panels).forEach(consider); } catch(e){}
  try { Object.values(g_webshark_panels).forEach(consider); } catch(e){}
  try { Object.values(g_demo_panels).forEach(consider); } catch(e){}
  try { Object.values(g_traffic_panels).forEach(consider); } catch(e){}
  return best;
}
let g_webshark_panels = {};  // "name_num" → { panel, anchorNode }

function updatePanelPositions() {
  // Reposition panels anchored to their node — but skip if manually dragged
  // (pinned)
  const wrap = canvas.parentElement;
  if (!wrap) return;
  const wRect = wrap.getBoundingClientRect();

  // Base sizes at zoom=1
  const SSH_BASE_W = 540, SSH_BASE_H = 280;
  const WS_BASE_W  = 720, WS_BASE_H  = 420;

  // Build a per-anchor-node stagger index so multiple panels anchored to the
  // same node cascade instead of stacking exactly on top of each other.
  const stagger = {};
  function staggerIndex(nd) {
    if (!nd) return 0;
    const k = nd.name || "?";
    const i = stagger[k] || 0;
    stagger[k] = i + 1;
    return i;
  }

  function reposition(panelObj, baseW, baseH, fixedSize) {
    if (panelObj.transferred) return;
    // Clamp zoom for panel size: don't go below 0.5× or above 2×
    const zf = Math.max(0.5, Math.min(2.0, g_zoom));
    let pw, ph;
    if (fixedSize) {
      const wrap2 = canvas.parentElement;
      const vw = wrap2 ? wrap2.clientWidth  : window.innerWidth;
      const vh = wrap2 ? wrap2.clientHeight : window.innerHeight;
      pw = Math.min(baseW, Math.round(vw * 0.9));
      ph = Math.min(baseH, Math.round(vh * 0.9));
    } else {
      pw = Math.round(baseW * zf);
      ph = Math.round(baseH * zf);
    }

    // Resize panel div to follow zoom (only if not pinned — pinned panels
    // keep their size)
    if (!panelObj.pinned) {
      panelObj.panel.style.width  = pw + "px";
      panelObj.panel.style.height = ph + "px";
    }

    if (panelObj.pinned) return;   // don't reposition pinned panels

    const nd = panelObj.anchorNode;
    if (!nd) return;
    const si = staggerIndex(nd);
    const sc = worldToScreen(nd.x, nd.y);
    let px = sc.x + (nd.r || LAN_R) * g_zoom + 8 + si * 26;
    let py = sc.y - ph / 2 + si * 26;
    px = Math.max(0, Math.min(px, wRect.width  - pw));
    py = Math.max(0, Math.min(py, wRect.height - ph));
    panelObj.panel.style.left   = px + "px";
    panelObj.panel.style.top    = py + "px";
    panelObj.panel.style.right  = "auto";
    panelObj.panel.style.bottom = "auto";
  }

  Object.values(g_ssh_panels).forEach(p => reposition(p, SSH_BASE_W, SSH_BASE_H));
  Object.values(g_webshark_panels).forEach(p => reposition(p, WS_BASE_W, WS_BASE_H));
  Object.values(g_vnc_panels).forEach(p => reposition(p, 800, 520, true));
  try { Object.values(g_traffic_panels).forEach(p => reposition(p, 480, 300)); } catch(e){}

  // Refit all xterm terminals after potential size change
  Object.values(g_ssh_panels).forEach(p => {
    try { p.fitAddon && p.fitAddon.fit(); } catch(e) {}
  });
}

function makePanelShell(title, onClose, w, h, panelRef) {
  // panelRef { pinned } is shared with the caller.
  const wrap = canvas.parentElement;
  wrap.style.position = "relative";
  const panel = document.createElement("div");
  panel.style.cssText = `
    position:absolute; left:10px; top:10px;
    width:${w}px; height:${h}px;
    background:#0d1117; border:1px solid #30363d; border-radius:6px;
    box-shadow:0 8px 24px #000c; z-index:100;
    display:flex; flex-direction:column; overflow:hidden;`;

  const bar = document.createElement("div");
  bar.style.cssText = `
    display:flex; align-items:center; padding:4px 8px; flex-shrink:0;
    background:#161b22; border-bottom:1px solid #30363d;
    cursor:move; user-select:none;
    font:11px 'SF Mono','Fira Code',monospace; color:#8b949e;`;

  const dot = document.createElement("span");
  dot.style.cssText = "width:9px;height:9px;border-radius:50%;background:#3fb950;margin-right:8px;flex-shrink:0;";
  bar.appendChild(dot);

  const lbl = document.createElement("span");
  lbl.textContent = title; lbl.style.flex = "1";
  bar.appendChild(lbl);

  const btnClose = document.createElement("span");
  btnClose.textContent = "✕"; btnClose.title = "Close";
  btnClose.style.cssText = "cursor:pointer;padding:0 4px;font-size:13px;";
  btnClose.addEventListener("click", onClose);
  bar.appendChild(btnClose);

  panel.appendChild(bar);
  wrap.appendChild(panel);

  raisePanel(panel);
  // One click raises the panel and gives it the keyboard: panelRef.focus
  // (a terminal), else the iframe, whose canvas needs its document focused.
  function focusPanel() {
    if (panelRef && panelRef.focus) { try { panelRef.focus(); } catch(_){} return; }
    const f = panel.querySelector("iframe");
    if (!f) return;
    try { f.focus(); } catch(_){}
    try { if (f.contentWindow) f.contentWindow.focus(); } catch(_){}
  }
  function own() {
    if ((panelRef && panelRef.focus) || panel.querySelector("iframe"))
      g_kbd_panel = { panel, focus: focusPanel };
  }
  // Also called by the canvas: a lowered panel is under it, the click lands there.
  panel._take = ()=>{ raisePanel(panel); own(); focusPanel(); };
  panel.addEventListener("mousedown", panel._take, true);
  // A click inside an iframe never reaches this document.
  panel.addEventListener("load", e=>{
    const f = e.target;
    if (f.tagName !== "IFRAME") return;
    try { f.contentWindow.addEventListener("mousedown", own, true); f._kbdHooked = true; }
    catch(_){}
  }, true);

  let pdragging=false, pdx=0, pdy=0, hasMoved=false;

  function setIframePointerEvents(val) {
    panel.querySelectorAll("iframe").forEach(f => f.style.pointerEvents = val);
  }

  function startDrag(e) {
    pdragging=true; hasMoved=false;
    const pr=panel.getBoundingClientRect();
    pdx=e.clientX-pr.left; pdy=e.clientY-pr.top;
    setIframePointerEvents("none");  // let mouse events pass through iframe
    e.preventDefault(); e.stopPropagation();
  }
  // Also called by the canvas: a lowered panel's bar is under it.
  panel._drag = startDrag;
  bar.addEventListener("mousedown", e=>{
    if(e.button!==0) return;
    startDrag(e);
  });
  // A button released out of the window: no mouseup, seen at the next move.
  document.addEventListener("mousemove", e=>{
    if(!pdragging) return;
    if(!(e.buttons & 1)) { stopDrag(null); return; }
    const wr=wrap.getBoundingClientRect();
    const pw=panel.offsetWidth, ph=panel.offsetHeight;
    let nx=e.clientX-wr.left-pdx, ny=e.clientY-wr.top-pdy;
    nx=Math.max(0,Math.min(nx,wr.width-pw));
    ny=Math.max(0,Math.min(ny,wr.height-ph));
    panel.style.left=nx+"px"; panel.style.top=ny+"px";
    panel.style.right="auto"; panel.style.bottom="auto";
    hasMoved=true;
  });
  function stopDrag(e) {
    if(!pdragging) return;
    if(e && e.button!==0) return;
    if(hasMoved && panelRef) panelRef.pinned = true;
    pdragging=false;
    setIframePointerEvents("");  // restore iframe interaction
  }
  document.addEventListener("mouseup", stopDrag);

  // Resized from any border or corner, as a desktop window; the corner
  // bottom right keeps its drawn grip.
  const MIN_W = 180, MIN_H = 120;
  // the top corners are small, the close button sits near the right one
  const EDGE = 6, CORNER = 14, TOP_CORNER = 8;
  const CURSOR = { n:"ns-resize", s:"ns-resize", e:"ew-resize", w:"ew-resize",
                   ne:"nesw-resize", sw:"nesw-resize",
                   nw:"nwse-resize", se:"nwse-resize" };
  const PLACE = {
    n:  `top:0; left:${TOP_CORNER}px; right:${TOP_CORNER}px; height:${EDGE}px;`,
    s:  `bottom:0; left:${CORNER}px; right:${CORNER}px; height:${EDGE}px;`,
    w:  `left:0; top:${TOP_CORNER}px; bottom:${CORNER}px; width:${EDGE}px;`,
    e:  `right:0; top:${TOP_CORNER}px; bottom:${CORNER}px; width:${EDGE}px;`,
    nw: `top:0; left:0; width:${TOP_CORNER}px; height:${TOP_CORNER}px;`,
    ne: `top:0; right:0; width:${TOP_CORNER}px; height:${TOP_CORNER}px;`,
    sw: `bottom:0; left:0; width:${CORNER}px; height:${CORNER}px;`,
    se: `bottom:0; right:0; width:16px; height:16px;
         background:linear-gradient(135deg, transparent 50%, #30363d 50%, #30363d 60%,
         transparent 60%, transparent 72%, #30363d 72%, #30363d 82%, transparent 82%);`,
  };

  let rsz=null, rsx=0, rsy=0, rsl=0, rst=0, rsw=0, rsh=0;
  function startResize(e, dir) {
    rsz = dir;
    rsx = e.clientX; rsy = e.clientY;
    rsl = panel.offsetLeft; rst = panel.offsetTop;
    rsw = panel.offsetWidth; rsh = panel.offsetHeight;
    panel.style.right = "auto"; panel.style.bottom = "auto";
    panel.style.left = rsl + "px"; panel.style.top = rst + "px";
    setIframePointerEvents("none");  // iframes swallow mousemove/up otherwise
    raisePanel(panel);
    // pinned from the start: the zoom follow would undo the size meanwhile
    if (panelRef) panelRef.pinned = true;
    e.preventDefault(); e.stopPropagation();
  }
  // the se grip last: cr-play.js inserts its content before lastChild
  ["n", "s", "w", "e", "nw", "ne", "sw", "se"].forEach(dir => {
    const h = document.createElement("div");
    h.style.cssText = `position:absolute; z-index:10; cursor:${CURSOR[dir]}; ${PLACE[dir]}`;
    h.dataset.rdir = dir;
    h.addEventListener("mousedown", e => {
      if (e.button !== 0) return;
      startResize(e, dir);
    });
    panel.appendChild(h);
  });
  // Also called by the canvas: the border of a lowered panel is under it.
  panel._resize = startResize;

  document.addEventListener("mousemove", e=>{
    if(!rsz) return;
    if(!(e.buttons & 1)) { stopResize(null); return; }
    const dx = e.clientX - rsx, dy = e.clientY - rsy;
    let l = rsl, t = rst, w = rsw, h = rsh;
    if (rsz.includes("e")) w = Math.max(MIN_W, rsw + dx);
    if (rsz.includes("s")) h = Math.max(MIN_H, rsh + dy);
    if (rsz.includes("w")) { w = Math.max(MIN_W, rsw - dx); l = rsl + rsw - w; }
    if (rsz.includes("n")) { h = Math.max(MIN_H, rsh - dy); t = rst + rsh - h; }
    panel.style.left = l + "px"; panel.style.top = t + "px";
    panel.style.width = w + "px"; panel.style.height = h + "px";
    e.preventDefault();
  });
  function stopResize(e){
    if(!rsz) return;
    if(e && e.button!==0) return;
    rsz=null;
    setIframePointerEvents("");
  }
  document.addEventListener("mouseup", stopResize);

  return { panel, bar, own };
}

function openBackgroundTab(url) {
  const a = document.createElement("a");
  a.href = url;
  a.target = "_blank";
  a.rel = "noopener";
  // Hidden but in the DOM so the click dispatches normally.
  a.style.cssText = "position:absolute;left:-9999px;top:-9999px;";
  document.body.appendChild(a);
  // ctrlKey (Linux/Win) + metaKey (mac) => open in a new background tab.
  const ev = new MouseEvent("click", {
    bubbles: true, cancelable: true, view: window,
    button: 0, ctrlKey: true, metaKey: true,
  });
  a.dispatchEvent(ev);
  setTimeout(()=>{ try { a.remove(); } catch(e){} try { window.focus(); } catch(e){} }, 0);
}

function panelBarMenu(bar, popoutUrl, tabName, onClose, transferNode) {
  bar.addEventListener("contextmenu", e=>{
    e.preventDefault();
    e.stopPropagation();
    // Remove any existing panel menu
    const old = document.getElementById("panel-bar-menu");
    if (old) old.remove();

    const menu = document.createElement("div");
    menu.id = "panel-bar-menu";
    menu.style.cssText = `
      position:fixed; left:${e.clientX}px; top:${e.clientY}px;
      background:#16213e; border:1px solid #0f3460; border-radius:4px;
      font-family:monospace; font-size:12px; color:#e0e0e0;
      z-index:9999; min-width:180px; box-shadow:0 4px 16px #000c; padding:4px 0;`;

    function mitem(label, action) {
      const el = document.createElement("div");
      el.textContent = label;
      el.style.cssText = "padding:7px 18px; cursor:pointer; white-space:nowrap;";
      el.addEventListener("mouseenter", ()=>el.style.background="#0f3460");
      el.addEventListener("mouseleave", ()=>el.style.background="");
      el.addEventListener("mouseup", ()=>{ menu.remove(); action(); });
      menu.appendChild(el);
    }

    mitem("Move to new tab", ()=>{
      if (typeof transferNode === "function") {
        transferNode();
      } else {
        try { openBackgroundTab(popoutUrl); }
        finally { onClose(); }
      }
    });
    mitem("Close panel", ()=> onClose());

    document.body.appendChild(menu);
    // Keep the menu fully on-screen (shift left/up from the click point).
    {
      const M = 6, vw = window.innerWidth, vh = window.innerHeight;
      const r = menu.getBoundingClientRect();
      let left = e.clientX, top = e.clientY;
      if (left + r.width  > vw - M) left = Math.max(M, vw - r.width  - M);
      if (top  + r.height > vh - M) top  = Math.max(M, vh - r.height - M);
      menu.style.left = left + "px";
      menu.style.top  = top  + "px";
    }
    const dismiss = ev => {
      if (!menu.contains(ev.target)) { menu.remove(); document.removeEventListener("mousedown", dismiss); }
    };
    document.addEventListener("mousedown", dismiss);
    menu.addEventListener("mouseleave", ()=>{ menu.remove(); document.removeEventListener("mousedown", dismiss); });
  });
}

function showTabError(win, msg) {
  const d = win.document.createElement("div");
  d.textContent = msg;
  d.style.cssText = "position:fixed;left:0;right:0;top:0;z-index:99999;" +
    "background:#7f1d1d;color:#fff;font:13px monospace;padding:8px 12px;" +
    "white-space:pre-wrap;";
  win.document.body.appendChild(d);
}

function transferPanelToTab(panel, title, registry, key) {
  const popup = window.open("about:blank", `${key}-tab`);
  if (!popup) { alert("Popup blocked — allow popups for this site to move panels."); return; }

  popup.document.open();
  popup.document.write(
    '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<title>' + title + '</title>' +
    '<style>html,body{margin:0;padding:0;height:100%;background:#0d1117;overflow:hidden;}' +
    'iframe{border:none;width:100%;height:100%;display:block;}</style>' +
    '</head><body></body></html>'
  );
  popup.document.close();

  // Remember the on-canvas size so "Back to canvas" can restore it.
  const _entry0 = registry[key];
  if (_entry0) {
    const r = panel.getBoundingClientRect();
    _entry0.savedW = Math.round(r.width);
    _entry0.savedH = Math.round(r.height);
  }

  panel.style.position = "static";
  panel.style.left = "0"; panel.style.top = "0";
  panel.style.width  = "100vw";
  panel.style.height = "100vh";
  panel.style.borderRadius = "0";
  panel.style.boxShadow = "none";
  popup.document.body.appendChild(panel);

  const entry = registry[key];
  if (entry) { entry.popup = popup; entry.transferred = true; }
  // If the user closes the tab manually, drop the registry entry.
  const _bu = ()=>{ try { delete registry[key]; } catch(e){} };
  if (entry) entry._beforeUnload = _bu;
  popup.addEventListener("beforeunload", _bu);
  try { updatePanelPositions(); } catch(e){}
  try { popup.blur(); } catch(e){}
  try { window.focus(); } catch(e){}
  setTimeout(()=>{ try { popup.blur(); } catch(e){} try { window.focus(); } catch(e){} }, 0);

  // Offer a way back: a button inside the tab that re-docks the panel here.
  addReturnButton(popup, registry, key);
}

function registryByName(name) {
  switch (name) {
    case "ssh":      return g_ssh_panels;
    case "vnc":      return g_vnc_panels;
    case "webshark": return g_webshark_panels;
    case "demo":     return g_demo_panels;
    case "traffic":  return g_traffic_panels;
    default:         return null;
  }
}
function registryName(registry) {
  if (registry === g_ssh_panels)      return "ssh";
  if (registry === g_vnc_panels)      return "vnc";
  if (registry === g_webshark_panels) return "webshark";
  if (registry === g_demo_panels)     return "demo";
  if (registry === g_traffic_panels)  return "traffic";
  return null;
}

function addReturnButton(popup, registry, key) {
  const rname = registryName(registry);
  if (!rname) return;
  let doc;
  try { doc = popup.document; } catch(e){ return; }
  if (!doc || !doc.body) return;

  const btn = doc.createElement("button");
  btn.textContent = "⮌ Back to canvas";
  btn.title = "Re-dock this window onto the topology canvas";
  btn.style.cssText =
    "position:fixed; top:8px; right:8px; z-index:2147483647;" +
    "padding:5px 12px; background:#0f3460; color:#e6edf3;" +
    "border:1px solid #1f4287; border-radius:4px; cursor:pointer;" +
    "font:12px system-ui,sans-serif; box-shadow:0 2px 8px #000a;";
  btn.addEventListener("mouseenter", ()=>{ btn.style.background = "#1f4287"; });
  btn.addEventListener("mouseleave", ()=>{ btn.style.background = "#0f3460"; });
  btn.addEventListener("click", ()=>{
    try { window.returnPanelFromTab(rname, key); }
    catch(e){ try { alert("The main cloonix tab is no longer available."); } catch(_){} }
  });
  doc.body.appendChild(btn);
}

function returnPanelFromTab(rname, key) {
  const registry = registryByName(rname);
  if (!registry) return;
  const entry = registry[key];
  if (!entry || !entry.transferred || !entry.panel) return;

  const panel = entry.panel;
  const popup = entry.popup;

  // Detach the popup's own teardown so closing the tab won't kill the
  // session.
  if (popup) {
    try { if (entry._beforeUnload) popup.removeEventListener("beforeunload", entry._beforeUnload); }
    catch(e){}
  }

  // Adopt the node back into THIS document and re-attach to the canvas wrap.
  const wrap = canvas.parentElement;
  try {
    if (document.adoptNode) document.adoptNode(panel);
  } catch(e){}
  try { wrap.appendChild(panel); } catch(e){}

  // Restore floating geometry.
  const w = entry.savedW || 560;
  const h = entry.savedH || 420;
  panel.style.position    = "absolute";
  panel.style.width       = w + "px";
  panel.style.height      = h + "px";
  panel.style.left        = "40px";
  panel.style.top         = "40px";
  panel.style.right       = "auto";
  panel.style.bottom      = "auto";
  panel.style.borderRadius = "6px";
  panel.style.boxShadow    = "0 8px 24px #000c";

  entry.transferred = false;
  if (entry.panelRef) entry.panelRef.pinned = true;

  // Close the now-empty tab.
  if (popup) { try { popup.close(); } catch(e){} entry.popup = null; }

  // Let the panel type re-bind observers / refit (xterm, iframe, etc.).
  try { if (typeof entry.onReturn === "function") entry.onReturn(); } catch(e){}

  raisePanel(panel);
  try { updatePanelPositions(); } catch(e){}
  try { window.focus(); } catch(e){}
}
window.returnPanelFromTab = returnPanelFromTab;

// VNC panel
let g_vnc_panels = {};  // vmName → { panel, anchorNode, pinned }

function openVncPanel(vmName, url, nodeObj) {
  // One VNC desktop per VM: close any existing one and relaunch fresh.
  if (g_vnc_panels[vmName]) {
    try { g_vnc_panels[vmName].close(); } catch(e){}
    delete g_vnc_panels[vmName];
  }
  const panelRef = { pinned: false };
  const { panel, bar, own } = makePanelShell(
    `Desktop: ${vmName}`,
    ()=>closeVnc(),
    800, 520, panelRef);

  bar.style.background = "#1a0d2e";

  const frame = document.createElement("iframe");
  frame.src = url;
  frame.tabIndex = 0;  // focusable, so clicking the panel can hand it the keyboard
  frame.style.cssText = "flex:1; border:none; width:100%;";
  panel.appendChild(frame);
  own();

  function closeVnc(){
    try { panel.remove(); } catch(e){}
    const e = g_vnc_panels[vmName];
    if (e && e.popup) { try { e.popup.close(); } catch(x){} }
    delete g_vnc_panels[vmName];
    try { updatePanelPositions(); } catch(x){}
  }

  panelBarMenu(bar, url, `vnc-${vmName}`, ()=>closeVnc(),
               ()=> transferPanelToTab(panel, `Desktop: ${vmName}`, g_vnc_panels, vmName));

  g_vnc_panels[vmName] = { panel, anchorNode: nodeObj, vmKey: vmName, popup: null,
                              close: closeVnc, panelRef,
                              onReturn: ()=>{ try { updatePanelPositions(); } catch(e){} },
                              get pinned(){ return panelRef.pinned; } };
  updatePanelPositions();
}
// An air has one capture, opened from the air: raised when already open.
function openAirWebshark(air, lanObj, filter) {
  const p = g_webshark_panels[`${air}_0`];
  if (p && p.panel) {
    raisePanel(p.panel);
    if (filter && p.setFilter) p.setFilter(filter);
    return;
  }
  openWebsharkPanel(air, 0, lanObj, `webshark: air ${air}`, filter);
}

// filter: a display filter the capture opens with (a sniff line of a demo).
function openWebsharkPanel(nodeName, ethNum, nodeObj, airTitle, filter) {
  const key = `${nodeName}_${ethNum}`;
  // One webshark per name_eth: close any existing one and relaunch fresh.
  if (g_webshark_panels[key]) {
    try { g_webshark_panels[key].close(); } catch(e){}
    delete g_webshark_panels[key];
  }

  const snfSock  = `${nodeName}_${ethNum}`;   // pcap FIFO: <snf_dir>/NodeName_ethNum
  const sockName = `sock_${nodeName}_${ethNum}`;   // webshark listen socket
  const url      = `/ws-${sockName}/`;
  const title    = airTitle || `webshark: ${nodeName} eth${ethNum}`;

  // A running PcapRec on this interface is not in the way: the snf has one
  // dumpcap, webshark reads its fifo and the record its socket.
  launchWebshark(nodeName, ethNum, nodeObj, key, snfSock, sockName, url, title,
                 filter);
}

// A small read-only panel shown when a webshark is refused because the main
// record owns this interface's capture socket.
function showWebsharkRefused(nodeName, ethNum, nodeObj, title, text) {
  const key = `${nodeName}_${ethNum}`;
  const panelRef = { pinned: false };
  const { panel, bar } = makePanelShell(title, ()=>closeRefused(), 480, 150, panelRef);
  bar.style.background = "#3a1d1d";

  const msg = document.createElement("div");
  msg.style.cssText =
    "flex:1; display:flex; align-items:center; justify-content:center;" +
    "padding:18px; text-align:center; font:13px monospace; color:#ff9a9a;" +
    "background:#0d1117;";
  msg.textContent = text ||
                    `eth${ethNum} of ${nodeName} is in use by the main record ` +
                    `(PcapRec). Stop the record to use webshark here.`;
  panel.appendChild(msg);

  function closeRefused(){
    try { panel.remove(); } catch(e){}
    delete g_webshark_panels[key];
    try { updatePanelPositions(); } catch(x){}
  }

  panelBarMenu(bar, "", `webshark-${key}`, ()=>closeRefused(), null);

  g_webshark_panels[key] = { panel, anchorNode: nodeObj, vmKey: nodeName, popup: null,
                              close: closeRefused, panelRef,
                              onReturn: ()=>{ try { updatePanelPositions(); } catch(e){} },
                              get pinned(){ return panelRef.pinned; } };
  updatePanelPositions();
}

function launchWebshark(nodeName, ethNum, nodeObj, key, snfSock, sockName, url, title,
                        filter) {
  const panelRef = { pinned: false };
  const { panel, bar } = makePanelShell(title, ()=>closeShark(), 720, 420, panelRef);
  bar.style.background = "#0d2137";

  const frame = document.createElement("iframe");
  frame.src = url;
  frame.style.cssText = "flex:1; border:none; width:100%;";
  panel.appendChild(frame);

  let everConnected = false;
  let closed = false;
  let graceUntil = Date.now() + 12000;   // tolerate up to 12s before first connect

  function onMsg(ev){
    if (!frame.contentWindow || ev.source !== frame.contentWindow) return;
    const d = ev.data;
    if (!d || typeof d !== "object" || !("cloonix_webshark" in d)) return;
    if (d.cloonix_webshark === "connected") {
      if (!everConnected && filter) setFilter(filter);
      everConnected = true;
    } else if (d.cloonix_webshark === "disconnected") {
      if (everConnected || Date.now() > graceUntil)
        closeShark();
    }
  }
  window.addEventListener("message", onMsg);

  // Never running (iframe got a 502, no postMessage at all): give up after the grace.
  // the page applies it as if typed in its Filter box
  function setFilter(f) {
    try { frame.contentWindow.postMessage({ cloonix_webshark_filter: f }, "*"); }
    catch(e){}
  }

  let graceTimer = setTimeout(function(){
    if (!everConnected) closeShark();
  }, 12500);

  function closeShark(reason){
    if (closed) return;
    closed = true;
    try { fetch(`/ctl/stop?sock=${encodeURIComponent(sockName)}`); } catch(e){}
    try { window.removeEventListener("message", onMsg); } catch(e){}
    try { clearTimeout(graceTimer); } catch(e){}
    try { panel.remove(); } catch(e){}
    const e = g_webshark_panels[key];
    if (e && e.popup) { try { e.popup.close(); } catch(x){} }
    delete g_webshark_panels[key];
    try { updatePanelPositions(); } catch(x){}
    if (reason) showWebsharkRefused(nodeName, ethNum, nodeObj, title, reason);
  }

  panelBarMenu(bar, url, `webshark-${key}`, ()=>closeShark(),
               ()=> transferPanelToTab(panel, title, g_webshark_panels, key));

  g_webshark_panels[key] = { panel, anchorNode: nodeObj, vmKey: nodeName, popup: null,
                              close: closeShark, panelRef, setFilter,
                              refused: ()=>closeShark(isAir(nodeName)
                                ? `no capture of the air ${nodeName}: not captured, or its capture not up after 10 s`
                                : `no capture on ${nodeName} eth${ethNum}: no snf, or its snf not up after 10 s`),
                              onReturn: ()=>{ try { updatePanelPositions(); } catch(e){} },
                              get pinned(){ return panelRef.pinned; } };
  updatePanelPositions();

  const startUrl = snfSock
    ? `/ctl/start?sock=${encodeURIComponent(sockName)}&snf_sock=${encodeURIComponent(snfSock)}`
    : `/ctl/start?sock=${encodeURIComponent(sockName)}`;

  fetch('/ctl/wireshark-start', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name:nodeName, num:ethNum})
  })
  .then(()=> fetch(startUrl))
  .catch(()=> fetch(startUrl));  // launch webshark even if RPC fails
}

// Middle-click paste, last selection wins: a web selection is seen when
// made; a host one is deduced at the paste (focus lost, new paste text).
let g_primary_selection = "";
let g_web_at = 0;
let g_away_at = 0;
let g_native_ref = null;
const g_sel_wins = new Set();

function webSelection(text){
  if (!text) return;
  g_primary_selection = text;
  g_web_at = Date.now();
}

function hookSelectionWindow(win){
  if (g_sel_wins.has(win)) return;
  g_sel_wins.add(win);
  win.addEventListener("blur", () => {
    const at = Date.now();
    setTimeout(() => {
      for (const w of g_sel_wins) {
        let focus = false;
        try { focus = !w.closed && w.document.hasFocus(); } catch(e){}
        if (w.closed) g_sel_wins.delete(w);
        if (focus) return;
      }
      g_away_at = at;
    }, 250);
  });
  win.document.addEventListener("selectionchange", () => {
    const a = win.document.activeElement;
    if (a && a.closest && a.closest(".xterm")) return;
    webSelection(String(win.document.getSelection() || ""));
  });
}

// firstCmd: a command typed in the shell once it has shown its prompt (a
// terminal line of the Comments panel).
function openSshPanel(vmName, net, nodeObj, kindOverride, firstCmd) {
  // kind: "ssh" cloonix exec pipe, "shk" qemu serial console, "crun" container shell.
  const kind = kindOverride ? kindOverride
              : nodeObj && nodeObj.type === "kvm" ? "shk"
              : nodeObj && nodeObj.type === "cnt" ? "crun"
              : "ssh";
  const label = kind === "ssh" ? "ssh" : "console";
  const isConsole = (kind !== "ssh");

  // console (shk/crun): at most one per node; ssh: unlimited.
  let panelKey;
  if (isConsole) {
    panelKey = `console:${vmName}`;
    if (g_ssh_panels[panelKey]) {
      try { g_ssh_panels[panelKey].close(); } catch(e){}
      delete g_ssh_panels[panelKey];
    }
  } else {
    panelKey = `ssh:${vmName}:${g_ssh_seq++}`;
  }

  const url   = `/sh-${net}/${kind}/${vmName}`;
  const wsUri = (location.protocol==='https:'?'wss://':'ws://')
                + location.host + url + '/ws';

  const panelRef = { pinned: false };
  const { panel, bar, own } = makePanelShell(
    `${net} / ${vmName} [${label}]`,
    ()=>closePanel(panelKey),
    540, 280, panelRef);

  let termDiv = document.createElement("div");
  termDiv.className = "ssh-panel-term";
  termDiv.style.cssText = "flex:1; padding:2px; min-height:0; overflow:hidden;";
  panel.appendChild(termDiv);

  const TERM_OPTS = {
    cursorBlink:true, fontSize:13,
    fontFamily:"'SF Mono','Fira Code','Cascadia Code',monospace",
    theme:{
      background:'#0d1117', foreground:'#e6edf3', cursor:'#58a6ff',
      selectionBackground:'#2f6feb',
      selectionForeground:'#ffffff',
      selectionInactiveBackground:'#26456e',
      black:'#21262d', red:'#ff7b72', green:'#3fb950', yellow:'#d29922',
      blue:'#388bfd', magenta:'#bc8cff', cyan:'#39c5cf', white:'#b1bac4',
      brightBlack:'#484f58', brightRed:'#ffa198', brightGreen:'#56d364',
      brightYellow:'#e3b341', brightBlue:'#79c0ff', brightMagenta:'#d2a8ff',
      brightCyan:'#56d4dd', brightWhite:'#f0f6fc',
    },
    scrollback:5000,
  };
  let term = new Terminal(TERM_OPTS);
  let fitAddon = new FitAddon.FitAddon();
  term.loadAddon(fitAddon);
  term.open(termDiv);
  fitAddon.fit();
  term.focus();
  panelRef.focus = () => focusTerm(term);
  own();

  let ws=null, destroyed=false, g_popup=null;
  // Reconnect policy: retry a healthy session that drops, but give up when
  // the guest is gone (after "Delete all" or the VM dying).
  const MAX_DEAD_RETRIES = 4;
  const STABLE_MS = 2000;
  let deadRetries = 0, openedAt = 0;
  // the first output of the shell, then a quiet moment: the prompt is there
  let cmdPending = firstCmd || "", cmdTimer = null;
  function cmdSoon(){
    if (!cmdPending) return;
    clearTimeout(cmdTimer);
    cmdTimer = setTimeout(()=>{
      if (cmdPending && ws && ws.readyState === WebSocket.OPEN) {
        ws.send(new TextEncoder().encode(cmdPending + "\r"));
        cmdPending = "";
      }
    }, 300);
  }
  function shellOut(e){
    term.write(e.data instanceof ArrayBuffer ? new Uint8Array(e.data) : e.data);
    cmdSoon();
  }
  function sendResize(){
    if(ws&&ws.readyState===WebSocket.OPEN)
      ws.send(JSON.stringify({cols:term.cols,rows:term.rows}));
  }
  function connect(){
    if(destroyed) return;
    openedAt = 0;
    term.write(`\r\n\x1b[33mConnecting to ${wsUri}\x1b[0m\r\n`);
    ws=new WebSocket(wsUri,['binary']);
    ws.binaryType='arraybuffer';
    ws.onopen =()=>{ openedAt = Date.now();
                     term.write('\x1b[32mConnected\x1b[0m\r\n'); sendResize(); };
    ws.onmessage=shellOut;
    // term is rebound by wireTerm() when the panel changes window.
    ws.onclose=()=>{
      if(destroyed) return;
      const wasStable = (openedAt && (Date.now() - openedAt >= STABLE_MS));
      if(wasStable) deadRetries = 0;
      else          deadRetries++;
      if(deadRetries >= MAX_DEAD_RETRIES){
        term.write('\r\n\x1b[31mGuest unreachable — closing this console.\x1b[0m\r\n');
        setTimeout(()=>closePanel(panelKey), 800);
        return;
      }
      term.write('\r\n\x1b[31mDisconnected — reconnecting in 3s\x1b[0m\r\n');
      setTimeout(connect,3000);
    };
    ws.onerror=()=>term.write('\r\n\x1b[31mWebSocket error\x1b[0m\r\n');
  }
  function wireTerm(){
    term.onData(data=>{
      if(ws&&ws.readyState===WebSocket.OPEN)
        ws.send(new TextEncoder().encode(data));
    });
    term.onResize(()=>sendResize());
    if(ws) ws.onmessage = shellOut;
    wireSelection();
  }

  function wireSelection(){
    term.onSelectionChange(() => webSelection(term.getSelection()));
    hookSelectionWindow(termDiv.ownerDocument.defaultView);

    // The browser pastes on middle release: chosen there; without a paste
    // event the web selection goes PASTE_MS after the release.
    const PASTE_MS = 200;
    let middle = false, timer = null;

    function middleEnd(){ middle = false; clearTimeout(timer); timer = null; }

    termDiv.addEventListener("mousedown", e => {
      middleEnd();
      if (e.button !== 1) return;
      middle = true;
      e.preventDefault();
    });

    termDiv.addEventListener("mouseup", e => {
      if (e.button !== 1 || !middle) return;
      timer = setTimeout(() => {
        middleEnd();
        sendPrimary(g_primary_selection);
      }, PASTE_MS);
    }, true);

    // Released outside the terminal: a keyboard paste stays the browser's.
    termDiv.addEventListener("keydown", middleEnd, true);

    // Capture phase on the container: runs before xterm's own paste listener.
    termDiv.addEventListener("paste", e => {
      if (!middle) return;
      middleEnd();
      let native = "";
      try { native = (e.clipboardData && e.clipboardData.getData("text")) || ""; }
      catch(err){}
      const hostLast = native && native !== g_native_ref && g_away_at > g_web_at;
      g_native_ref = native;
      if (hostLast) g_primary_selection = "";
      if (!g_primary_selection) return;
      e.preventDefault();
      e.stopPropagation();
      sendPrimary(g_primary_selection);
    }, true);
  }

  function sendPrimary(text){
    if (text && ws && ws.readyState === WebSocket.OPEN)
      ws.send(new TextEncoder().encode(text));
  }

  // Plain text of what is on screen and in the scrollback, so a rebuilt
  // terminal does not start blank.
  function snapshotTerm(){
    let out = "";
    try {
      const b = term.buffer.active;
      const first = Math.max(0, b.length - 2000);
      for (let i = first; i < b.length; i++) {
        const ln = b.getLine(i);
        if (ln) out += ln.translateToString(true) + "\r\n";
      }
      out = out.replace(/(\r\n)+$/, "\r\n");
    } catch(e){}
    return out;
  }

  function rebuildTerminalIn(win){
    let nterm = null, nfit = null, ndiv = null;
    try {
      ndiv = win.document.createElement("div");
      ndiv.style.width = "100%";
      ndiv.style.height = "100%";
      termDiv.parentNode.insertBefore(ndiv, termDiv);
      nterm = new win.Terminal(TERM_OPTS);
      nfit  = new win.FitAddon.FitAddon();
      nterm.loadAddon(nfit);
      nterm.open(ndiv);
    }
    catch(e) {
      try { if (ndiv && ndiv.parentNode) ndiv.parentNode.removeChild(ndiv); } catch(e2){}
      const msg = "cloonix: could not rebuild the terminal in this window: " +
                  (e && e.message ? e.message : e);
      try { (win.console || console).error(msg, e); } catch(e2){}
      try { term.write("\r\n\x1b[31m" + msg + "\x1b[0m\r\n"); } catch(e2){}
      try { if (win !== window) showTabError(win, msg); } catch(e2){}
      return false;
    }

    // The new terminal is up.
    const snap = snapshotTerm();
    try { ro.disconnect(); } catch(e){}
    try { term.dispose(); } catch(e){}
    try { termDiv.parentNode.removeChild(termDiv); } catch(e){}
    termDiv = ndiv;
    term = nterm;
    fitAddon = nfit;
    try { fitAddon.fit(); } catch(e){}
    if (snap) { try { term.write(snap); } catch(e){} }
    wireTerm();
    try {
      ro = new win.ResizeObserver(()=>{ try{fitAddon.fit();}catch(e){} sendResize(); });
      ro.observe(termDiv);
    } catch(e){}
    try { term.focus(); } catch(e){}
    sendResize();
    return true;
  }

  wireTerm();
  connect();

  let ro=new ResizeObserver(()=>{ fitAddon.fit(); sendResize(); });
  ro.observe(termDiv);

  // Transfer to a new window, keeping WebSocket + shell session alive
  function transferToWindow(){
    const popup = window.open("about:blank", panelKey.replace(/[:]/g,"-"));
    if (!popup) { alert("Popup blocked — allow popups for this site to move terminals."); return; }
    g_popup = popup;

    popup.document.open();
    popup.document.write(
      '<!DOCTYPE html><html><head><meta charset="utf-8">' +
      '<title>' + g_net + ' / ' + vmName + ' [' + label + ']</title>' +
      '<link rel="stylesheet" href="/xterm/xterm.css">' +
      '<style>html,body{margin:0;padding:0;height:100%;background:#0d1117;overflow:hidden;}' +
      '.xterm{height:100%;}.xterm-viewport{overflow-y:auto;}</style>' +
      '</head><body></body></html>'
    );
    popup.document.close();

    ro.disconnect();

    // Remember the on-canvas size so "Back to canvas" can restore it.
    {
      const _r = panel.getBoundingClientRect();
      const _e0 = g_ssh_panels[panelKey];
      if (_e0) { _e0.savedW = Math.round(_r.width); _e0.savedH = Math.round(_r.height); }
    }

    panel.style.position = "static";
    panel.style.left = "0"; panel.style.top = "0";
    panel.style.width  = "100vw";
    panel.style.height = "100vh";
    panel.style.borderRadius = "0";
    panel.style.boxShadow = "none";
    popup.document.body.appendChild(panel);

    popup.addEventListener("resize", ()=>{ try{fitAddon.fit();}catch(e){} sendResize(); });

    (function loadXtermInTab(){
      const need = ["/xterm/xterm.js", "/xterm/addon-fit.js"];
      let left = need.length;
      const fail = (src, why)=>{
        const msg = "cloonix: " + src + " did not load in the tab (" + why +
                    "), the terminal cannot be rebuilt here";
        try { (popup.console || console).error(msg); } catch(e){}
        try { showTabError(popup, msg); } catch(e){}
      };
      const done = ()=>{
        if (--left) return;
        if (destroyed || popup.closed) return;
        if (!popup.Terminal || !popup.FitAddon) {
          fail("xterm", "loaded but defined no global");
          return;
        }
        rebuildTerminalIn(popup);
      };
      try {
        need.forEach(src=>{
          const sc = popup.document.createElement("script");
          sc.src = src;
          sc.onload  = done;
          sc.onerror = ()=>{ fail(src, "network error"); left = 99; };
          popup.document.head.appendChild(sc);
        });
      }
      catch(e) { fail("xterm", e && e.message ? e.message : e); }
    })();

    const refocusTerm = ()=>{ try { term.focus(); } catch(e){} };
    popup.addEventListener("focus", refocusTerm);
    popup.document.addEventListener("mousedown", refocusTerm, true);
    popup.document.addEventListener("mouseup",
                                    ()=>setTimeout(refocusTerm, 0), true);

    const _bu = ()=>{
      destroyed = true;
      if (ws) { ws.onclose=null; ws.close(); }
      try { ro.disconnect(); } catch(e){}
      term.dispose();
      delete g_ssh_panels[panelKey];   // user closed the tab manually
      g_popup = null;
    };
    popup.addEventListener("beforeunload", _bu);

    const _e = g_ssh_panels[panelKey];
    if (_e) {
      _e.transferred = true;
      _e.popup = popup;
      _e._beforeUnload = _bu;
      // returnPanelFromTab() calls this after re-docking: rebind the xterm
      // ResizeObserver to THIS window and refit the terminal.
      _e.onReturn = ()=>{ setTimeout(()=>rebuildTerminalIn(window), 50); };
    }
    updatePanelPositions();
    // Add the "Back to canvas" button inside the tab.
    addReturnButton(popup, g_ssh_panels, panelKey);
    try { popup.blur(); } catch(e){}
    try { window.focus(); } catch(e){}
    setTimeout(()=>{ try { popup.blur(); } catch(e){} try { window.focus(); } catch(e){} }, 0);
  }

  panelBarMenu(bar, url, panelKey.replace(/[:]/g,"-"), ()=>closePanel(panelKey), transferToWindow);

  function closePanel(name){
    destroyed=true;
    if(ws){ws.onclose=null;ws.close();}
    try { ro.disconnect(); } catch(e){}
    term.dispose();
    panel.remove();
    delete g_ssh_panels[name];
    if (g_popup) { try { g_popup.close(); } catch(e){} g_popup = null; }
    updatePanelPositions();
  }

  g_ssh_panels[panelKey] = { panel, term, fitAddon,
                            anchorNode: nodeObj, vmKey: vmName, panelRef,
                            get pinned() { return panelRef.pinned; },
                            close:()=>closePanel(panelKey) };
  updatePanelPositions();
}
