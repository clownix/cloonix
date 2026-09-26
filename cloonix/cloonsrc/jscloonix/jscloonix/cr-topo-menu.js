// cr-topo-menu.js — context menus, config / node-info dialogs, the topology
// WebSocket connections and init.

const VMF_PERSISTENT = 0x00001,
      VMF_FULL_VIRT  = 0x00008, VMF_INSTALL_CDROM = 0x00020,
      VMF_NO_REBOOT  = 0x00040, VMF_WINDOWS = 0x00400,
      VMF_UEFI       = 0x00800, VMF_DERIVED_BACKING = 0x10000;

let ctxMenu = null;
function removeCtxMenu() {
  if (ctxMenu) { ctxMenu.remove(); ctxMenu = null; }
  document.querySelectorAll(".ctx-submenu").forEach(el=>el.remove());
}

function menuItem(label, onClick) {
  const el = document.createElement("div");
  el.style.cssText = "padding:7px 18px; cursor:pointer; white-space:nowrap;";
  el.textContent = label;
  el.addEventListener("mouseenter", ()=>{ el.style.background="#0f3460"; });
  el.addEventListener("mouseleave", ()=>{ el.style.background=""; });
  if (onClick) el.addEventListener("mouseup", e=>{ e.stopPropagation(); removeCtxMenu(); onClick(); });
  return el;
}

function menuSep() {
  const el = document.createElement("div");
  el.style.cssText = "height:1px; background:#0f3460; margin:3px 0;";
  return el;
}

function menuRadio(label, checked, onChange) {
  const wrap = document.createElement("div");
  wrap.style.cssText = "display:flex; align-items:center; padding:5px 18px; cursor:pointer; gap:6px;";
  const rb = document.createElement("input");
  rb.type="radio"; rb.name="brand"; rb.checked=checked; rb.style.cursor="pointer";
  wrap.appendChild(rb); wrap.appendChild(document.createTextNode(label));
  wrap.addEventListener("mouseenter",()=>wrap.style.background="#0f3460");
  wrap.addEventListener("mouseleave",()=>wrap.style.background="");
  wrap.addEventListener("mouseup", e=>{ e.stopPropagation(); rb.checked=true; onChange(); });
  return wrap;
}

function menuToggleDot(label, isOn, onClick) {
  const wrap = document.createElement("div");
  wrap.style.cssText = "display:flex; align-items:center; padding:7px 18px; cursor:pointer; gap:8px; white-space:nowrap;";
  const dot = document.createElement("span");
  dot.style.cssText = `width:9px; height:9px; border-radius:50%; flex-shrink:0;
    background:${isOn ? "#2ecc71" : "#e74c3c"};
    box-shadow:0 0 4px ${isOn ? "#2ecc7188" : "#e74c3c88"};`;
  wrap.appendChild(dot);
  wrap.appendChild(document.createTextNode(label));
  wrap.addEventListener("mouseenter",()=>wrap.style.background="#0f3460");
  wrap.addEventListener("mouseleave",()=>wrap.style.background="");
  wrap.addEventListener("mouseup", e=>{ e.stopPropagation(); removeCtxMenu(); onClick(); });
  return wrap;
}

function clampToViewport(el, clientX, clientY) {
  // Called after the menu's items are in the DOM so we know its real size.
  const M = 6;                       // margin from the window edge
  const vw = window.innerWidth, vh = window.innerHeight;
  const rect = el.getBoundingClientRect();
  let left = clientX, top = clientY;
  if (left + rect.width  > vw - M) left = Math.max(M, vw - rect.width  - M);
  if (top  + rect.height > vh - M) top  = Math.max(M, vh - rect.height - M);
  if (rect.height > vh - 2*M) {
    el.style.maxHeight = (vh - 2*M) + "px";
    el.style.overflowY = "auto";
    top = M;
  }
  el.style.left = left + "px";
  el.style.top  = top  + "px";
}

function makeMenu(clientX, clientY) {
  const menu = document.createElement("div");
  menu.style.cssText = `
    position:fixed; left:${clientX}px; top:${clientY}px; visibility:hidden;
    background:#16213e; border:1px solid #0f3460; border-radius:4px;
    font-family:monospace; font-size:12px; color:#e0e0e0;
    z-index:1000; min-width:200px; box-shadow:0 4px 16px #000c; padding:4px 0;`;
  // Dismiss on click outside (not on mouseleave — leaving toward a submenu
  // must not tear the menu down).
  setTimeout(()=>{
    const dismiss = ev => {
      if (ctxMenu && !ctxMenu.contains(ev.target) &&
          !(ev.target.closest && ev.target.closest(".ctx-submenu"))) {
        removeCtxMenu();
        document.removeEventListener("mousedown", dismiss, true);
      }
    };
    document.addEventListener("mousedown", dismiss, true);
  }, 0);
  document.body.appendChild(menu);
  ctxMenu = menu;
  requestAnimationFrame(()=>{ clampToViewport(menu, clientX, clientY);
                              menu.style.visibility = "visible"; });
  return menu;
}

function buildCtxMenu(mx, my, clientX, clientY) {
  const ehit = (typeof ethAt === "function") ? ethAt(mx, my) : null;
  if (ehit) {
    const nd = ehit.node, i = ehit.ethIdx;
    const etype = (nd.etype && nd.etype[i]) || "eths";
    const isEthv = etype === "ethv";
    const isEths = etype === "eths";
    const menu = makeMenu(clientX, clientY);
    if (etype === "wlan") {
      const air = airOf(nd, i);
      const dead = menuItem(air ? `wlan${wlanIdx(nd, i)} on the air ${air}: ` +
                                  `double-click the air for its capture`
                                : `wlan${wlanIdx(nd, i)}: on no air`, null);
      dead.style.opacity = "0.5"; dead.style.cursor = "default";
      menu.appendChild(dead);
      return;
    }

    // PlotTraffic — sniffable only.
    if (isEths) {
      menu.appendChild(menuItem(`PlotTraffic  ${nd.name} eth${i}`, ()=>{
        if (typeof openTrafficPanel === "function")
          openTrafficPanel(nd.name, i, nd);
      }));
    } else {
      const dead = menuItem(`eth${i}: not sniffable (no traffic plot)`, null);
      dead.style.opacity = "0.5"; dead.style.cursor = "default";
      menu.appendChild(dead);
    }

    // Sniffable toggle — only for real veth/seth interfaces (eths/ethv), not
    // for "none"/special endpoints which have no snf capability to flip.
    if (isEths || isEthv) {
      menu.appendChild(menuSep());
      if (isEths)
        menu.appendChild(menuItem(`Sniff off  (make non-sniffable)`,
          ()=> snfSet(nd.name, i, 0)));
      else
        menu.appendChild(menuItem(`Sniff on  (make sniffable)`,
          ()=> snfSet(nd.name, i, 1)));
    }
    return;
  }

  const lhit = lanAt(mx, my);
  if (lhit) {
    const menu = makeMenu(clientX, clientY);
    menu.appendChild(menuToggleDot(
      lhit._locked ? "Locked (click to unlock)" : "Lock into place",
      !!lhit._locked,
      ()=>{ lhit._locked = !lhit._locked;
            if (!lhit._locked) reviveLayout(120);
            if (typeof draw === "function") draw(); }));
    menu.appendChild(menuSep());
    if (isAir(lhit.name)) {
      // an air is born not captured: cnf <air> sniff=on|off
      menu.appendChild(menuItem(`Sniff on  (capture the air)`,
        ()=> snfSet(lhit.name, 0, 1)));
      menu.appendChild(menuItem(`Sniff off  (stop capturing it)`,
        ()=> snfSet(lhit.name, 0, 0)));
      menu.appendChild(menuItem(`Webshark  the air ${lhit.name}`,
        ()=> openAirWebshark(lhit.name, lhit)));
      menu.appendChild(menuItem(`Select, to plug or unplug a wlan`,
        ()=>{ g_selected_lan = lhit.name; draw(); }));
      menu.appendChild(menuSep());
    }
    menu.appendChild(menuItem(`Delete LAN "${lhit.name}"`, ()=>{
      const links = g_edges.filter(e => e.lan === lhit.name);
      links.forEach(e =>
        post("/ctl/del-edge", { name:e.name, num:e.num, lan:lhit.name }));
      g_pending_lans = g_pending_lans.filter(n => n !== lhit.name);
    }));
    return;
  }

  const edgeHit = edgeAt(mx, my);
  if (edgeHit) {
    const menu = makeMenu(clientX, clientY);
    menu.appendChild(menuItem(
      `Delete link  ${edgeHit.name}/${edgeHit.num} — ${edgeHit.lan}`,
      ()=>{
        post("/ctl/del-edge",
             { name:edgeHit.name, num:edgeHit.num, lan:edgeHit.lan });
        const remaining = g_edges.filter(e =>
          e.lan === edgeHit.lan &&
          !(e.name === edgeHit.name && e.num === edgeHit.num));
        if (remaining.length === 0) {
          g_pending_lans = g_pending_lans.filter(n => n !== edgeHit.lan);
        }
      }));
    return;
  }

  // Right-click on node.
  const nhit = nodeAt(mx, my);
  if (nhit) {
    const menu = makeMenu(clientX, clientY);

    menu.appendChild(menuToggleDot(
      nhit._locked ? "Locked (click to unlock)" : "Lock into place",
      !!nhit._locked,
      ()=>{ nhit._locked = !nhit._locked;
            if (!nhit._locked) reviveLayout(120);
            if (typeof draw === "function") draw(); }));
    menu.appendChild(menuSep());

    if (nhit.type === "kvm") {
      // node context menu order: Desktop, Console, Save, Reboot, Delete, Info
      menu.appendChild(menuItem("Desktop", ()=>{
        fetch("/ctl/vnc-desktop", {
          method:"POST",
          headers:{"Content-Type":"application/json"},
          body: JSON.stringify({ name: nhit.name, vm_id: nhit.vm_id||0 })
        })
        .then(r => r.json())
        .then(j => openVncPanel(nhit.name, j.url, nhit))
        .catch(()=> openVncPanel(nhit.name, `/vnc-${nhit.name}/`, nhit));
      }));

      menu.appendChild(menuItem("Console", ()=>{
        openSshPanel(nhit.name, g_net, nhit);
      }));

      // Save — node_sav_whole
      menu.appendChild(menuItem("Save", ()=>{
        const dflt = `/var/lib/cloonix/${g_net}/save/${nhit.name}.qcow2`;
        const path = window.prompt(`Save ${nhit.name} rootfs to:`, dflt);
        if (path) post("/ctl/save-vm", { name: nhit.name, path });
      }));

      // Reboot — node_qreboot_vm
      menu.appendChild(menuItem("Reboot", ()=>{
        post("/ctl/reboot-vm", { name: nhit.name });
      }));

      // Delete — node_item_delete
      menu.appendChild(menuItem("Delete",
        ()=>post("/ctl/del-node",{name:nhit.name})));

      // Info — node_item_info
      menu.appendChild(menuItem("Info", ()=> showNodeInfo(nhit)));
    } else if (nhit.type === "cnt") {
      // cnt: remote console (container shell), info, delete
      menu.appendChild(menuItem("remote console", ()=>{
        openSshPanel(nhit.name, g_net, nhit);
      }));
      menu.appendChild(menuItem("info", ()=> showNodeInfo(nhit)));
      menu.appendChild(menuItem("delete",
        ()=>post("/ctl/del-node",{name:nhit.name})));
    } else {
      if (nhit.type === "a2b") {
        menu.appendChild(menuItem(`Tune a2b "${nhit.name}"`, ()=>{
          if (typeof openA2bPanel === "function") openA2bPanel(nhit.name);
        }));
        menu.appendChild(menuSep());
      }
      // Special endpoints (tap / nat / a2b / c2c / phy): delete.
      menu.appendChild(menuItem(`delete ${nhit.type} "${nhit.name}"`,
        ()=>post("/ctl/del-node",{name:nhit.name})));
    }
    return;
  }

  // Right-click on canvas background: full creation menu
  const menu = makeMenu(clientX, clientY);

  function curImages() {
    if (g_brand==="brandkvm") return g_images.kvm;
    return g_images.cnt;   // brandcnt — zip removed
  }
  function curCfg() { return g_brand==="brandkvm" ? g_kvm_cfg : g_cnt_cfg; }
  function curImage() {
    const imgs=curImages(), cfg=curCfg();
    if (!cfg.image && imgs.length) cfg.image=imgs[0];
    return cfg.image || "(no image)";
  }

  function createAtMouse() {
    if (g_brand==="brandkvm") {
      const cfg=g_kvm_cfg;
      const name="Cloon"+(cfg.counter++);
      post("/ctl/add-vm",{name, image:cfg.image||curImages()[0]||"",
        cpu:cfg.cpu, mem:cfg.mem, nb_eth:cfg.nb_eth,
        eth:Array.from({length:cfg.nb_eth},()=>({type:"ethv"})),
        persistent:0, no_qemu_ga:0, windows:cfg.windows||0,
        cisco:cfg.cisco||0, freebsd:cfg.freebsd||0, nb_vwif:cfg.nb_vwif||0});
    } else {
      const cfg=g_cnt_cfg;
      const name="Cnt"+(cfg.counter++);
      post("/ctl/add-cnt",{name, brandtype:g_brand,
        image:cfg.image||curImages()[0]||"", nb_eth:cfg.nb_eth,
        eth:Array.from({length:cfg.nb_eth},()=>({type:"ethv"})),
        nb_vwif:cfg.nb_vwif, persistent:cfg.persistent,
        startup_env:cfg.startup_env, vmount:cfg.vmount});
    }
  }

  // Top item: current image name = creates immediately
  const topItem = menuItem(curImage(), createAtMouse);
  topItem.style.fontWeight = "bold";
  menu.appendChild(topItem);

  // "List Other Choices" submenu, flush against the main menu (no gap to cross).
  const subImg = document.createElement("div");
  subImg.className = "ctx-submenu";
  subImg.style.cssText=`display:none; position:fixed; left:${clientX+196}px; top:${clientY}px;
    background:#16213e; border:1px solid #0f3460; border-radius:4px;
    z-index:1002; min-width:160px; padding:4px 0; box-shadow:0 4px 16px #000c;`;
  let subHideTimer = null;
  const subShow = ()=>{
    if(subHideTimer){clearTimeout(subHideTimer);subHideTimer=null;}
    subImg.style.display="block";
    const M = 6, vw = window.innerWidth, vh = window.innerHeight;
    const mr = menu.getBoundingClientRect();
    const sr = subImg.getBoundingClientRect();
    let left = mr.right - 4;
    if (left + sr.width > vw - M) left = mr.left - sr.width + 4; // flip left
    if (left < M) left = M;
    let top = mr.top;
    if (top + sr.height > vh - M) top = Math.max(M, vh - sr.height - M);
    if (sr.height > vh - 2*M) {
      subImg.style.maxHeight = (vh - 2*M) + "px";
      subImg.style.overflowY = "auto";
      top = M;
    }
    subImg.style.left = left + "px";
    subImg.style.top  = top  + "px";
  };
  const subHideSoon = ()=>{ if(subHideTimer)clearTimeout(subHideTimer); subHideTimer=setTimeout(()=>{subImg.style.display="none";},260); };
  subImg.addEventListener("mouseenter", subShow);
  subImg.addEventListener("mouseleave", subHideSoon);
  (curImages().length?curImages():["(no image)"]).forEach(img=>{
    const it=menuItem(img,()=>{
      curCfg().image=img; removeCtxMenu();
      buildCtxMenu(mx,my,clientX,clientY);
    });
    if(img===curImage()) it.style.color="#58d68d";
    subImg.appendChild(it);
  });
  document.body.appendChild(subImg);

  const listBtn=menuItem("List Other Choices ▶", null);
  listBtn.addEventListener("mouseenter", subShow);
  listBtn.addEventListener("mouseleave", subHideSoon);
  menu.appendChild(listBtn);

  menu.appendChild(menuItem(g_brand==="brandkvm"?"kvm config":"cnt config",
    ()=>showConfigDialog(g_brand)));

  menu.appendChild(menuSep());
  // Only two node kinds now: kvm and cnt.
  [["brandkvm","kvm"],["brandcnt","cnt"]].forEach(([b,lbl])=>{
    menu.appendChild(menuRadio(lbl, g_brand===b, ()=>{
      g_brand=b; removeCtxMenu(); subImg.remove();
      buildCtxMenu(mx,my,clientX,clientY);
    }));
  });

  menu.appendChild(menuSep());
  menu.appendChild(menuItem("lan", ()=>{
    const name=`lan${String(g_lan_counter++).padStart(2,"0")}`;
    post("/ctl/add-lan",{name});
    g_pending_lans.push(name);
    if (g_last_topo) applyTopo(g_last_topo, false);
    else draw();
  }));
  menu.appendChild(menuItem("tap", ()=>{
    post("/ctl/add-tap",{name:`tap${String(g_tap_counter++).padStart(2,"0")}`});
  }));
  menu.appendChild(menuItem("nat", ()=>{
    post("/ctl/add-nat",{name:`nat${String(g_nat_counter++).padStart(2,"0")}`});
  }));
  menu.appendChild(menuItem("a2b", ()=>{
    post("/ctl/add-a2b",{name:`a2b${String(g_a2b_counter++).padStart(2,"0")}`});
  }));
  {
    const phys = (g_last_topo && g_last_topo.info_phy) || [];
    const subPhy = document.createElement("div");
    subPhy.className = "ctx-submenu";
    subPhy.style.cssText = `display:none; position:fixed; left:${clientX+196}px; top:${clientY}px;
      background:#16213e; border:1px solid #0f3460; border-radius:4px;
      font-family:monospace; font-size:12px; color:#e0e0e0;
      z-index:1002; min-width:160px; padding:4px 0; box-shadow:0 4px 16px #000c;`;
    let phyTimer = null;
    const phyShow = ()=>{
      if (phyTimer) clearTimeout(phyTimer);
      subPhy.style.display = "block";
      const r = phyBtn.getBoundingClientRect();
      let left = r.right, top = r.top;
      const sw = subPhy.offsetWidth, sh = subPhy.offsetHeight;
      if (left + sw > window.innerWidth)  left = Math.max(0, r.left - sw);
      if (top + sh > window.innerHeight)  top  = Math.max(0, window.innerHeight - sh);
      subPhy.style.left = left + "px"; subPhy.style.top = top + "px";
    };
    const phyHideSoon = ()=>{ if(phyTimer)clearTimeout(phyTimer); phyTimer=setTimeout(()=>{subPhy.style.display="none";},260); };
    subPhy.addEventListener("mouseenter", phyShow);
    subPhy.addEventListener("mouseleave", phyHideSoon);
    if (!phys.length) {
      const none = menuItem("(no host interface known)", null);
      none.style.opacity = "0.5"; none.style.cursor = "default";
      subPhy.appendChild(none);
    }
    phys.forEach(name => {
      ["absorb", "macvlan"].forEach(type => {
        subPhy.appendChild(menuItem(`${name}  ${type}`, ()=>{
          post("/ctl/add-phy", { name, type });
        }));
      });
    });
    document.body.appendChild(subPhy);
    const phyBtn = menuItem("phy ▶", null);
    phyBtn.addEventListener("mouseenter", phyShow);
    phyBtn.addEventListener("mouseleave", phyHideSoon);
    menu.appendChild(phyBtn);
    const obsPhy = new MutationObserver(()=>{
      if(!document.body.contains(menu)){ subPhy.remove(); obsPhy.disconnect(); }
    });
    obsPhy.observe(document.body, {childList:true});
  }
  menu.appendChild(menuItem("c2c", ()=>{
    removeCtxMenu();
    const dest = window.prompt("c2c destination cluster name:", "");
    if (dest && dest.trim()) {
      post("/ctl/add-c2c",
        { name:`c2c${String(g_c2c_counter++).padStart(2,"0")}`, dest:dest.trim() });
    }
  }));

  const obs=new MutationObserver(()=>{
    if(!document.body.contains(menu)){ subImg.remove(); obs.disconnect(); }
  });
  obs.observe(document.body,{childList:true,subtree:true});
}

// Config dialog
function showConfigDialog(brand) {
  const isKvm=brand==="brandkvm";
  const cfg=isKvm?g_kvm_cfg:g_cnt_cfg;
  const overlay=document.createElement("div");
  overlay.style.cssText=`position:fixed;inset:0;background:#0008;
    display:flex;align-items:center;justify-content:center;z-index:2000;`;
  const dlg=document.createElement("div");
  dlg.style.cssText=`background:#16213e;border:1px solid #0f3460;border-radius:6px;
    padding:20px;min-width:340px;font-family:monospace;font-size:12px;
    color:#e0e0e0;max-height:80vh;overflow-y:auto;`;
  const title=document.createElement("h3");
  title.textContent=isKvm?"KVM Config":"Container Config";
  title.style.cssText="margin:0 0 14px;color:#e94560;font-size:14px;";
  dlg.appendChild(title);

  function row(label,widget){
    const r=document.createElement("div");
    r.style.cssText="display:flex;align-items:center;gap:8px;margin:5px 0;";
    const l=document.createElement("label");
    l.textContent=label; l.style.cssText="width:110px;flex-shrink:0;color:#aaa;";
    r.appendChild(l); r.appendChild(widget); dlg.appendChild(r); return widget;
  }
  function inp(val){
    const el=document.createElement("input"); el.value=val;
    el.style.cssText=`background:#0f3460;border:1px solid #5dade2;color:#e0e0e0;
      padding:4px 6px;border-radius:3px;font-family:monospace;width:180px;`; return el;
  }
  function spn(min,max,val){
    const el=document.createElement("input"); el.type="number"; el.min=min; el.max=max; el.value=val;
    el.style.cssText=`background:#0f3460;border:1px solid #5dade2;color:#e0e0e0;
      padding:4px 6px;border-radius:3px;font-family:monospace;width:80px;`; return el;
  }
  function chk(label,checked){
    const el=document.createElement("input"); el.type="checkbox"; el.checked=!!checked; el.style.marginRight="6px";
    const w=document.createElement("label"); w.style.cssText="display:flex;align-items:center;cursor:pointer;";
    w.appendChild(el); w.appendChild(document.createTextNode(label)); return{w,el};
  }

  const inpName=row("Name:",inp(cfg.name));
  let inpCpu,inpMem,cbPers,cbWin,cbCisco,cbBsd,inpWif,inpEnv,inpMnt,inpNbEth;

  const MAX_ETH = 8;

  if(isKvm){
    inpCpu=row("CPU:",spn(1,32,cfg.cpu));
    inpMem=row("RAM (MB):",spn(128,50000,cfg.mem));
    inpNbEth=row("Nb eth:",spn(0,MAX_ETH,cfg.nb_eth));
    // one tick for the whole windows set: --windows and --uefi
    {
      const{w,el}=chk("windows",!!cfg.windows); cbWin=el;
      const fr=document.createElement("div"); fr.style.cssText="margin:5px 0 5px 118px;";
      fr.appendChild(w); dlg.appendChild(fr);
    }
    {
      const{w,el}=chk("cisco",!!cfg.cisco); cbCisco=el;
      const fr=document.createElement("div"); fr.style.cssText="margin:5px 0 5px 118px;";
      fr.appendChild(w); dlg.appendChild(fr);
    }
    {
      const{w,el}=chk("freebsd",!!cfg.freebsd); cbBsd=el;
      const fr=document.createElement("div"); fr.style.cssText="margin:5px 0 5px 118px;";
      fr.appendChild(w); dlg.appendChild(fr);
    }
    inpWif=row("Wifi:",spn(0,3,cfg.nb_vwif));
  } else {
    const{w,el}=chk("persistent",!!cfg.persistent); cbPers=el;
    const fr=document.createElement("div"); fr.style.cssText="margin:5px 0 5px 118px;";
    fr.appendChild(w); dlg.appendChild(fr);
    inpNbEth=row("Nb eth:",spn(0,MAX_ETH,cfg.nb_eth));
    inpWif=row("Wifi:",spn(0,3,cfg.nb_vwif));
    inpEnv=row("startup_env:",inp(cfg.startup_env));
  }

  const btns=document.createElement("div");
  btns.style.cssText="display:flex;justify-content:flex-end;gap:8px;margin-top:14px;";
  const btnOk=document.createElement("button");
  btnOk.textContent="OK";
  btnOk.style.cssText=`background:#1a5276;border:1px solid #5dade2;color:#e0e0e0;
    padding:5px 14px;border-radius:3px;cursor:pointer;font-family:monospace;`;
  const btnCancel=document.createElement("button");
  btnCancel.textContent="Cancel";
  btnCancel.style.cssText=`background:#2c3e50;border:1px solid #7f8c8d;color:#e0e0e0;
    padding:5px 14px;border-radius:3px;cursor:pointer;font-family:monospace;`;
  btns.appendChild(btnCancel); btns.appendChild(btnOk); dlg.appendChild(btns);

  btnCancel.addEventListener("click",()=>overlay.remove());
  btnOk.addEventListener("click",()=>{
    cfg.name=inpName.value.trim()||cfg.name;
    cfg.nb_eth=Math.max(0, Math.min(MAX_ETH, parseInt(inpNbEth.value)|0));
    if(isKvm){ cfg.cpu=parseInt(inpCpu.value); cfg.mem=parseInt(inpMem.value);
               cfg.windows=cbWin.checked?1:0; cfg.cisco=cbCisco.checked?1:0;
               cfg.freebsd=cbBsd.checked?1:0; cfg.nb_vwif=parseInt(inpWif.value); }
    else { cfg.nb_vwif=parseInt(inpWif.value); cfg.persistent=cbPers.checked?1:0;
           cfg.startup_env=inpEnv.value; }
    overlay.remove();
  });

  overlay.appendChild(dlg); document.body.appendChild(overlay);
  overlay.addEventListener("click",e=>{ if(e.target===overlay) overlay.remove(); });
}

// Node Info dialog
function showNodeInfo(nd) {
  const overlay = document.createElement("div");
  overlay.style.cssText = `position:fixed;inset:0;background:#0008;
    display:flex;align-items:center;justify-content:center;z-index:2000;`;

  const dlg = document.createElement("div");
  dlg.style.cssText = `background:#16213e;border:1px solid #0f3460;border-radius:6px;
    padding:20px;min-width:340px;max-width:520px;font-family:monospace;font-size:12px;
    color:#e0e0e0;max-height:80vh;overflow-y:auto;white-space:pre-wrap;`;

  const title = document.createElement("h3");
  title.textContent = nd.name;
  title.style.cssText = "margin:0 0 12px;color:#e94560;font-size:14px;";
  dlg.appendChild(title);

  const body = document.createElement("div");
  const lines = [];

  if (nd.type === "kvm") {
    const f = nd.vm_config_flags || 0;
    lines.push("QEMU_KVM");
    if (f & VMF_WINDOWS) lines.push("WINDOWS");
    if (f & VMF_UEFI) lines.push("UEFI (OVMF)");
    lines.push((f & VMF_PERSISTENT) ? "PERSISTENT" : "EVANESCENT");
    if (f & VMF_FULL_VIRT) lines.push("FULL VIRT");
    if (f & VMF_DERIVED_BACKING) {
      lines.push("DERIVED FROM BACKING");
      lines.push(`Derived: ${nd.rootfs_used || nd.rootfs || "?"}`);
      lines.push(`Backing: ${nd.rootfs_backing || "?"}`);
    } else {
      lines.push("SOLO ROOTFS");
      lines.push(`Rootfs: ${nd.rootfs_used || nd.rootfs || "?"}`);
    }
    lines.push(`CPU: ${nd.cpu ?? "?"}`);
    lines.push(`MEM: ${nd.mem ?? "?"} MB`);
    if (f & VMF_INSTALL_CDROM) lines.push("INSTALL_CDROM");
    if (f & VMF_NO_REBOOT) lines.push("NO_REBOOT");
    if (f & 0x08000) lines.push("WITH_PXE");
    if (f & 0x00080) lines.push("ADDED_CDROM");
    if (f & 0x00100) lines.push("ADDED_DISK");
    if (f & 0x00200) lines.push("NO_QEMU_GA");
  } else {
    lines.push("CONTAINER");
    lines.push(`Brand: ${nd.brandtype || "?"}`);
    lines.push(`Image: ${nd.image || "?"}`);
    lines.push(nd.persistent ? "PERSISTENT" : "EVANESCENT");
  }
  lines.push(`vm_id: ${nd.vm_id ?? "?"}`);
  lines.push(`nb_eth: ${nd.nb_eth ?? "?"}`);

  body.textContent = lines.join("\n");
  dlg.appendChild(body);

  const btns = document.createElement("div");
  btns.style.cssText = "display:flex;justify-content:flex-end;margin-top:14px;";
  const btnOk = document.createElement("button");
  btnOk.textContent = "Close";
  btnOk.style.cssText = `background:#1a5276;border:1px solid #5dade2;color:#e0e0e0;
    padding:5px 14px;border-radius:3px;cursor:pointer;font-family:monospace;`;
  btnOk.addEventListener("click", ()=> overlay.remove());
  btns.appendChild(btnOk); dlg.appendChild(btns);

  overlay.appendChild(dlg); document.body.appendChild(overlay);
  overlay.addEventListener("click", e => { if (e.target === overlay) overlay.remove(); });
}

function startDrag(obj, wx, wy) {
  dragging=obj;
  dragOffX = obj.x - wx;
  dragOffY = obj.y - wy;
}

const PAGE_BUILD = "__JS_BUILD__";

const BEAT_DEAD_MS = 4000;

let g_last_beat = Date.now();
let g_overlay = null;
let g_srv_up = true;
let g_topo_ws = null;
let g_dead_confirm = 0;

let g_srv_build = null;

function overlayShow(title, detail) {
  if (!g_overlay) {
    g_overlay = document.createElement("div");
    // Opaque, not 0.88 with a blur behind it.
    g_overlay.style.cssText =
      "position:fixed;inset:0;z-index:99999;display:flex;" +
      "flex-direction:column;align-items:center;justify-content:center;" +
      "background:#0a0c14;color:#e74c3c;" +
      "font-family:monospace;text-align:center;user-select:none";
    g_overlay.innerHTML =
      '<div id="ovl-t" style="font-size:44px;font-weight:bold;' +
      'letter-spacing:2px"></div>' +
      '<div id="ovl-d" style="font-size:17px;color:#c8ccd4;' +
      'margin-top:18px;line-height:1.6"></div>';
    document.body.appendChild(g_overlay);
  }
  g_overlay.querySelector("#ovl-t").textContent = title;
  g_overlay.querySelector("#ovl-d").innerHTML = detail;
  g_overlay.style.display = "flex";
}

function overlayHide() {
  if (g_overlay) g_overlay.style.display = "none";
}

setInterval(() => {
  const silent = Date.now() - g_last_beat;
  if (silent <= BEAT_DEAD_MS) { g_dead_confirm = 0; return; }

  // Silence is not always the server's.
  const open = g_topo_ws && g_topo_ws.readyState === WebSocket.OPEN;
  if (open && g_dead_confirm < 1) { g_dead_confirm++; return; }
  g_dead_confirm = 0;

  // The overlay goes up BEFORE the clearing, not after.
  overlayShow("UNABLE TO CONNECT",
    "no answer from the cloonix server for " +
    Math.floor(silent / 1000) + " seconds<br><br>" +
    "the net may have been stopped with <b>cloonix stop &lt;net&gt;</b>" +
    "<br>this page reconnects on its own as soon as it is back");
  try { clearServerState(); } catch(e) { console.error("clear failed", e); }
}, 1000);

// WebSocket connections: a server still starting is not a failure.
const SRV_DOWN_BEATS    = 4;     // beats of srv:0 before believing it
const RECONNECT_MIN_MS  = 250;   // first retry, then doubling
const RECONNECT_MAX_MS  = 3000;
const RECONNECT_QUIET_MS = 1500; // a disconnect shorter than this says nothing

let g_srv_down_count = 0;
let g_reconnect_ms = RECONNECT_MIN_MS;
let g_reconnect_overlay_timer = null;

function connectTopo() {
  const proto=location.protocol==="https:"?"wss":"ws";
  const ws=new WebSocket(`${proto}://${location.host}/topo`);
  g_topo_ws = ws;
  ws.addEventListener("open",()=>{
    statusEl.textContent="connected"; statusEl.style.color="#58d68d";
    g_last_beat = Date.now();
    g_reconnect_ms = RECONNECT_MIN_MS;
    if (g_reconnect_overlay_timer !== null) {
      clearTimeout(g_reconnect_overlay_timer);
      g_reconnect_overlay_timer = null;
    }
  });
  ws.addEventListener("message",ev=>{
    // any message is proof of life, not only the beat
    g_last_beat = Date.now();
    try {
      const m=JSON.parse(ev.data);
      if(m.type==="beat") {
        if (m.bld) {
          if (g_srv_build === null)
            g_srv_build = m.bld;
          else if (g_srv_build !== m.bld) {
            overlayShow("RELOAD THIS PAGE",
              "the cloonix server has been rebuilt since this page was " +
              "loaded<br><br>this page is still running the older " +
              "javascript<br>press <b>ctrl-shift-R</b> to pick up the new " +
              "one<br><br><span style=\'font-size:13px\'>page: " +
              g_srv_build + " &nbsp;&nbsp; server: " + m.bld + "</span>");
            return;
          }
        }
        if (m.srv !== 0) {
          g_srv_down_count = 0;
          g_srv_up = true;
          overlayHide();
        }
        else if (++g_srv_down_count >= SRV_DOWN_BEATS) {
          g_srv_up = false;
          overlayShow("NET IS DOWN",
            "the web server is answering but the cloonix net behind it is " +
            "not<br><br>start it again with <b>cloonix start &lt;net&gt;</b>" +
            "<br>this page picks it up on its own");
          try { clearServerState(); }
          catch(e) { console.error("clear failed", e); }
        }
        return;
      }
      if(m.type==="topo") applyTopo(m, true);
      else if(m.type==="activity") {
        (m.evts||[]).forEach(e => setActivity(e.name, e.num, e.ptx, e.prx));
      }
      else if(m.type==="usage") {
        (m.evts||[]).forEach(e => setUsage(e.name, e.ram, e.cpu));
        draw();
      }
      else if(m.type==="traffic") {
        // Feed any open PlotTraffic windows (cr-traffic.js).
        if (typeof trafficFeed === "function")
          (m.evts||[]).forEach(e => trafficFeed(e.name, e.num, e.ms, e.btx, e.brx));
      }
      else if(m.type==="webshark_error") {
        // Capture refused by the switch: close the panel rather than wait for ever.
        const p = g_webshark_panels[`${m.name}_${m.num}`];
        if (p && p.refused) p.refused();
      }
      else if(m.type==="vmstate") {
        g_vm_states[m.name] = m.state;
        draw();
      }
      else if(m.type==="itempos") applyItemPos(m.name, m.x, m.y);
      else if(m.type==="itemposeth") applyItemPosEth(m.name, m.num, m.deg);
      else if(m.type==="pending_lans"){
        g_pending_lans = m.lans||[];
        // Re-apply the last real topo so nodes are never lost
        if (g_last_topo) applyTopo(g_last_topo, false);
        else draw();
      }
    } catch(e){ console.error("topo message handling failed:", e, ev.data && ev.data.slice ? ev.data.slice(0,200) : ev.data); }
  });
  ws.addEventListener("close",()=>{
    statusEl.textContent="disconnected — reconnecting…"; statusEl.style.color="#e74c3c";
    // The overlay is not raised on the spot.
    if (g_reconnect_overlay_timer === null) {
      g_reconnect_overlay_timer = setTimeout(() => {
        g_reconnect_overlay_timer = null;
        overlayShow("UNABLE TO CONNECT",
          "the connection to the cloonix server was closed<br><br>" +
          "the net may have been stopped with <b>cloonix stop &lt;net&gt;</b>" +
          "<br>this page reconnects on its own as soon as it is back");
        try { clearServerState(); } catch(e) { console.error("clear failed", e); }
      }, RECONNECT_QUIET_MS);
    }
    setTimeout(connectTopo, g_reconnect_ms);
    g_reconnect_ms = Math.min(g_reconnect_ms * 2, RECONNECT_MAX_MS);
  });
  ws.addEventListener("error",()=>ws.close());
}

function connectImages() {
  const proto=location.protocol==="https:"?"wss":"ws";
  const ws=new WebSocket(`${proto}://${location.host}/images`);
  ws.addEventListener("message",ev=>{
    try {
      const m=JSON.parse(ev.data);
      if(m.type==="images"){
        g_images.kvm=m.kvm||[]; g_images.cnt=m.cnt||[];
        if(!g_kvm_cfg.image&&g_images.kvm.length) g_kvm_cfg.image=g_images.kvm[0];
        if(!g_cnt_cfg.image&&g_images.cnt.length) g_cnt_cfg.image=g_images.cnt[0];
      }
    } catch(e){}
  });
  ws.addEventListener("close",()=>setTimeout(connectImages,5000));
  ws.addEventListener("error",()=>ws.close());
}

// Init
document.addEventListener("DOMContentLoaded",()=>{
  canvas=document.getElementById("topo-canvas");
  ctx=canvas.getContext("2d");
  statusEl=document.getElementById("status");
  if(!canvas||!ctx||!statusEl){ console.error("DOM elements not found"); return; }

  try {
    const b = document.createElement("span");
    b.id = "page-build";
    b.style.cssText = "margin-left:14px;font-size:11px;color:#5a6472;" +
                      "font-family:monospace";
    b.textContent = "js " + PAGE_BUILD;
    b.title = "build of the javascript this page is running";
    statusEl.parentNode.appendChild(b);

    const age = document.createElement("span");
    age.id = "beat-age";
    age.style.cssText = "margin-left:10px;font-size:11px;color:#5a6472;" +
                        "font-family:monospace";
    age.title = "seconds since the last message from the server";
    statusEl.parentNode.appendChild(age);
    setInterval(() => {
      const s = (Date.now() - g_last_beat) / 1000;
      age.textContent = "beat " + s.toFixed(1) + "s";
      age.style.color = (s > 2) ? "#e74c3c" : "#5a6472";
    }, 200);
  } catch(e) {}

  // Zoom: mouse wheel
  canvas.addEventListener("wheel", e=>{
    e.preventDefault();
    const sx = canvas.width  / 2;
    const sy = canvas.height / 2;
    const factor = e.deltaY < 0 ? 1.12 : 1/1.12;
    const nz = Math.max(ZOOM_MIN, Math.min(ZOOM_MAX, g_zoom * factor));
    g_pan_x = sx - (sx - g_pan_x) * (nz / g_zoom);
    g_pan_y = sy - (sy - g_pan_y) * (nz / g_zoom);
    g_zoom  = nz;
    draw(); // draw() calls updatePanelPositions()
  }, { passive: false });

  // Pan: middle-button drag
  let panning=false, panSX=0, panSY=0, panOX=0, panOY=0;

  // Track mousedown world position to distinguish click from drag
  let mdWorldX = 0, mdWorldY = 0;

  canvas.addEventListener("mousedown", e=>{
    if(e.button===1){
      e.preventDefault();
      panning=true; panSX=e.clientX; panSY=e.clientY;
      panOX=g_pan_x; panOY=g_pan_y; return;
    }
    if(e.button!==0) return;
    const r=canvas.getBoundingClientRect();
    const w=screenToWorld(e.clientX-r.left, e.clientY-r.top);
    mdWorldX=w.x; mdWorldY=w.y;

    const barPnl = panelBarAtScreen(e.clientX, e.clientY);
    if (barPnl) { raisePanel(barPnl); return; }

    const hitEth = g_selected_lan ? null : ethAt(w.x,w.y);
    const hitLan = lanAt(w.x,w.y);
    const hitNode = nodeAt(w.x,w.y);
    const onTopo = hitEth || hitLan || hitNode;
    if (onTopo) {
      lowerAllPanels();
    } else {
      const pnl = panelAtScreen(e.clientX, e.clientY);
      if (pnl) { raisePanel(pnl); return; }   // bring the panel to the front
      lowerAllPanels();
    }
    if(hitEth){
      const nd=hitEth.node;
      // Offset between the grabbed eth's current angle and its "slot" angle,
      // so the dot follows the mouse exactly while others stay evenly spaced.
      const slotAngle = (2*Math.PI*hitEth.ethIdx)/(nd.nb_eth||1) - Math.PI/2;
      const mouseAngle = Math.atan2(w.y-nd.y, w.x-nd.x);
      dragging = { type:"ethrot", node:nd, slotAngle };
      nd.ethRot = mouseAngle - slotAngle;
      draw();
      return;
    }
    if(hitLan){ startDrag(hitLan,w.x,w.y); return; }
    if(hitNode){ startDrag(hitNode,w.x,w.y); return; }
  });

  document.addEventListener("mousemove", e=>{
    if(panning){
      g_pan_x=panOX+(e.clientX-panSX);
      g_pan_y=panOY+(e.clientY-panSY);
      draw(); return;  // draw() calls updatePanelPositions()
    }
    if(!dragging) return;
    const r=canvas.getBoundingClientRect();
    const w=screenToWorld(e.clientX-r.left, e.clientY-r.top);
    if(dragging.type==="ethrot"){
      const nd=dragging.node;
      const mouseAngle = Math.atan2(w.y-nd.y, w.x-nd.x);
      nd.ethRot = mouseAngle - dragging.slotAngle;
      draw();
      return;
    }
    dragging.x=w.x+dragOffX;
    dragging.y=w.y+dragOffY;
    draw();
  });

  document.addEventListener("mouseup", e=>{
    if(e.button===1){ panning=false; return; }
    if(e.button!==0){
      if (!(dragging && dragging.x !== undefined && dragging.type !== "ethrot"))
        dragging = null;
      return;
    }
    const r=canvas.getBoundingClientRect();
    const w=screenToWorld(e.clientX-r.left, e.clientY-r.top);
    const tol=5/g_zoom;
    const wasEthRot = dragging && dragging.type==="ethrot";
    const wasDrag = dragging &&
      (Math.abs(w.x-mdWorldX)>tol || Math.abs(w.y-mdWorldY)>tol);
    const draggedNode = (dragging && dragging.x !== undefined) ? dragging : null;
    const ethRotNode = wasEthRot ? dragging.node : null;
    dragging=null;
    if (wasDrag && draggedNode) {
      if (typeof reviveLayout === "function") reviveLayout(120);
      else { g_layout_settled = false; g_layout_wake = 120; }
      draggedNode.vx = 0; draggedNode.vy = 0;
      // Where the user put it is where dump commands must put it back.
      reportItemPos(draggedNode);
    }
    if (wasEthRot && ethRotNode) reportNodeEths(ethRotNode);
    if(!wasDrag && !wasEthRot) handleClick(w.x, w.y);
  });

  canvas.addEventListener("dblclick",e=>{
    const r=canvas.getBoundingClientRect();
    const w=screenToWorld(e.clientX-r.left, e.clientY-r.top);
    handleDblClick(w.x,w.y);
  });

  canvas.addEventListener("mouseleave",()=>{ dragging=null; panning=false; });

  canvas.addEventListener("contextmenu",e=>{
    e.preventDefault();
    removeCtxMenu();
    const r=canvas.getBoundingClientRect();
    const w=screenToWorld(e.clientX-r.left, e.clientY-r.top);
    if (dragging && dragging.x !== undefined && dragging.type !== "ethrot") {
      const it = dragging;
      it._locked = true;
      it.vx = 0; it.vy = 0;
      dragging = null;
      reportItemPos(it);
      draw();
      return;          // no menu — the gesture's whole purpose was to lock
    }
    g_click_x=w.x; g_click_y=w.y;
    buildCtxMenu(w.x,w.y,e.clientX,e.clientY);
  });

  window.addEventListener("resize",resize);
  resize();
  connectTopo();
  connectImages();
});

