// cr-traffic.js — live per-interface RX/TX throughput plot ("PlotTraffic").

let g_traffic_panels = {};

const TRAFFIC_PERIOD_MS = 1000;   // one rate sample per second
let g_traffic_accum = {};         // "name_num" -> { tx, rx, last_ms }

function trafficKey(name, num) { return `${name}_${num}`; }

function trafficFeed(name, num, ms, btx, brx) {
  const key = trafficKey(name, num);
  const acc = g_traffic_accum[key];
  if (!acc) return;                       // nobody watching this interface
  acc.tx += (btx | 0);
  acc.rx += (brx | 0);
  if (acc.last_ms === 0) { acc.last_ms = ms; return; }
  const delta = ms - acc.last_ms;
  if (delta < TRAFFIC_PERIOD_MS) return;
  // bytes accumulated over `delta` ms -> bytes/second
  const rateTx = acc.tx * 1000 / delta;
  const rateRx = acc.rx * 1000 / delta;
  acc.tx = 0; acc.rx = 0; acc.last_ms = ms;
  // Feed the rate sample to every plot open on this interface.
  const entry = g_traffic_panels[key];
  if (entry && typeof entry.pushSample === "function")
    entry.pushSample(ms, rateTx, rateRx);
}

// Byte-rate formatting for the Y axis / legend
function fmtRate(bps) {
  if (bps < 1)        return "0";
  const u = ["B/s","kB/s","MB/s","GB/s"];
  let i = 0, v = bps;
  while (v >= 1024 && i < u.length-1) { v /= 1024; i++; }
  return (v >= 100 ? v.toFixed(0) : v >= 10 ? v.toFixed(1) : v.toFixed(2)) + " " + u[i];
}

const TRAFFIC_WINDOW_S = 60;      // sliding window width in seconds
const COL_RX = "#3fb950";         // green
const COL_TX = "#58a6ff";         // blue
const COL_GRID = "#21262d";
const COL_AXIS = "#8b949e";
const COL_BG   = "#0d1117";

function makeTrafficPlot(getCanvas) {
  // getCanvas() returns the live <canvas> (it may be relocated into a popup
  // tab; we re-query each frame so drawing keeps working after a move).
  const series = [];
  let maxRate = 1;            // current Y scale (bytes/s), grows/shrinks smoothly
  let lastT = 0;

  function push(tMs, tx, rx) {
    const t = tMs / 1000;
    lastT = t;
    series.push({ t, tx, rx });
    // drop points older than the window
    const cutoff = t - TRAFFIC_WINDOW_S;
    while (series.length && series[0].t < cutoff) series.shift();
  }

  function draw() {
    const cv = getCanvas();
    if (!cv) return;
    const rect = cv.getBoundingClientRect();
    const dpr = (cv.ownerDocument.defaultView || window).devicePixelRatio || 1;
    const w = Math.max(80, Math.round(rect.width));
    const h = Math.max(60, Math.round(rect.height));
    if (cv.width !== Math.round(w*dpr) || cv.height !== Math.round(h*dpr)) {
      cv.width = Math.round(w*dpr); cv.height = Math.round(h*dpr);
    }
    const g = cv.getContext("2d");
    g.setTransform(dpr,0,0,dpr,0,0);
    g.clearRect(0,0,w,h);
    g.fillStyle = COL_BG; g.fillRect(0,0,w,h);

    const padL = 64, padR = 10, padT = 22, padB = 18;
    const plotW = Math.max(1, w - padL - padR);
    const plotH = Math.max(1, h - padT - padB);

    let peak = 1;
    for (const s of series) { if (s.tx > peak) peak = s.tx; if (s.rx > peak) peak = s.rx; }
    const target = peak * 1.2;
    maxRate += (target - maxRate) * 0.15;
    if (maxRate < 1) maxRate = 1;

    const tEnd   = lastT || 0;
    const tStart = tEnd - TRAFFIC_WINDOW_S;
    const xOf = t => padL + ((t - tStart) / TRAFFIC_WINDOW_S) * plotW;
    const yOf = r => padT + plotH - (r / maxRate) * plotH;

    // grid + Y tick labels (4 rows)
    g.strokeStyle = COL_GRID; g.lineWidth = 1;
    g.fillStyle = COL_AXIS;
    g.font = "10px monospace"; g.textBaseline = "middle";
    for (let i = 0; i <= 4; i++) {
      const yy = padT + plotH - (i/4)*plotH;
      g.beginPath(); g.moveTo(padL, yy); g.lineTo(w-padR, yy); g.stroke();
      g.textAlign = "right";
      g.fillText(fmtRate(maxRate * i/4), padL-6, yy);
    }

    function plotSeries(field, color) {
      if (series.length < 2) return;
      g.strokeStyle = color; g.lineWidth = 1.5;
      g.beginPath();
      let started = false;
      for (const s of series) {
        const x = xOf(s.t), y = yOf(s[field]);
        if (!started) { g.moveTo(x,y); started = true; } else g.lineTo(x,y);
      }
      g.stroke();
    }
    plotSeries("rx", COL_RX);
    plotSeries("tx", COL_TX);

    // legend with current instantaneous values
    const cur = series.length ? series[series.length-1] : { tx:0, rx:0 };
    g.textBaseline = "alphabetic"; g.textAlign = "left";
    g.fillStyle = COL_RX; g.fillText("● RX " + fmtRate(cur.rx), padL+4, 14);
    const rxW = g.measureText("● RX " + fmtRate(cur.rx)).width;
    g.fillStyle = COL_TX; g.fillText("● TX " + fmtRate(cur.tx), padL+4+rxW+18, 14);
  }

  return { push, draw, series };
}

function openTrafficPanel(nodeName, ethNum, nodeObj) {
  const key = trafficKey(nodeName, ethNum);
  if (g_traffic_panels[key]) {
    try { g_traffic_panels[key].close(); } catch(e){}
    delete g_traffic_panels[key];
  }

  // Start (or reuse) the rate accumulator for this interface.
  if (!g_traffic_accum[key]) g_traffic_accum[key] = { tx:0, rx:0, last_ms:0 };

  const title = `traffic: ${nodeName} eth${ethNum}`;
  const panelRef = { pinned: false };
  const { panel, bar } = makePanelShell(title, ()=>closePlot(), 480, 300, panelRef);
  bar.style.background = "#0d2a1a";   // greenish bar to distinguish from webshark

  // Plot canvas fills the panel body.
  const cv = document.createElement("canvas");
  cv.style.cssText = "flex:1; width:100%; height:100%; display:block; background:#0d1117;";
  panel.appendChild(cv);

  const plot = makeTrafficPlot(()=>cv);

  // ~10 fps render loop (cheap; data only changes ~1/s but resize/anim needs
  // it)
  let raf = 0, alive = true;
  function loop() {
    if (!alive) return;
    try { plot.draw(); } catch(e){}
    raf = requestAnimationFrame(()=>setTimeout(loop, 100));
  }
  loop();

  function closePlot() {
    alive = false;
    if (raf) cancelAnimationFrame(raf);
    try { panel.remove(); } catch(e){}
    const e = g_traffic_panels[key];
    if (e && e.popup) { try { e.popup.close(); } catch(x){} }
    delete g_traffic_panels[key];
    delete g_traffic_accum[key];
    try { updatePanelPositions(); } catch(x){}
  }

  panelBarMenu(bar, "", `traffic-${key}`, ()=>closePlot(),
               ()=> transferPanelToTab(panel, title, g_traffic_panels, key));

  g_traffic_panels[key] = {
    panel, canvas: cv, anchorNode: nodeObj, vmKey: nodeName, popup: null,
    close: closePlot, panelRef, transferred: false,
    pushSample: (ms, tx, rx) => plot.push(ms, tx, rx),
    onReturn: ()=>{ try { updatePanelPositions(); } catch(e){} },
    get pinned(){ return panelRef.pinned; },
  };
  updatePanelPositions();
}
