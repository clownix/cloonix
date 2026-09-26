// cr-topo-draw.js — topology application (server topo -> scene), canvas
// drawing (nodes/lans/edges) and hit-testing.

// Topo application
function applyTopo(topo, fromServer) {
  if (topo.net) g_net = topo.net;
  // Save real server topo so pending-LAN merges don't lose node data
  if (fromServer) g_last_topo = topo;
  if (fromServer && !(topo.kvm||[]).length && !(topo.cnt||[]).length &&
      !(topo.lan||[]).length && !(topo.endp||[]).length) {
    g_has_fitted = false;
    g_fit_anim = null;
  }

  // Snapshot click position before anything resets it
  const cx = g_click_x >= 0 ? g_click_x : null;
  const cy = g_click_y >= 0 ? g_click_y : null;
  const cxDef = cx ?? (canvas ? canvas.width/2  + (Math.random()-0.5)*40 : 400);
  const cyDef = cy ?? (canvas ? canvas.height/2 + (Math.random()-0.5)*40 : 300);
  g_click_x = g_click_y = -1;

  let g_spawn_i = 0;
  const PHI = Math.PI * (3 - Math.sqrt(5)); // golden angle -> even spiral
  function spawnPos(name) {
    if (name && g_saved_positions && g_saved_positions[name]) {
      const sp = g_saved_positions[name];
      delete g_saved_positions[name];
      return { x: sp.x, y: sp.y, locked: true };
    }
    if (cx != null && cy != null) return { x: cx + (Math.random()-0.5)*40,
                                           y: cy + (Math.random()-0.5)*40 };
    const k = g_spawn_i++;
    // Tight spiral so 1/dist^2 repulsion is strong from the start.
    const ring = NODE_R * 0.6;           // spacing between successive bodies
    const rad  = ring * Math.sqrt(k + 0.5);
    const ang  = k * PHI;
    return { x: cxDef + rad * Math.cos(ang),
             y: cyDef + rad * Math.sin(ang) };
  }

  // Nodes
  const keepN = {};
  g_nodes.forEach(nd => { keepN[nd.name] = nd; });
  const nextN = [];
  (topo.kvm||[]).forEach(k => {
    const ex = keepN[k.name];
    if (ex) {
      ex.nb_eth = k.nb_eth; ex.vm_id = k.vm_id; ex.vhost = k.vhost||[]; ex.etype = k.etype||[];
      ex.cpu = k.cpu; ex.mem = k.mem; ex.rootfs = k.rootfs; ex.vm_config_flags = k.vm_config_flags;
      ex.rootfs_used = k.rootfs_used; ex.rootfs_backing = k.rootfs_backing;
      nextN.push(ex);
    }
    else { const p = spawnPos(k.name);
           nextN.push({ name:k.name, type:"kvm", nb_eth:k.nb_eth, vm_id:k.vm_id,
                      vhost:k.vhost||[], etype:k.etype||[], x:p.x, y:p.y, r:NODE_R,
                      cpu:k.cpu, mem:k.mem, rootfs:k.rootfs, vm_config_flags:k.vm_config_flags,
                      rootfs_used:k.rootfs_used, rootfs_backing:k.rootfs_backing,
                      _locked:!!p.locked }); }
  });
  (topo.cnt||[]).forEach(c => {
    const ex = keepN[c.name];
    if (ex) {
      ex.nb_eth = c.nb_eth; ex.vhost = c.vhost||[]; ex.etype = c.etype||[];
      ex.vm_id = c.vm_id; ex.brandtype = c.brandtype; ex.image = c.image; ex.persistent = c.persistent;
      nextN.push(ex);
    }
    else { const p = spawnPos(c.name);
           nextN.push({ name:c.name, type:"cnt", nb_eth:c.nb_eth,
                      vhost:c.vhost||[], etype:c.etype||[], x:p.x, y:p.y, r:CNT_R,
                      vm_id:c.vm_id, brandtype:c.brandtype, image:c.image, persistent:c.persistent,
                      _locked:!!p.locked }); }
  });
  (topo.endp||[]).forEach(sp => {
    const neth = (sp.kind === "a2b") ? 2 : 1;
    const ex = keepN[sp.name];
    if (ex) {
      ex.type = sp.kind; ex.nb_eth = neth; ex.r = SPECIAL_R;
      ex.etype = sp.etype || []; ex.phykind = sp.phykind;
      ex.tcp_peered = sp.tcp_peered; ex.udp_peered = sp.udp_peered;
      nextN.push(ex);
    }
    else { const p = spawnPos(sp.name);
           nextN.push({ name:sp.name, type:sp.kind, nb_eth:neth,
                      vhost:[], etype:sp.etype||[], x:p.x, y:p.y, r:SPECIAL_R,
                      tcp_peered:sp.tcp_peered, udp_peered:sp.udp_peered,
                      phykind:sp.phykind, _locked:!!p.locked }); }
  });
  g_nodes = nextN;

  // LANs
  const keepL = {};
  g_lans.forEach(l => { keepL[l.name] = l; });
  const nextL = [];
  (topo.lan||[]).forEach(name => {
    if (keepL[name]) nextL.push(keepL[name]);
    else { const p = spawnPos(name); nextL.push({ name, x: p.x, y: p.y, _locked:!!p.locked }); }
  });
  if (fromServer) {
    const realLan = new Set(topo.lan || []);
    const lanHasEdge = new Set((topo.edge || []).map(e => e.lan));
    g_pending_lans = g_pending_lans.filter(
      name => !realLan.has(name) && !lanHasEdge.has(name));
  }
  g_pending_lans.forEach(name => {
    if (!nextL.find(l => l.name === name)) {
      if (keepL[name]) nextL.push(keepL[name]);
      else { const p = spawnPos(name); nextL.push({ name, x: p.x, y: p.y, _locked:!!p.locked }); }
    }
  });
  if (g_selected_lan && !nextL.find(l => l.name === g_selected_lan))
    g_selected_lan = null;
  g_lans = nextL;

  const oldEdgeKey = g_edges.map(e => `${e.name}:${e.num}:${e.lan}`).sort().join("|");
  const newEdges   = topo.edge || [];
  const newEdgeKey = newEdges.map(e => `${e.name}:${e.num}:${e.lan}`).sort().join("|");
  const edgeChanged = (oldEdgeKey !== newEdgeKey);
  g_edges = newEdges;

  g_edges.forEach(e => {
    if (e.lan && !g_lans.find(l => l.name === e.lan)) {
      const kept = keepL[e.lan];
      if (kept) g_lans.push(kept);
      else { const p = spawnPos(e.lan); g_lans.push({ name:e.lan, x:p.x, y:p.y, _locked:!!p.locked }); }
    }
  });

  const nodeAdded = nextN.some(nd => !keepN[nd.name]);
  const lanAdded  = nextL.some(l  => !keepL[l.name]);
  const nodeRemoved = Object.keys(keepN).some(nm => !nextN.find(nd => nd.name === nm));
  const lanRemoved  = Object.keys(keepL).some(nm => !nextL.find(l  => l.name === nm));
  const topoChanged = nodeAdded || lanAdded || nodeRemoved || lanRemoved || edgeChanged;
  if (topoChanged && (nextN.length + nextL.length) > 1)
    forceLayout(nextN, nextL, g_edges);
  else if ((nextN.length + nextL.length) <= 1 && g_layout_anim_id !== null) {
    cancelAnimationFrame(g_layout_anim_id);
    g_layout_anim_id = null;
    g_layout_settled = false;
  }

  const activeNames = new Set(nextN.map(n => n.name));
  Object.keys(g_vm_states).forEach(n => { if (!activeNames.has(n)) delete g_vm_states[n]; });
  Object.keys(g_usage).forEach(n => { if (!activeNames.has(n)) delete g_usage[n]; });

  if (fromServer) { try { reapDeadPanels(); } catch(e){} }

  updatePanelPositions();
  draw();
}

// Drawing
function drawEdges() {
  g_edges.forEach(edge => {
    const nd  = g_nodes.find(n => n.name === edge.name);
    const lan = g_lans.find(l => l.name === edge.lan);
    if (!nd || !lan) return;
    // Use the node's eth count for placement; if metadata lags (nb_eth not
    // yet known) fall back to the node centre so the link still shows.
    const ep = (nd.nb_eth && edge.num < nd.nb_eth)
                 ? ethPos(nd, edge.num) : { x: nd.x, y: nd.y };
    ctx.beginPath();
    ctx.moveTo(ep.x, ep.y);
    ctx.lineTo(lan.x, lan.y);
    ctx.strokeStyle = isAir(lan.name) ? "#f5b041" : "#3498db";
    ctx.lineWidth = 2.5 / (g_zoom || 1);   // keep links visible when zoomed out
    ctx.setLineDash([]);
    ctx.stroke();
  });
}

function drawNode(nd) {
  const { x, y, r, name, type, nb_eth } = nd;
  // Node body colour from vmstate events, "down" before the first one.
  const state = g_vm_states[name];
  const SPECIAL = {
    tap: { fill:"#aee0e6", stroke:"#15616d", shape:"square"  },
    nat: { fill:"#a8e6a1", stroke:"#1b6535", shape:"diamond" },
    a2b: { fill:"#aee0e6", stroke:"#15616d", shape:"circle"  },
    c2c: { fill:"#c9b3e0", stroke:"#6a1b6a", shape:"diamond" },
    phy: { fill:"#d4a373", stroke:"#5a3a1a", shape:"square"  },
  };
  const spec = SPECIAL[type] ? { ...SPECIAL[type] } : null;
  // c2c fill: tcp && udp peered -> light green ; tcp only -> light red ; else red.
  if (spec && type === "c2c") {
    if (nd.tcp_peered && nd.udp_peered)      spec.fill = "#a8e6a1"; // lightgreen
    else if (nd.tcp_peered)                  spec.fill = "#f1948a"; // lightred
    else                                     spec.fill = "#e74c3c"; // red
  }
  const col = spec ? spec
            : (state && VM_COLOR[state]) ? VM_COLOR[state] : VM_COLOR.down;
  ctx.beginPath();
  if (spec && spec.shape === "square") {
    const d = r;
    ctx.rect(x - d, y - d, 2*d, 2*d);
  } else if (spec && spec.shape === "diamond") {
    const d = r * 1.25;
    ctx.moveTo(x, y - d); ctx.lineTo(x + d, y);
    ctx.lineTo(x, y + d); ctx.lineTo(x - d, y); ctx.closePath();
  } else {
    ctx.arc(x, y, r, 0, 2*Math.PI);
  }
  ctx.fillStyle   = col.fill;
  ctx.strokeStyle = col.stroke;
  ctx.lineWidth = 2; ctx.fill(); ctx.stroke();
  if (nd._locked) {
    const lx = x + r*0.72, ly = y - r*0.72;
    ctx.fillStyle = "#f1c40f";
    ctx.font = "12px monospace";
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    ctx.fillText("\u{1F512}", lx, ly);
  }
  if (spec) {
    ctx.fillStyle = "#ecf0f1";
    ctx.font = "9px monospace";
    ctx.textAlign = "center"; ctx.textBaseline = "top";
    const below = (spec.shape === "diamond") ? r * 1.25 : r;
    ctx.fillText(name, x, y + below + 3);
  } else {
    ctx.fillStyle = "#ecf0f1";
    ctx.font = `bold ${type==="kvm"?11:9}px monospace`;
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    const labelY = (type === "kvm") ? y - 10 : y;
    ctx.fillText(name, x, labelY);
  }

  if (!spec && type === "kvm") {
    const u = getUsage(name) || { ram: 0, cpu: 0 };
    const BAR_H = 5, GAP = 2;
    const ramTop = y + 4;
    const cpuTop = ramTop + BAR_H + GAP;
    const margin = 6;
    const dy = (cpuTop + BAR_H) - y;                 // furthest bar edge from centre
    const rr = Math.max(0, (r - margin) * (r - margin) - dy * dy);
    const TRACK_W = Math.max(20, Math.round(2 * Math.sqrt(rr)));
    const SCALE   = TRACK_W / 30;                    // track designed at 30px
    const ramW = Math.max(0, Math.min(TRACK_W, Math.round((u.ram / 20) * SCALE)));
    const cpuW = Math.max(0, Math.min(TRACK_W, Math.round((u.cpu / 2)  * SCALE)));
    const drawBar = (yTop, w, label) => {
      const bx = x - TRACK_W/2;
      // track: white translucent + black outline
      ctx.beginPath();
      ctx.rect(bx, yTop, TRACK_W, BAR_H);
      ctx.fillStyle = "rgba(255,255,255,0.6)";
      ctx.fill();
      ctx.lineWidth = 1; ctx.strokeStyle = "rgba(0,0,0,0.85)";
      ctx.stroke();
      // orange fill
      if (w > 0) {
        ctx.beginPath();
        ctx.rect(bx, yTop, w, BAR_H);
        ctx.fillStyle = "rgba(252,84,0,0.8)";
        ctx.fill();
      }
      // label tab on the left edge of the track
      ctx.fillStyle = "#10202a";
      ctx.font = "6px monospace";
      ctx.textAlign = "left"; ctx.textBaseline = "middle";
      ctx.fillText(label, bx + 2, yTop + BAR_H/2 + 0.5);
    };
    drawBar(ramTop, ramW, "ram");
    drawBar(cpuTop, cpuW, "cpu");
  }
  const ethR    = spec ? 7 : ETH_R;
  const ethInner= spec ? 3 : INNER_R;
  const ethFont = spec ? "6px monospace" : "8px monospace";
  for (let i = 0; i < nb_eth; i++) {
    const ep = ethPos(nd, i);
    const connected = g_edges.find(e => e.name===nd.name && e.num===i);
    const targetable = g_selected_lan && !connected;
    const act = getActivity(nd.name, i);
    const isEthv = nd.etype && nd.etype[i] === "ethv";
    const wlan = isWlan(nd, i);

    let fill, stroke;
    if (wlan) {
      // orange family (wlan, on the wifi lan)
      fill   = "#ca6f1e";
      stroke = "#f5b041";
    } else if (isEthv) {
      // cyan family (veth, non-sniffable)
      fill   = targetable ? "#33ffff" : connected ? "#16a3a3" : "#0e6e6e";
      stroke = targetable ? "#aaffff" : connected ? "#33ffff" : "#16a3a3";
    } else {
      // green family (seth, sniffable)
      fill   = targetable ? "#54ff54" : connected ? "#27ae60" : "#1d8348";
      stroke = targetable ? "#aaffaa" : connected ? "#54ff54" : "#27ae60";
    }

    ctx.beginPath();
    ctx.arc(ep.x, ep.y, ethR, 0, 2*Math.PI);
    ctx.fillStyle = fill; ctx.strokeStyle = stroke;
    ctx.lineWidth = 1.5; ctx.fill(); ctx.stroke();

    // TX: flash outer ring
    if (act && act.tx) {
      ctx.beginPath();
      ctx.arc(ep.x, ep.y, ethR + 2.5, 0, 2*Math.PI);
      ctx.strokeStyle = "#e74c3c"; ctx.lineWidth = 3; ctx.stroke();
    }
    // RX: flash inner dot
    if (act && act.rx) {
      ctx.beginPath();
      ctx.arc(ep.x, ep.y, ethInner, 0, 2*Math.PI);
      ctx.fillStyle = "#ff6b6b"; ctx.fill();
    }

    // Index label
    ctx.fillStyle = "#ecf0f1";
    ctx.font = ethFont;
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    ctx.fillText(wlan ? "w" + wlanIdx(nd, i)
                      : (nd.type === "a2b") ? "AB"[i] || i : i, ep.x, ep.y);
  }
}

function drawLan(lan) {
  const { x, y, name } = lan;
  const sel = lan.name === g_selected_lan;
  const air = isAir(lan.name);
  // Draw as a circle (same style as eth dots but larger)
  ctx.beginPath();
  ctx.arc(x, y, LAN_R, 0, 2*Math.PI);
  ctx.fillStyle   = air ? "#9c640c" : sel ? "#6c3483" : "#555";
  ctx.strokeStyle = air ? "#f5b041" : sel ? "#a569bd" : "#888";
  ctx.lineWidth   = sel ? 2.5 : 1.5;
  ctx.fill(); ctx.stroke();
  if (lan._locked) {
    ctx.fillStyle = "#f1c40f";
    ctx.font = "11px monospace";
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    ctx.fillText("\u{1F512}", x + LAN_R, y - LAN_R);
  }
  // Label below the dot
  ctx.fillStyle = "#ecf0f1";
  ctx.font = "9px monospace";
  ctx.textAlign = "center"; ctx.textBaseline = "top";
  ctx.fillText(name, x, y + LAN_R + 2);
}

function worldToScreen(wx, wy) {
  return { x: wx * g_zoom + g_pan_x,
           y: wy * g_zoom + g_pan_y };
}

function draw() {
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  applyTransform();
  if (g_nodes.length===0 && g_lans.length===0) {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = "#555"; ctx.font = "14px monospace";
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    ctx.fillText("Right-click to create VMs, containers or LANs",
                 canvas.width/2, canvas.height/2);
    return;
  }
  drawEdges();
  g_lans.forEach(drawLan);
  g_nodes.forEach(drawNode);
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  updatePanelPositions();
}

function resize() {
  canvas.width  = canvas.offsetWidth;
  canvas.height = canvas.offsetHeight;
  draw();
}

// Hit testing
function nodeAt(wx, wy) {
  for (let i=g_nodes.length-1; i>=0; i--) {
    const nd=g_nodes[i], dx=wx-nd.x, dy=wy-nd.y;
    if (dx*dx+dy*dy<=(nd.r+ETH_R+ETH_GAP+4)**2) return nd;
  }
  return null;
}

function lanAt(wx, wy) {
  for (let i=g_lans.length-1; i>=0; i--) {
    const l=g_lans[i], dx=wx-l.x, dy=wy-l.y;
    if (dx*dx+dy*dy<=(LAN_R+6)**2) return l;
  }
  return null;
}

function ethAt(wx, wy) {
  for (const nd of g_nodes) {
    const er = (nd.r <= SPECIAL_R) ? 7 : ETH_R;
    for (let i=0; i<nd.nb_eth; i++) {
      const ep=ethPos(nd,i), dx=wx-ep.x, dy=wy-ep.y;
      if (dx*dx+dy*dy<=(er+4)**2) return {node:nd, ethIdx:i};
    }
  }
  return null;
}

function edgeAt(wx, wy) {
  const TOL = 7;  // world-units pick tolerance around the line
  let best = null, bestD = TOL;
  for (const edge of g_edges) {
    const nd  = g_nodes.find(n => n.name === edge.name);
    const lan = g_lans.find(l => l.name === edge.lan);
    if (!nd || !lan) continue;
    const ep = (nd.nb_eth && edge.num < nd.nb_eth)
                 ? ethPos(nd, edge.num) : { x: nd.x, y: nd.y };
    // distance from point to segment ep->lan
    const vx = lan.x - ep.x, vy = lan.y - ep.y;
    const wx2 = wx - ep.x, wy2 = wy - ep.y;
    const len2 = vx*vx + vy*vy;
    let t = len2 ? (wx2*vx + wy2*vy) / len2 : 0;
    t = Math.max(0, Math.min(1, t));
    const cx = ep.x + t*vx, cy = ep.y + t*vy;
    const dx = wx - cx, dy = wy - cy;
    const d = Math.sqrt(dx*dx + dy*dy);
    if (d < bestD) { bestD = d; best = edge; }
  }
  return best;
}

