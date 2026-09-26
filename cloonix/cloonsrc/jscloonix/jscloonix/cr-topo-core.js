// cr-topo-core.js — geometry/state globals, helpers, colours, activity and
// the force-directed layout simulation.

"use strict";

// Geometry constants
const NODE_R  = 38;   // KVM circle radius
const CNT_R   = 28;   // Container circle radius
const SPECIAL_R = 14; // tap / nat / a2b / c2c / phy — half-size; VMs/CNTs dominate
const ETH_R   = 12;   // Interface dot radius (same as LAN)
const LAN_R   = 12;   // LAN dot radius
const ETH_GAP =  2;   // gap between node circle edge and eth dot centre
const MAX_ETH =  5;   // max eth shown in dialog
const INNER_R =  5;   // inner RX indicator dot radius

// Canvas / context
let canvas, ctx, statusEl;

const g_tune = {
  REPEL:     1000000,  // node/lan mutual repulsion strength (1/dist^2)
  SPRING:    0.9,      // edge (node<->lan) attraction stiffness
  DAMP:      0.88,     // velocity retained per step (higher = less braking)
  VEL_CAP_F: 0.7,      // max per-step speed, as a fraction of NODE_R
  L0_F:      1.15,     // natural link length, as a multiple of NODE_R
  SPRING_MUL_MAX: 12,  // super-linear pull cap for long (stretched) edges
  SETTLE_KE: 0.008,    // avg speed^2 below which the layout idles (rest)
};

// Zoom / pan state
let g_zoom   = 1.0;
let g_pan_x  = 0;
let g_pan_y  = 0;
const ZOOM_MIN = 0.15;
const ZOOM_MAX = 8.0;

let g_has_fitted = false;        // becomes true after the first fit completes
let g_demo_view = false;         // a demo plays: the fit puts the web at the bottom
let g_fit_anim = null;           // { z0,px0,py0, z1,px1,py1, t, dur } while easing

// Start easing the view transform toward (z1, px1, py1) over `frames` frames.
function startFitAnim(z1, px1, py1, frames) {
  g_fit_anim = {
    z0: g_zoom, px0: g_pan_x, py0: g_pan_y,
    z1: z1, px1: px1, py1: py1,
    t: 0, dur: Math.max(1, frames || 24)
  };
}

function stepFitAnim() {
  if (!g_fit_anim) return false;
  const a = g_fit_anim;
  a.t++;
  let u = a.t / a.dur;
  if (u >= 1) u = 1;
  const e = u*u*(3 - 2*u);          // smoothstep
  g_zoom  = a.z0  + (a.z1  - a.z0)  * e;
  g_pan_x = a.px0 + (a.px1 - a.px0) * e;
  g_pan_y = a.py0 + (a.py1 - a.py0) * e;
  if (a.t >= a.dur) { g_fit_anim = null; return false; }
  return true;
}

// Convert screen coordinates to world coordinates
function screenToWorld(sx, sy) {
  return { x: (sx - g_pan_x) / g_zoom,
           y: (sy - g_pan_y) / g_zoom };
}
// Apply the current transform to ctx before drawing
function applyTransform() {
  ctx.setTransform(g_zoom, 0, 0, g_zoom, g_pan_x, g_pan_y);
}

// Topology state
let g_nodes = [];   // { name, type:"kvm"|"cnt", nb_eth, x, y, r }
let g_lans  = [];   // { name, x, y }
let g_edges = [];   // { name, num, lan }
let g_net   = "";   // cloonix network name (from topo.net)
let g_last_topo = null;  // last real topo from server — used to re-apply with pending LANs

// UI state
let g_selected_lan = null;
let dragging = null, dragOffX = 0, dragOffY = 0;
let g_click_x = -1, g_click_y = -1;
let g_pending_lans = [];

let g_saved_positions = {};

let g_eth_angles = {};

function ethAnglePinned(name, num) {
  const e = g_eth_angles[name];
  if (!e) return null;
  const d = e[num];
  return (typeof d === "number") ? (d - 90) * Math.PI / 180 : null;
}

function applyItemPos(name, x, y) {
  const it = g_nodes.find(n => n.name === name) ||
             g_lans.find(l => l.name === name);
  if (!it) { g_saved_positions[name] = { x: x, y: y }; return; }
  it.x = x; it.y = y; it.vx = 0; it.vy = 0; it._locked = true;
  // The neighbours must give way to where it now sits.
  reviveLayout(120);
  draw();
}

function reportItemPos(it) {
  if (!it || !it.name) return;
  post("/ctl/position", { name: it.name,
                          x: Math.round(it.x), y: Math.round(it.y) });
}

// An air is a lan of wlans: an eth goes on a lan, a wlan on an air.
function isWlan(nd, i) { return !!(nd && nd.etype && nd.etype[i] === "wlan"); }
function lanKind(name) {
  const e = g_edges.find(x => x.lan === name);
  if (!e) return "";
  return isWlan(g_nodes.find(n => n.name === e.name), e.num) ? "air" : "lan";
}
function isAir(name) { return lanKind(name) === "air"; }
function airOf(nd, i) {
  const e = g_edges.find(x => x.name === nd.name && x.num === i);
  return e ? e.lan : null;
}
function wlanIdx(nd, i) {
  let n = 0;
  for (let k = 0; k < i; k++) if (isWlan(nd, k)) n++;
  return n;
}

function reportNodeEths(nd) {
  if (!nd || !nd.name || !nd.nb_eth) return;
  for (let i = 0; i < nd.nb_eth; i++) {
    if (isWlan(nd, i)) continue;
    const p = ethPos(nd, i);
    let deg = Math.round(Math.atan2(p.y - nd.y, p.x - nd.x) * 180 / Math.PI) + 90;
    deg = ((deg % 360) + 360) % 360;
    if (!g_eth_angles[nd.name]) g_eth_angles[nd.name] = {};
    g_eth_angles[nd.name][i] = deg;
    post("/ctl/poseth", { name: nd.name, num: i, deg: deg });
  }
}

function applyItemPosEth(name, num, deg) {
  if (!g_eth_angles[name]) g_eth_angles[name] = {};
  g_eth_angles[name][num] = deg;
  reviveLayout(120);
  draw();
}

// Image lists
let g_images = { kvm: [], cnt: [] };

let g_kvm_cfg = {
  name: "Cloon", counter: 1, image: "",
  cpu: 4, mem: 4000,
  // off by default: it only suits a windows image, and ticking it for a linux
  // one gives an ide rootfs and a firmware that image did not ask for
  windows: 0,
  // off by default too: an ios-xe image has no agent to answer, and ticking
  // it for a linux one leaves that guest with no agent channel at all
  cisco: 0,
  // and off by default: it gives the guest the freebsd agent disc, whose
  // binaries a linux guest cannot exec, so its node would stay red saying
  // nothing
  freebsd: 0,
  // radios, wlan0.. of the guest, on the air kvmwifi
  nb_vwif: 0,
  nb_eth: 3,
};
let g_cnt_cfg = {
  name: "Cnt", counter: 1, brandtype: "brandcnt", image: "",
  nb_eth: 3,
  nb_vwif: 0, persistent: 0, startup_env: "NODE_ID=1", vmount: "",
};
let g_lan_counter = 1;
let g_tap_counter = 1;
let g_nat_counter = 1;
let g_a2b_counter = 1;
let g_c2c_counter = 1;
let g_brand = "brandkvm";

// Helpers
function post(url, body, onOk) {
  fetch(url, { method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify(body) })
    .then(r => { if (!r.ok) r.text().then(t => alert(url+" error: "+t));
                 else if (onOk) onOk(); })
    .catch(e => alert("Network error: " + e));
}

function snfSet(name, num, on) {
  fetch("/ctl/snf", { method:"POST", headers:{"Content-Type":"application/json"},
    body: JSON.stringify({ name, num, on }) })
    .then(r => {
      if (r.ok) return;
      return r.json().then(j => { throw new Error((j && j.error) || ("snf error ("+r.status+")")); })
                     .catch(err => { throw (err instanceof Error ? err : new Error("snf error ("+r.status+")")); });
    })
    .catch(e => alert(e.message || ("Network error: " + e)));
}

function ethPos(nd, i) {
  if (nd.nb_eth === 0) return { x: nd.x, y: nd.y };
  const er = (nd.r <= SPECIAL_R) ? 7 : ETH_R;
  const ringR = nd.r + er + ETH_GAP;
  let angle;
  const pin = ethAnglePinned(nd.name, i);
  const edge = (typeof g_edges !== "undefined")
             ? g_edges.find(e => e.name === nd.name && e.num === i) : null;
  const lan  = edge && (typeof g_lans !== "undefined")
             ? g_lans.find(l => l.name === edge.lan) : null;
  if (pin !== null) {
    angle = pin;
  } else if (lan) {
    // Point the eth dot at the LAN.
    angle = Math.atan2(lan.y - nd.y, lan.x - nd.x);
  } else {
    const rot = nd.ethRot || 0;
    angle = (2 * Math.PI * i) / nd.nb_eth - Math.PI / 2 + rot;
  }
  return { x: nd.x + ringR * Math.cos(angle),
           y: nd.y + ringR * Math.sin(angle) };
}

// VM/CNT state colours (from topo_small_event)
const VM_COLOR = {
  down:    { fill:"#804d4d", stroke:"#a06060" },
  booting: { fill:"#e61a1a", stroke:"#ff3333" },
  up:      { fill:"#8080ff", stroke:"#9999ff" },
};
// Before any event: dark red (same as "down") — qemu not yet started
const CNT_COLOR_DEFAULT = { fill:"#804d4d", stroke:"#a06060" };

let g_vm_states = {};  // name → "down" | "booting" | "up"
// g_activity[name][num] = { tx: bool, rx: bool } — cleared after FLASH_MS
const FLASH_MS = 180;
let g_activity = {};   // { "Cloon1": { 0: {tx,rx}, 1: {tx,rx} } }

function setActivity(name, num, ptx, prx) {
  if (!g_activity[name]) g_activity[name] = {};
  const prev = g_activity[name][num] || { tx: false, rx: false };
  g_activity[name][num] = { tx: ptx || prev.tx, rx: prx || prev.rx };
  // Auto-clear after FLASH_MS
  setTimeout(() => {
    if (g_activity[name]) {
      delete g_activity[name][num];
      if (Object.keys(g_activity[name]).length === 0)
        delete g_activity[name];
    }
    draw();
  }, FLASH_MS);
  draw();
}

function getActivity(name, num) {
  return (g_activity[name] && g_activity[name][num]) || null;
}

// Per-node RAM/CPU gauge levels.
let g_usage = {};
function setUsage(name, ram, cpu) {
  g_usage[name] = { ram: ram|0, cpu: cpu|0 };
}
function getUsage(name) {
  return g_usage[name] || null;
}

function clearServerState() {
  g_nodes = [];
  g_lans = [];
  g_edges = [];
  g_net = "";
  g_last_topo = null;
  g_pending_lans = [];
  g_vm_states = {};
  g_activity = {};
  g_usage = {};
  g_images = { kvm: [], cnt: [] };
  g_selected_lan = null;
  // Only names declared in a file cloonix.html actually loads may be cleared
  // here.
  try { if (typeof panels_close_all === "function") panels_close_all(); } catch(e) {}
  try { draw(); } catch(e) {}
}

let g_layout_anim_id = null;
let g_layout_settled = false;  // true once motion has eased to rest
let g_layout_wake = 0;         // frames to stay awake (set on drag-release or
                               // a tuning change); decremented each frame

function reviveLayout(frames) {
  g_layout_settled = false;
  g_layout_wake = Math.max(g_layout_wake, frames || 90);
}

function forceLayout(nodes, lans, edges) {
  if (nodes.length + lans.length < 2) return;

  const items  = [...nodes, ...lans];
  const MIN_DIST = NODE_R * 2;
  const FSCALE = 0.0006;
  // "Horizon": a soft, weak centring pull that only kicks in far from origin.
  const HZ1 = (NODE_R*NODE_R)*60, HZ2 = (NODE_R*NODE_R)*200, HZ3 = (NODE_R*NODE_R)*500;

  // Give tiny random jitter to break exact overlaps
  items.forEach(it => {
    it.vx = 0; it.vy = 0;
    if (!it._jittered) {
      it.x += (Math.random()-0.5)*4;
      it.y += (Math.random()-0.5)*4;
      it._jittered = true;
    }
  });

  const nodeByName = new Map();
  nodes.forEach(n => nodeByName.set(n.name, n));
  const lanByName  = new Map();
  lans.forEach(l => lanByName.set(l.name, l));

  const idx = new Map();
  items.forEach((it,i) => idx.set(it, i));
  const adj = items.map(() => []);
  edges.forEach(edge => {
    const nd  = nodeByName.get(edge.name);
    const lan = lanByName.get(edge.lan);
    if (!nd || !lan) return;
    const ni = idx.get(nd), li = idx.get(lan);
    if (ni===undefined || li===undefined) return;
    adj[ni].push(li);
    adj[li].push(ni);
  });
  // BFS from each item to get graph distance to all others
  const graphDist = items.map((_, start) => {
    const dist = new Array(items.length).fill(Infinity);
    dist[start] = 0;
    const queue = [start];
    while (queue.length) {
      const cur = queue.shift();
      for (const nb of adj[cur]) {
        if (dist[nb] === Infinity) { dist[nb] = dist[cur] + 1; queue.push(nb); }
      }
    }
    return dist;
  });

  // Cancel any in-progress layout animation so they don't fight each other
  if (g_layout_anim_id !== null) {
    cancelAnimationFrame(g_layout_anim_id);
    g_layout_anim_id = null;
  }
  g_layout_settled = false;

  function step() {
    if (g_layout_settled && !dragging && g_layout_wake <= 0) {
      g_layout_anim_id = requestAnimationFrame(step);
      return;
    }
    if (g_layout_wake > 0) g_layout_wake--;

    const REPEL     = g_tune.REPEL;
    const SPRING    = g_tune.SPRING;
    const DAMP      = g_tune.DAMP;
    const L0        = NODE_R * g_tune.L0_F;
    const VEL_CAP   = NODE_R * g_tune.VEL_CAP_F;
    const SETTLE_KE = g_tune.SETTLE_KE;
    const SMAX      = g_tune.SPRING_MUL_MAX;
    // Super-linear spring multiplier: an edge stretched past L0 pulls
    // progressively harder, up to SMAX past 2*L0.
    const springMul = (d) => {
      if (d > 2.0*L0)  return SMAX;
      if (d > 1.7*L0)  return Math.max(1, SMAX*0.67);
      if (d > 1.5*L0)  return Math.max(1, SMAX*0.42);
      if (d > 1.3*L0)  return Math.max(1, SMAX*0.25);
      if (d > 1.15*L0) return Math.max(1, SMAX*0.13);
      return 1;
    };

    items.forEach(it => { it.fx = 0; it.fy = 0; });
    nodes.forEach(nd => { nd._ethRotV = 0; });

    const SEP = NODE_R * 3;        // clear gap enforced between any two items
    const OVERLAP_REPEL = 90000;   // short-range anti-overlap strength
    for (let i = 0; i < items.length; i++) {
      for (let j = i+1; j < items.length; j++) {
        const a = items[i], b = items[j];
        const dx = b.x - a.x, dy = b.y - a.y;
        const dist = Math.max(Math.sqrt(dx*dx + dy*dy), MIN_DIST);
        const nx = dx/dist, ny = dy/dist;
        const gd = graphDist[i][j];
        if (isFinite(gd)) {
          // full-strength Coulomb push, no graph-distance attenuation
          const force = REPEL / (dist * dist);
          a.fx -= force*nx; a.fy -= force*ny;
          b.fx += force*nx; b.fy += force*ny;
        } else if (dist < SEP) {
          // different components: only keep them from physically overlapping
          const over = (SEP - dist) / SEP;
          const of = OVERLAP_REPEL * over / dist;
          a.fx -= of*nx; a.fy -= of*ny;
          b.fx += of*nx; b.fy += of*ny;
        }
      }
    }

    const ROT_SPRING = 0.18;
    edges.forEach(edge => {
      const nd  = nodeByName.get(edge.name);
      const lan = lanByName.get(edge.lan);
      if (!nd || !lan) return;
      const nb = nd.nb_eth || 1;
      const er = (nd.r <= SPECIAL_R) ? 7 : ETH_R;
      const ringR = nd.r + er + ETH_GAP;
      const slotAngle = (2*Math.PI*edge.num)/nb - Math.PI/2;
      const rot = nd.ethRot || 0;
      const pin = ethAnglePinned(nd.name, edge.num);
      const angle = (pin !== null) ? pin : slotAngle + rot;
      const ax = nd.x + ringR*Math.cos(angle);
      const ay = nd.y + ringR*Math.sin(angle);
      const dx = lan.x - ax, dy = lan.y - ay;
      const dist = Math.sqrt(dx*dx + dy*dy) || 1;
      const stretch = dist - L0;
      const mul = springMul(dist);
      const fx = SPRING * stretch * mul * dx/dist;
      const fy = SPRING * stretch * mul * dy/dist;
      nd.fx += fx;  nd.fy += fy;
      lan.fx -= fx; lan.fy -= fy;

      if (pin === null) {
        const targetAngle = Math.atan2(lan.y - nd.y, lan.x - nd.x);
        let angDiff = (targetAngle - slotAngle) - rot;
        // normalize to [-pi, pi]
        angDiff = ((angDiff + Math.PI) % (2*Math.PI) + 2*Math.PI) % (2*Math.PI) - Math.PI;
        nd._ethRotV = (nd._ethRotV || 0) + ROT_SPRING * angDiff;
      }
    });

    // Soft "horizon" centring: a weak pull back toward origin that only acts
    // far out.
    items.forEach(it => {
      const hf = it.x*it.x + it.y*it.y;
      if      (hf > HZ3) { it.fx -= it.x*0.08; it.fy -= it.y*0.08; }
      else if (hf > HZ2) { it.fx -= it.x*0.03; it.fy -= it.y*0.03; }
      else if (hf > HZ1) { it.fx -= it.x*0.01; it.fy -= it.y*0.01; }
    });

    items.forEach(it => {
      if (dragging && dragging.x !== undefined && it === dragging) {
        it.vx = 0; it.vy = 0; return;
      }
      if (it._locked) { it.vx = 0; it.vy = 0; return; }
      it.vx = (it.vx + it.fx * FSCALE) * DAMP;
      it.vy = (it.vy + it.fy * FSCALE) * DAMP;
      const speed = Math.sqrt(it.vx*it.vx + it.vy*it.vy);
      if (speed > VEL_CAP) { it.vx *= VEL_CAP/speed; it.vy *= VEL_CAP/speed; }
      it.x += it.vx; it.y += it.vy;
    });

    // Integrate eth-ring rotation (nodes only)
    nodes.forEach(nd => {
      if (nd._ethRotV === undefined) return;
      nd._ethRotV *= DAMP;
      nd.ethRot = (nd.ethRot || 0) + nd._ethRotV;
    });

    draw();

    let ke = 0;
    items.forEach(it => { ke += it.vx*it.vx + it.vy*it.vy; });
    const avgKe = ke / items.length;
    if (avgKe < SETTLE_KE && !dragging && g_layout_wake <= 0) {
      items.forEach(it => { it.vx = 0; it.vy = 0; });
      if (!g_has_fitted && !g_fit_anim && canvas) {
        let minx=Infinity,miny=Infinity,maxx=-Infinity,maxy=-Infinity;
        // A demo sends every position before its first item: the fit is on
        // the whole drawing, the items to come included.
        const spots = g_demo_view
          ? items.concat(Object.values(g_saved_positions)) : items;
        for (const it of spots) {
          if (it.x<minx) minx=it.x; if (it.x>maxx) maxx=it.x;
          if (it.y<miny) miny=it.y; if (it.y>maxy) maxy=it.y;
        }
        const pad = NODE_R * 3;
        const ww = (maxx-minx) + pad*2, hh = (maxy-miny) + pad*2;
        const vw = canvas.width || 800, vh = canvas.height || 600;
        if (isFinite(ww) && isFinite(hh) && ww>0 && hh>0) {
          const z = Math.min(vw/ww, vh/hh);            // fit whole web in view
          const z1 = Math.max(0.05, Math.min(1, z));   // never zoom IN past 1:1
          const cxw = (minx + maxx)/2, cyw = (miny + maxy)/2;
          const px1 = vw/2 - z1 * cxw;
          // Under the Demo and History panels: the low edge of the drawing
          // near the low edge of the view; a tall one goes behind the panels.
          const py1 = g_demo_view ? vh - z1 * (maxy + NODE_R * 2)
                                  : vh/2 - z1 * cyw;
          startFitAnim(z1, px1, py1, 24);
        }
      }
      if (stepFitAnim()) {
        draw();
        g_layout_settled = false;
      } else {
        g_has_fitted = true;
        g_layout_settled = true;   // idle until a drag wakes us
      }
    } else {
      g_layout_settled = false;
    }
    g_layout_anim_id = requestAnimationFrame(step);
  }

  g_layout_anim_id = requestAnimationFrame(step);
}

