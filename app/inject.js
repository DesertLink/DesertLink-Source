(() => {
  'use strict';
  if (window.__desertLinkInjectedV105) return;
  window.__desertLinkInjectedV105 = true;
  if (!window.desertLink) return;

  let state = { settings: { follow: true }, position: null, teleport: {}, telemetry: {} };
  let waypoints = [];
  let map = null;
  let follow = true;
  let lastFollowAt = 0;
  let mapListenersBound = false;
  let currentY = null;
  let toastTimer = null;
  let panelHidden = false;
  let lastPanelToggleAt = 0;
  const PANEL_LAYOUT_KEY = 'desertlink-panel-layout-v105';

  const css = `
  #dl-panel{position:fixed;z-index:2147483600;top:18px;left:18px;width:350px;color:#eef5ff;background:rgba(7,12,18,.92);border:1px solid rgba(255,255,255,.13);border-radius:14px;box-shadow:0 18px 55px rgba(0,0,0,.48);backdrop-filter:blur(14px);font:13px/1.35 Inter,Segoe UI,Arial,sans-serif;overflow:hidden;user-select:none}
  #dl-panel *{box-sizing:border-box} #dl-head{display:flex;align-items:center;gap:9px;padding:12px 10px 12px 13px;background:linear-gradient(100deg,rgba(193,35,45,.28),rgba(20,28,38,.2));border-bottom:1px solid rgba(255,255,255,.08);cursor:move;touch-action:none}
  #dl-logo{font-weight:900;font-size:17px;letter-spacing:.2px} #dl-logo b{color:#ff4555} #dl-sub{font-size:10px;opacity:.64;margin-left:auto}.dl-headbtn{width:28px;height:25px;border:1px solid rgba(255,255,255,.15);border-radius:7px;background:#111a23;color:#eaf2fb;cursor:pointer;font-weight:900;line-height:1}.dl-headbtn:hover{background:#24313e}
  #dl-tab{position:fixed;z-index:2147483601;top:18px;left:18px;display:none;padding:8px 12px;border-radius:10px;background:linear-gradient(105deg,#7c1d29,#161f29);border:1px solid rgba(255,255,255,.18);color:#fff;font:800 12px/1 Segoe UI,Arial;box-shadow:0 8px 26px #0008;cursor:pointer;user-select:none} #dl-tab b{color:#ff5261}
  #dl-body{padding:10px 12px 12px}.dl-row{display:flex;gap:7px;align-items:center;margin:7px 0}.dl-chip{display:inline-flex;align-items:center;gap:5px;padding:4px 7px;border-radius:999px;background:#17202a;border:1px solid #2b3947;font-size:11px}.dl-dot{width:7px;height:7px;border-radius:50%;background:#7c8793}.dl-ok .dl-dot{background:#45d483;box-shadow:0 0 9px #45d483}.dl-warn .dl-dot{background:#ffb24d}.dl-bad .dl-dot{background:#ff5261}
  #dl-coords{font-family:Consolas,monospace;font-size:12px;background:#0a1016;border:1px solid #1d2a36;border-radius:9px;padding:8px 9px;display:grid;grid-template-columns:1fr 1fr;gap:3px 12px}.dl-muted{opacity:.62}.dl-btn{border:1px solid #314151;background:#15202a;color:#eef5ff;border-radius:9px;padding:7px 9px;font-weight:650;cursor:pointer;transition:.13s}.dl-btn:hover{background:#21303d;border-color:#506477}.dl-btn.primary{background:#9f2633;border-color:#c03c49}.dl-btn.primary:hover{background:#bd3140}.dl-btn.danger{background:#35171c;border-color:#71303a}.dl-btn.active{outline:2px solid #4fd69b55;border-color:#4fd69b}.dl-grow{flex:1}.dl-input{width:92px;border:1px solid #30404e;background:#0c131a;color:white;border-radius:8px;padding:7px 8px;font-family:Consolas,monospace}.dl-label{font-size:11px;opacity:.7}.dl-sep{height:1px;background:rgba(255,255,255,.08);margin:10px 0}.dl-mini{font-size:10px;opacity:.58}.dl-waypoints{max-height:150px;overflow:auto;margin-top:6px}.dl-wp{display:flex;align-items:center;gap:6px;padding:5px 0;border-bottom:1px solid rgba(255,255,255,.05)}.dl-wp-name{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1}.dl-iconbtn{width:29px;height:27px;padding:0;border-radius:7px;border:1px solid #2c3a47;background:#131d26;color:#fff;cursor:pointer}.dl-iconbtn:hover{background:#23313d}.dl-delete{color:#ff7d86}.dl-modegrp{display:flex;gap:5px;flex:1}.dl-modebtn{flex:1}.dl-modebtn.active{border-color:#ff5967;background:#33171d;box-shadow:0 0 0 1px #ff596733 inset}.dl-hotkey{min-width:82px;font-family:Consolas,monospace}.dl-hotkey.listening{border-color:#ffb24d;color:#ffd596}.dl-settinglabel{width:76px;font-size:11px;opacity:.72;flex:none}
  #dl-marker{position:fixed;z-index:2147483500;width:24px;height:24px;pointer-events:none;display:none;transform:translate(-50%,-50%)}#dl-marker:before{content:'';position:absolute;left:5px;top:5px;width:14px;height:14px;border-radius:50%;background:#ff3348;border:3px solid white;box-shadow:0 2px 9px #000,0 0 0 5px rgba(255,51,72,.25)}#dl-arrow{position:absolute;left:9px;top:-8px;width:0;height:0;border-left:4px solid transparent;border-right:4px solid transparent;border-bottom:9px solid #fff;transform-origin:4px 20px}
  #dl-crosshair{position:fixed;z-index:2147483400;width:18px;height:18px;pointer-events:none;display:none;opacity:.72}#dl-crosshair:before,#dl-crosshair:after{content:'';position:absolute;background:white;box-shadow:0 0 3px #000}#dl-crosshair:before{width:18px;height:1px;top:8px}#dl-crosshair:after{width:1px;height:18px;left:8px}
  #dl-toast{position:fixed;z-index:2147483646;right:24px;bottom:24px;max-width:420px;padding:11px 14px;border-radius:10px;background:#101820;color:white;border:1px solid #334656;box-shadow:0 10px 30px #0008;opacity:0;transform:translateY(8px);transition:.2s;pointer-events:none;font:13px Segoe UI,Arial}.show{opacity:1!important;transform:none!important}
  `;
  const st = document.createElement('style'); st.id='dl-style'; st.textContent=css; document.documentElement.appendChild(st);

  const panel = document.createElement('div'); panel.id='dl-panel'; panel.innerHTML=`
    <div id="dl-head"><div id="dl-logo">Desert<b>Link</b></div><div class="dl-chip" id="dl-realm">PYWEL</div><div id="dl-sub">Companion 1.0.7</div><button class="dl-headbtn" id="dl-collapse" title="Hide panel">—</button></div>
    <div id="dl-body">
      <div class="dl-row"><span class="dl-chip" id="dl-tel"><span class="dl-dot"></span><span>Telemetry</span></span><span class="dl-chip" id="dl-tp"><span class="dl-dot"></span><span>Teleport</span></span><span class="dl-chip" id="dl-build"><span class="dl-dot"></span><span>Build</span></span></div>
      <div id="dl-coords"><span class="dl-muted">X</span><span id="dl-x">—</span><span class="dl-muted">Y</span><span id="dl-y">—</span><span class="dl-muted">Z</span><span id="dl-z">—</span></div>
      <div class="dl-row"><button class="dl-btn dl-grow" id="dl-follow">Follow: ON</button><button class="dl-btn" id="dl-reload">↻</button></div>
      <div class="dl-row"><span class="dl-settinglabel">Window</span><div class="dl-modegrp"><button class="dl-btn dl-modebtn" id="dl-mode-app">App</button><button class="dl-btn dl-modebtn" id="dl-mode-overlay">Overlay</button></div></div>
      <div class="dl-row"><span class="dl-settinglabel">Show/Hide</span><button class="dl-btn dl-hotkey" id="dl-hotkey">End</button><span class="dl-mini">Click and press a new key</span></div>
      <div class="dl-row"><span class="dl-label">Teleport Y</span><input class="dl-input" id="dl-ty" type="number" step="1"><button class="dl-btn" id="dl-usey">Use current Y</button></div>
      <div class="dl-row"><button class="dl-btn primary dl-grow" id="dl-center">◎ Teleport Map Center</button><button class="dl-btn danger" id="dl-abort">↩ Abort</button></div>
      <div class="dl-mini">Right-click map = teleport · F5 = map center · Shift+F5 = abort · Ctrl+Shift+M = panel · Overlay hotkey = whole Companion.</div>
      <div class="dl-sep"></div>
      <div class="dl-row"><b>Waypoints</b><button class="dl-btn dl-grow" id="dl-save">+ Save current position</button></div>
      <div class="dl-waypoints" id="dl-waypoints"></div>
      <div class="dl-sep"></div>
      <div class="dl-mini" id="dl-statusline">Waiting for CrimsonDesertTelemetry…</div>
    </div>`;
  document.body.appendChild(panel);
  const panelTab=document.createElement('div'); panelTab.id='dl-tab'; panelTab.innerHTML='Desert<b>Link</b>'; document.body.appendChild(panelTab);

  const marker=document.createElement('div'); marker.id='dl-marker'; marker.innerHTML='<div id="dl-arrow"></div>'; document.body.appendChild(marker);
  const cross=document.createElement('div'); cross.id='dl-crosshair'; document.body.appendChild(cross);
  const toast=document.createElement('div'); toast.id='dl-toast'; document.body.appendChild(toast);

  const $=id=>document.getElementById(id);
  function showToast(text){ if(!text)return; toast.textContent=text; toast.classList.add('show'); clearTimeout(toastTimer); toastTimer=setTimeout(()=>toast.classList.remove('show'),2600); }
  function chip(id, mode, text){ const e=$(id); e.classList.remove('dl-ok','dl-warn','dl-bad'); e.classList.add(mode); e.querySelector('span:last-child').textContent=text; }
  function fmt(n){ return Number.isFinite(+n)?(+n).toFixed(1):'—'; }

  function keyToAccelerator(e){
    if(!e)return null;
    const k=String(e.key||'');
    if(['Control','Shift','Alt','Meta','AltGraph'].includes(k))return null;
    const map={ArrowUp:'Up',ArrowDown:'Down',ArrowLeft:'Left',ArrowRight:'Right',' ':'Space',Esc:'Escape'};
    let key=map[k]||k;
    if(/^F\d{1,2}$/i.test(key))key=key.toUpperCase();
    else if(key.length===1)key=key.toUpperCase();
    else key=key.charAt(0).toUpperCase()+key.slice(1);
    const mods=[];
    if(e.ctrlKey)mods.push('Ctrl');
    if(e.altKey)mods.push('Alt');
    if(e.shiftKey)mods.push('Shift');
    if(e.metaKey)mods.push('Super');
    return [...mods,key].join('+');
  }

  function readPanelLayout(){
    try{const v=JSON.parse(localStorage.getItem(PANEL_LAYOUT_KEY)||'{}');return v&&typeof v==='object'?v:{};}catch{return {};}
  }
  function savePanelLayout(){
    try{localStorage.setItem(PANEL_LAYOUT_KEY,JSON.stringify({left:parseFloat(panel.style.left)||18,top:parseFloat(panel.style.top)||18,hidden:panelHidden}));}catch{}
  }
  function clampPanel(left,top){
    const w=panel.offsetWidth||350,h=panel.offsetHeight||120;
    return {left:Math.max(4,Math.min(left,window.innerWidth-w-4)),top:Math.max(4,Math.min(top,window.innerHeight-Math.min(h,80)-4))};
  }
  function applyPanelLayout(){
    const v=readPanelLayout();
    const p=clampPanel(Number.isFinite(+v.left)?+v.left:18,Number.isFinite(+v.top)?+v.top:18);
    panel.style.left=p.left+'px';panel.style.top=p.top+'px';
    panelTab.style.left=p.left+'px';panelTab.style.top=p.top+'px';
    setPanelHidden(!!v.hidden,false);
  }
  function setPanelHidden(hidden,persist=true){
    panelHidden=!!hidden;
    panel.style.display=panelHidden?'none':'block';
    panelTab.style.display=panelHidden?'block':'none';
    if(persist)savePanelLayout();
  }
  function togglePanel(){
    const now=Date.now();if(now-lastPanelToggleAt<180)return;lastPanelToggleAt=now;
    setPanelHidden(!panelHidden,true);
  }
  window.__desertLinkTogglePanel=togglePanel;

  // Drag the panel by its header. Position is remembered between launches.
  let drag=null;
  $('dl-head').addEventListener('pointerdown',e=>{
    if(e.button!==0||e.target.closest('button,input,a'))return;
    const r=panel.getBoundingClientRect();
    drag={dx:e.clientX-r.left,dy:e.clientY-r.top};
    try{$('dl-head').setPointerCapture(e.pointerId);}catch{}
    e.preventDefault();e.stopPropagation();
  },true);
  $('dl-head').addEventListener('pointermove',e=>{
    if(!drag)return;
    const p=clampPanel(e.clientX-drag.dx,e.clientY-drag.dy);
    panel.style.left=p.left+'px';panel.style.top=p.top+'px';
    panelTab.style.left=p.left+'px';panelTab.style.top=p.top+'px';
    e.preventDefault();e.stopPropagation();
  },true);
  const endDrag=e=>{if(!drag)return;drag=null;savePanelLayout();try{$('dl-head').releasePointerCapture(e.pointerId);}catch{}};
  $('dl-head').addEventListener('pointerup',endDrag,true);
  $('dl-head').addEventListener('pointercancel',endDrag,true);
  $('dl-collapse').onclick=e=>{e.preventDefault();e.stopPropagation();setPanelHidden(true,true);};
  panelTab.onclick=()=>setPanelHidden(false,true);
  window.addEventListener('resize',()=>{const r=panel.getBoundingClientRect();const p=clampPanel(r.left,r.top);panel.style.left=p.left+'px';panel.style.top=p.top+'px';panelTab.style.left=p.left+'px';panelTab.style.top=p.top+'px';savePanelLayout();});
  window.addEventListener('keydown',e=>{if((e.ctrlKey||e.metaKey)&&e.shiftKey&&String(e.key).toLowerCase()==='m'){e.preventDefault();togglePanel();}},true);
  if(window.desertLink.onPanelToggle)window.desertLink.onPanelToggle(togglePanel);
  applyPanelLayout();

  let hotkeyListening=false;
  function renderWindowSettings(){
    const mode=state.window?.mode||state.settings?.windowMode||'app';
    $('dl-mode-app').classList.toggle('active',mode==='app');
    $('dl-mode-overlay').classList.toggle('active',mode==='overlay');
    const hk=state.window?.overlayHotkey||state.settings?.overlayHotkey||'End';
    if(!hotkeyListening)$('dl-hotkey').textContent=hk;
    $('dl-hotkey').title=state.window?.overlayHotkeyRegistered===false?'This hotkey could not be registered':'Global show/hide hotkey';
  }
  $('dl-mode-app').onclick=async()=>{const r=await window.desertLink.command('set-window-mode',{mode:'app'});if(!r?.ok)showToast(r?.error||'Could not switch mode');};
  $('dl-mode-overlay').onclick=async()=>{const r=await window.desertLink.command('set-window-mode',{mode:'overlay'});if(!r?.ok)showToast(r?.error||'Could not switch mode');};
  $('dl-hotkey').onclick=e=>{e.preventDefault();e.stopPropagation();hotkeyListening=true;$('dl-hotkey').classList.add('listening');$('dl-hotkey').textContent='Press key…';};
  window.addEventListener('keydown',async e=>{
    if(!hotkeyListening)return;
    e.preventDefault();e.stopPropagation();
    if(e.key==='Escape'){hotkeyListening=false;$('dl-hotkey').classList.remove('listening');renderWindowSettings();return;}
    const accel=keyToAccelerator(e);if(!accel)return;
    hotkeyListening=false;$('dl-hotkey').classList.remove('listening');
    const r=await window.desertLink.command('set-overlay-hotkey',{hotkey:accel});
    if(!r?.ok)showToast(r?.error||'Hotkey unavailable');
    else showToast('Overlay hotkey: '+(r.hotkey||accel));
    renderWindowSettings();
  },true);

  function findMap(){
    const okay=o=>o&&typeof o.getCenter==='function'&&typeof o.project==='function'&&typeof o.unproject==='function'&&(typeof o.easeTo==='function'||typeof o.flyTo==='function');
    const obvious=[window.map,window.mapboxMap,window.mapManager?.map,window.mapManager];
    for(const o of obvious) if(okay(o)) return o;
    const names=Object.getOwnPropertyNames(window);
    for(const k of names){
      if(!/map/i.test(k)) continue;
      try{ const o=window[k]; if(okay(o)) return o; if(okay(o?.map)) return o.map; }catch{}
    }
    return null;
  }

  function mapRect(){
    if(!map) return null;
    try{ const c=typeof map.getContainer==='function'?map.getContainer():map.getCanvas?.()?.parentElement; return c?.getBoundingClientRect?.()||null; }catch{return null;}
  }

  function bindMap(){
    if(!map||mapListenersBound)return; mapListenersBound=true;
    let canvas=null; try{canvas=map.getCanvas();}catch{}
    if(canvas){
      canvas.addEventListener('contextmenu',async e=>{
        e.preventDefault();
        const r=canvas.getBoundingClientRect();
        let ll; try{ll=map.unproject([e.clientX-r.left,e.clientY-r.top]);}catch{return;}
        const y=Number($('dl-ty').value);
        const res=await window.desertLink.command('teleport-map',{lng:+ll.lng,lat:+ll.lat,y:Number.isFinite(y)?y:undefined,realm:state.position?.realm});
        showToast(res?.ok?'Teleport sent':(res?.error||'Teleport failed'));
      },true);
      canvas.addEventListener('wheel',()=>{lastFollowAt=Date.now()+1400;},{passive:true});
      canvas.addEventListener('pointerdown',()=>{lastFollowAt=Date.now()+1000;},{passive:true});
    }
  }

  function updateMapVisuals(){
    if(!map){ map=findMap(); if(map){ bindMap(); showToast('Map connected'); } }
    if(!map) { marker.style.display='none'; cross.style.display='none'; return; }
    const r=mapRect(); if(!r)return;
    cross.style.display='block'; cross.style.left=(r.left+r.width/2-9)+'px'; cross.style.top=(r.top+r.height/2-9)+'px';
    try{
      const c=map.getCenter(); if(c&&Number.isFinite(+c.lng)){ window.desertLink.command('set-map-center',{lng:+c.lng,lat:+c.lat,realm:state.position?.realm}); }
    }catch{}
    if(!state.position){marker.style.display='none';return;}
    try{
      const p=map.project([state.position.lng,state.position.lat]);
      marker.style.display='block'; marker.style.left=(r.left+p.x)+'px'; marker.style.top=(r.top+p.y)+'px';
      const bearing=typeof map.getBearing==='function'?+map.getBearing():0;
      const h=Number.isFinite(+state.position.heading)?+state.position.heading:0;
      $('dl-arrow').style.transform=`rotate(${h-bearing}deg)`;
      if(follow && Date.now()>lastFollowAt && Date.now()-lastFollowAt>0){
        const now=Date.now(); if(now-lastFollowAt>250){
          lastFollowAt=now;
          const fn=typeof map.easeTo==='function'?'easeTo':'flyTo';
          map[fn]({center:[state.position.lng,state.position.lat],duration:180,essential:true});
        }
      }
    }catch{marker.style.display='none';}
  }

  function renderWaypoints(){
    const box=$('dl-waypoints'); box.textContent='';
    if(!waypoints.length){box.innerHTML='<div class="dl-mini">No saved waypoints yet.</div>';return;}
    [...waypoints].reverse().slice(0,20).forEach(w=>{
      const row=document.createElement('div');row.className='dl-wp';
      const name=document.createElement('span');name.className='dl-wp-name';name.textContent=w.name;name.title=`X ${fmt(w.x)} Y ${fmt(w.y)} Z ${fmt(w.z)}`;
      const go=document.createElement('button');go.className='dl-iconbtn';go.textContent='◎';go.title='Teleport';go.onclick=async()=>{const r=await window.desertLink.command('teleport-waypoint',{id:w.id});showToast(r?.ok?'Teleport sent':r?.error);};
      const del=document.createElement('button');del.className='dl-iconbtn dl-delete';del.textContent='×';del.title='Delete';del.onclick=()=>window.desertLink.command('delete-waypoint',{id:w.id});
      row.append(name,go,del);box.appendChild(row);
    });
  }

  function applyState(s){
    if(!s)return; if(s.toast){showToast(s.toast);return;}
    state={...state,...s};
    const p=state.position;
    $('dl-x').textContent=fmt(p?.x);$('dl-y').textContent=fmt(p?.y);$('dl-z').textContent=fmt(p?.z);
    $('dl-realm').textContent=(p?.realm||'—').toUpperCase();
    if(p&&currentY==null){currentY=p.y+1.5;if(!$('dl-ty').value)$('dl-ty').value=currentY.toFixed(1);}
    const tel=!!state.telemetry?.connected; chip('dl-tel',tel?'dl-ok':'dl-bad',tel?'Telemetry OK':'Telemetry OFF');
    const tp=state.teleport||{}; chip('dl-tp',tp.physicsReady?'dl-ok':(tp.hookInstalled?'dl-warn':'dl-bad'),tp.physicsReady?'Teleport Ready':(tp.hookInstalled?'Waiting Physics':'Teleport Offline'));
    const exact=!!state.game?.testedExactBuild; chip('dl-build',exact?'dl-ok':(state.game?.exeSha256?'dl-warn':'dl-bad'),exact?'2.01 Exact':(state.game?.exeSha256?'Other Build':'No Game'));
    follow=state.settings?.follow!==false;$('dl-follow').textContent='Follow: '+(follow?'ON':'OFF');$('dl-follow').classList.toggle('active',follow);
    if(Number.isFinite(+state.settings?.teleportY)&&document.activeElement!==$('dl-ty'))$('dl-ty').value=(+state.settings.teleportY).toFixed(1);
    $('dl-statusline').textContent=tp.message||(!tel?'Install/start CrimsonDesertTelemetry and load into the world.':'Ready.');
    renderWindowSettings();
  }

  $('dl-follow').onclick=()=>window.desertLink.command('set-follow',{value:!follow});
  $('dl-reload').onclick=()=>window.desertLink.command('reload-map');
  $('dl-usey').onclick=()=>{if(state.position){const y=state.position.y+1.5;$('dl-ty').value=y.toFixed(1);window.desertLink.command('set-y',{y});}};
  $('dl-ty').onchange=()=>{const y=Number($('dl-ty').value);if(Number.isFinite(y))window.desertLink.command('set-y',{y});};
  $('dl-center').onclick=async()=>{
    if(!map)return showToast('Map not ready');let c;try{c=map.getCenter();}catch{return;}
    const y=Number($('dl-ty').value);const r=await window.desertLink.command('teleport-map',{lng:+c.lng,lat:+c.lat,y:Number.isFinite(y)?y:undefined,realm:state.position?.realm});showToast(r?.ok?'Teleport sent':r?.error);
  };
  $('dl-abort').onclick=async()=>{const r=await window.desertLink.command('abort');showToast(r?.ok?'Returning…':r?.error);};
  $('dl-save').onclick=async()=>{const name=prompt('Waypoint name:','Waypoint');if(name!=null){const r=await window.desertLink.command('save-waypoint',{name});showToast(r?.ok?'Waypoint saved':r?.error);}};

  window.desertLink.onState(applyState);
  window.desertLink.onWaypoints(w=>{waypoints=Array.isArray(w)?w:[];renderWaypoints();});
  window.desertLink.getState().then(applyState).catch(()=>{});
  window.desertLink.getWaypoints().then(w=>{waypoints=Array.isArray(w)?w:[];renderWaypoints();}).catch(()=>{});
  setInterval(updateMapVisuals,100);
  setTimeout(updateMapVisuals,500);
})();
