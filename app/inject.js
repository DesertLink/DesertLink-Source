(() => {
  'use strict';

  if (window.__desertLinkInjectedV120) return;
  window.__desertLinkInjectedV120 = true;
  if (!window.desertLink) return;

  let state = { settings: { follow: true, bindings: {} }, bindings: {}, position: null, teleport: {}, telemetry: {}, game: {} };
  let waypoints = [];
  let map = null;
  let mapListenersBound = false;
  let follow = true;
  let lastFollowAt = 0;
  let currentY = null;
  let toastTimer = null;
  let panelHidden = false;
  let bindingsVisible = false;
  let bindingCaptureAction = null;
  let lastCursorPoint = null;
  let lastCursorSentAt = 0;
  let restoredMapView = false;
  const waypointMarkers = new Map();

  const ACTION_ORDER = [
    'teleportMapClick',
    'teleportMapCenter',
    'returnPrevious',
    'toggleCompanion',
    'togglePanel',
    'toggleFollow',
    'reloadMap',
    'useCurrentY',
    'saveCurrentPosition',
    'toggleWindowMode',
    'gameFocusMode'
  ];

  const ACTION_LABELS = {
    teleportMapClick: 'Teleport to Map Click',
    teleportMapCenter: 'Teleport Map Center',
    returnPrevious: 'Return / Abort Previous',
    toggleCompanion: 'Show / Hide Companion',
    togglePanel: 'Show / Hide Panel',
    toggleFollow: 'Toggle Follow',
    reloadMap: 'Reload Map',
    useCurrentY: 'Use Current Y',
    saveCurrentPosition: 'Save Current Position',
    toggleWindowMode: 'Toggle App / Overlay Mode',
    gameFocusMode: 'Game Focus Mode'
  };

  const css = `
  #dl-panel{position:fixed;z-index:2147483600;top:18px;left:18px;width:360px;min-width:290px;max-width:min(620px,calc(100vw - 8px));height:auto;min-height:220px;max-height:calc(100vh - 8px);color:#eef5ff;background:rgba(7,12,18,.93);border:1px solid rgba(255,255,255,.13);border-radius:14px;box-shadow:0 18px 55px rgba(0,0,0,.48);backdrop-filter:blur(14px);font:13px/1.35 Inter,Segoe UI,Arial,sans-serif;overflow:hidden;user-select:none;resize:none}
  #dl-panel *{box-sizing:border-box}
  #dl-head{display:flex;align-items:center;gap:9px;padding:12px 10px 12px 13px;background:linear-gradient(100deg,rgba(193,35,45,.28),rgba(20,28,38,.2));border-bottom:1px solid rgba(255,255,255,.08);cursor:move;touch-action:none}
  #dl-logo{font-weight:900;font-size:17px;letter-spacing:.2px}#dl-logo b{color:#ff4555}#dl-sub{font-size:10px;opacity:.64;margin-left:auto}
  .dl-headbtn{width:28px;height:25px;border:1px solid rgba(255,255,255,.15);border-radius:7px;background:#111a23;color:#eaf2fb;cursor:pointer;font-weight:900;line-height:1}.dl-headbtn:hover{background:#24313e}
  #dl-tab{position:fixed;z-index:2147483601;top:18px;left:18px;display:none;padding:8px 12px;border-radius:10px;background:linear-gradient(105deg,#7c1d29,#161f29);border:1px solid rgba(255,255,255,.18);color:#fff;font:800 12px/1 Segoe UI,Arial;box-shadow:0 8px 26px #0008;cursor:pointer;user-select:none}#dl-tab b{color:#ff5261}
  #dl-body{padding:10px 12px 12px;overflow:auto;max-height:calc(100vh - 80px)}
  .dl-row{display:flex;gap:7px;align-items:center;margin:7px 0}.dl-chip{display:inline-flex;align-items:center;gap:5px;padding:4px 7px;border-radius:999px;background:#17202a;border:1px solid #2b3947;font-size:11px}.dl-dot{width:7px;height:7px;border-radius:50%;background:#7c8793}.dl-ok .dl-dot{background:#45d483;box-shadow:0 0 9px #45d483}.dl-warn .dl-dot{background:#ffb24d}.dl-bad .dl-dot{background:#ff5261}
  #dl-coords{font-family:Consolas,monospace;font-size:12px;background:#0a1016;border:1px solid #1d2a36;border-radius:9px;padding:8px 9px;display:grid;grid-template-columns:1fr 1fr;gap:3px 12px}.dl-muted{opacity:.62}
  .dl-btn{border:1px solid #314151;background:#15202a;color:#eef5ff;border-radius:9px;padding:7px 9px;font-weight:650;cursor:pointer;transition:.13s}.dl-btn:hover{background:#21303d;border-color:#506477}.dl-btn.primary{background:#9f2633;border-color:#c03c49}.dl-btn.primary:hover{background:#bd3140}.dl-btn.danger{background:#35171c;border-color:#71303a}.dl-btn.active{outline:2px solid #4fd69b55;border-color:#4fd69b}.dl-btn:disabled{opacity:.45;cursor:not-allowed}.dl-grow{flex:1}.dl-input{width:92px;border:1px solid #30404e;background:#0c131a;color:white;border-radius:8px;padding:7px 8px;font-family:Consolas,monospace}.dl-label{font-size:11px;opacity:.7}.dl-sep{height:1px;background:rgba(255,255,255,.08);margin:10px 0}.dl-mini{font-size:10px;opacity:.62}.dl-settinglabel{width:76px;font-size:11px;opacity:.72;flex:none}
  .dl-waypoints{max-height:170px;overflow:auto;margin-top:6px}.dl-wp{display:flex;align-items:center;gap:6px;padding:5px 0;border-bottom:1px solid rgba(255,255,255,.05)}.dl-wp-name{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;flex:1;cursor:default}.dl-iconbtn{width:29px;height:27px;padding:0;border-radius:7px;border:1px solid #2c3a47;background:#131d26;color:#fff;cursor:pointer}.dl-iconbtn:hover{background:#23313d}.dl-delete{color:#ff7d86}
  .dl-modegrp{display:flex;gap:5px;flex:1}.dl-modebtn{flex:1}.dl-modebtn.active{border-color:#ff5967;background:#33171d;box-shadow:0 0 0 1px #ff596733 inset}
  #dl-bindings{display:none;margin-top:8px;padding:8px;border:1px solid #263746;border-radius:10px;background:#0a1118}.dl-binding-row{display:grid;grid-template-columns:1fr 138px;gap:8px;align-items:center;padding:4px 0}.dl-binding-name{font-size:11px;opacity:.86}.dl-binding-btn{font:11px Consolas,monospace;text-align:center;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.dl-binding-btn.listening{border-color:#ffb24d;color:#ffd596}.dl-binding-help{margin-top:7px;padding-top:7px;border-top:1px solid rgba(255,255,255,.07)}
  #dl-marker{position:fixed;z-index:2147483500;width:24px;height:24px;pointer-events:none;display:none;transform:translate(-50%,-50%)}#dl-marker:before{content:'';position:absolute;left:5px;top:5px;width:14px;height:14px;border-radius:50%;background:#ff3348;border:3px solid white;box-shadow:0 2px 9px #000,0 0 0 5px rgba(255,51,72,.25)}#dl-arrow{position:absolute;left:9px;top:-8px;width:0;height:0;border-left:4px solid transparent;border-right:4px solid transparent;border-bottom:9px solid #fff;transform-origin:4px 20px}
  #dl-crosshair{position:fixed;z-index:2147483400;width:18px;height:18px;pointer-events:none;display:none;opacity:.72}#dl-crosshair:before,#dl-crosshair:after{content:'';position:absolute;background:white;box-shadow:0 0 3px #000}#dl-crosshair:before{width:18px;height:1px;top:8px}#dl-crosshair:after{width:1px;height:18px;left:8px}
  .dl-wp-marker{position:fixed;z-index:2147483450;display:none;pointer-events:none;transform:translate(-50%,-50%);filter:drop-shadow(0 2px 4px #000)}.dl-wp-dot{width:11px;height:11px;border-radius:50%;background:#f6c54b;border:2px solid #fff;box-shadow:0 0 0 3px rgba(246,197,75,.22)}.dl-wp-label{position:absolute;left:14px;top:-5px;max-width:170px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;padding:2px 6px;border-radius:6px;background:rgba(8,12,17,.88);border:1px solid rgba(255,255,255,.14);font:700 10px Segoe UI,Arial;color:#fff}
  #dl-resize{position:absolute;right:3px;bottom:3px;width:18px;height:18px;cursor:nwse-resize;opacity:.55}#dl-resize:before{content:'';position:absolute;right:2px;bottom:2px;width:10px;height:10px;border-right:2px solid #8fa2b5;border-bottom:2px solid #8fa2b5}
  #dl-toast{position:fixed;z-index:2147483646;right:24px;bottom:24px;max-width:420px;padding:11px 14px;border-radius:10px;background:#101820;color:white;border:1px solid #334656;box-shadow:0 10px 30px #0008;opacity:0;transform:translateY(8px);transition:.2s;pointer-events:none;font:13px Segoe UI,Arial}.show{opacity:1!important;transform:none!important}
  `;

  const style = document.createElement('style');
  style.id = 'dl-style';
  style.textContent = css;
  document.documentElement.appendChild(style);

  const panel = document.createElement('div');
  panel.id = 'dl-panel';
  panel.innerHTML = `
    <div id="dl-head">
      <div id="dl-logo">Desert<b>Link</b></div>
      <div class="dl-chip" id="dl-realm">PYWEL</div>
      <div id="dl-sub">Companion 1.2</div>
      <button class="dl-headbtn" id="dl-collapse" title="Hide panel">—</button>
    </div>
    <div id="dl-body">
      <div class="dl-row"><span class="dl-chip" id="dl-tel"><span class="dl-dot"></span><span>Telemetry</span></span><span class="dl-chip" id="dl-tp"><span class="dl-dot"></span><span>Teleport</span></span><span class="dl-chip" id="dl-build"><span class="dl-dot"></span><span>Build</span></span></div>
      <div id="dl-coords"><span class="dl-muted">X</span><span id="dl-x">—</span><span class="dl-muted">Y</span><span id="dl-y">—</span><span class="dl-muted">Z</span><span id="dl-z">—</span></div>
      <div class="dl-row"><button class="dl-btn dl-grow" id="dl-follow">Follow: ON</button><button class="dl-btn" id="dl-reload" title="Reload map">↻</button></div>
      <div class="dl-row"><span class="dl-settinglabel">Window</span><div class="dl-modegrp"><button class="dl-btn dl-modebtn" id="dl-mode-app">App</button><button class="dl-btn dl-modebtn" id="dl-mode-overlay">Overlay</button></div></div>
      <div class="dl-row"><button class="dl-btn dl-grow" id="dl-gamefocus">Game Focus: OFF</button><button class="dl-btn" id="dl-bindings-toggle">Show Bindings</button></div>
      <div class="dl-row"><span class="dl-label">Teleport Y</span><input class="dl-input" id="dl-ty" type="number" step="1"><button class="dl-btn" id="dl-usey">Use current Y</button></div>
      <div class="dl-row"><button class="dl-btn primary dl-grow" id="dl-center">◎ Teleport Map Center</button><button class="dl-btn danger" id="dl-abort">↩ Return</button></div>
      <div class="dl-mini" id="dl-binding-summary"></div>
      <div id="dl-bindings"></div>
      <div class="dl-sep"></div>
      <div class="dl-row"><b>Waypoints</b><button class="dl-btn dl-grow" id="dl-save">+ Save current position</button></div>
      <div class="dl-waypoints" id="dl-waypoints"></div>
      <div class="dl-sep"></div>
      <div class="dl-mini" id="dl-statusline">Waiting for CrimsonDesertTelemetry…</div>
    </div>
    <div id="dl-resize" title="Resize panel"></div>`;
  document.body.appendChild(panel);

  const panelTab = document.createElement('div');
  panelTab.id = 'dl-tab';
  panelTab.innerHTML = 'Desert<b>Link</b>';
  document.body.appendChild(panelTab);

  const marker = document.createElement('div');
  marker.id = 'dl-marker';
  marker.innerHTML = '<div id="dl-arrow"></div>';
  document.body.appendChild(marker);

  const cross = document.createElement('div');
  cross.id = 'dl-crosshair';
  document.body.appendChild(cross);

  const toast = document.createElement('div');
  toast.id = 'dl-toast';
  document.body.appendChild(toast);

  const $ = id => document.getElementById(id);
  const fmt = n => Number.isFinite(+n) ? (+n).toFixed(1) : '—';

  function showToast(text) {
    if (!text) return;
    toast.textContent = text;
    toast.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove('show'), 2600);
  }

  function chip(id, mode, text) {
    const el = $(id);
    el.classList.remove('dl-ok', 'dl-warn', 'dl-bad');
    el.classList.add(mode);
    el.querySelector('span:last-child').textContent = text;
  }

  function getBindings() {
    return state.bindings || state.settings?.bindings || {};
  }

  function displayBinding(binding) {
    if (!binding) return 'Unbound';
    return String(binding)
      .replace('CommandOrControl', 'Ctrl')
      .replace('MouseLeft', 'Left Mouse')
      .replace('MouseMiddle', 'Middle Mouse')
      .replace('MouseRight', 'Right Mouse');
  }

  function normalizeKeyboardEvent(e) {
    const key = String(e.key || '');
    if (['Control', 'Shift', 'Alt', 'Meta', 'AltGraph'].includes(key)) return null;
    if (key === 'Escape') return 'Escape';
    if (key === 'Backspace' || key === 'Delete') return 'Unbound';

    const keyMap = {
      ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right',
      ' ': 'Space', PageUp: 'PageUp', PageDown: 'PageDown'
    };
    let finalKey = keyMap[key] || key;
    if (/^F\d{1,2}$/i.test(finalKey)) finalKey = finalKey.toUpperCase();
    else if (finalKey.length === 1) finalKey = finalKey.toUpperCase();
    else finalKey = finalKey.charAt(0).toUpperCase() + finalKey.slice(1);

    const mods = [];
    if (e.ctrlKey || e.metaKey) mods.push('CommandOrControl');
    if (e.altKey) mods.push('Alt');
    if (e.shiftKey) mods.push('Shift');
    return [...mods, finalKey].join('+');
  }

  function mouseBindingFromButton(button) {
    return button === 0 ? 'MouseLeft' : button === 1 ? 'MouseMiddle' : button === 2 ? 'MouseRight' : button === 3 ? 'Mouse4' : button === 4 ? 'Mouse5' : null;
  }

  function actionForBinding(binding) {
    const bindings = getBindings();
    return ACTION_ORDER.find(action => bindings[action] === binding) || null;
  }

  async function setBinding(action, binding) {
    const result = await window.desertLink.command('set-binding', { action, binding: binding === 'Unbound' ? null : binding });
    if (!result?.ok) showToast(result?.error || 'Binding could not be saved');
    else showToast(`${ACTION_LABELS[action]}: ${displayBinding(result.binding)}`);
    bindingCaptureAction = null;
    renderBindings();
  }

  function renderBindingSummary() {
    const b = getBindings();
    const parts = [
      `Map click: ${displayBinding(b.teleportMapClick)}`,
      `Center: ${displayBinding(b.teleportMapCenter)}`,
      `Return: ${displayBinding(b.returnPrevious)}`,
      `Panel: ${displayBinding(b.togglePanel)}`
    ];
    $('dl-binding-summary').textContent = parts.join(' · ');
  }

  function renderBindings() {
    renderBindingSummary();
    const box = $('dl-bindings');
    box.style.display = bindingsVisible ? 'block' : 'none';
    $('dl-bindings-toggle').textContent = bindingsVisible ? 'Hide Bindings' : 'Show Bindings';
    if (!bindingsVisible) return;

    const bindings = getBindings();
    box.textContent = '';
    for (const action of ACTION_ORDER) {
      const row = document.createElement('div');
      row.className = 'dl-binding-row';
      const label = document.createElement('div');
      label.className = 'dl-binding-name';
      label.textContent = ACTION_LABELS[action];
      const btn = document.createElement('button');
      btn.className = 'dl-btn dl-binding-btn';
      btn.dataset.action = action;
      const listening = bindingCaptureAction === action;
      btn.classList.toggle('listening', listening);
      btn.textContent = listening ? 'Press key / mouse…' : displayBinding(bindings[action]);
      btn.onclick = e => {
        e.preventDefault();
        e.stopPropagation();
        bindingCaptureAction = action;
        renderBindings();
      };
      row.append(label, btn);
      box.appendChild(row);
    }

    const footer = document.createElement('div');
    footer.className = 'dl-binding-help dl-mini';
    footer.innerHTML = 'Click a binding, then press a key or mouse button.<br>Esc cancels · Backspace/Delete clears · one input can only belong to one action.';
    const reset = document.createElement('button');
    reset.className = 'dl-btn';
    reset.style.marginTop = '8px';
    reset.textContent = 'Reset Defaults';
    reset.onclick = async () => {
      bindingCaptureAction = null;
      const r = await window.desertLink.command('reset-bindings');
      if (!r?.ok) showToast(r?.error || 'Could not reset bindings');
      else showToast('Bindings reset to defaults');
    };
    footer.appendChild(document.createElement('br'));
    footer.appendChild(reset);
    box.appendChild(footer);
  }

  window.addEventListener('keydown', e => {
    if (!bindingCaptureAction) return;
    e.preventDefault();
    e.stopPropagation();
    const value = normalizeKeyboardEvent(e);
    if (!value) return;
    if (value === 'Escape') {
      bindingCaptureAction = null;
      renderBindings();
      return;
    }
    setBinding(bindingCaptureAction, value);
  }, true);

  window.addEventListener('mousedown', e => {
    if (!bindingCaptureAction) return;
    const value = mouseBindingFromButton(e.button);
    if (!value) return;
    e.preventDefault();
    e.stopPropagation();
    setBinding(bindingCaptureAction, value);
  }, true);

  function clampPanel(left, top, width = panel.offsetWidth || 360, height = panel.offsetHeight || 500) {
    const maxLeft = Math.max(4, window.innerWidth - Math.min(width, window.innerWidth - 8) - 4);
    const maxTop = Math.max(4, window.innerHeight - Math.min(height, 80) - 4);
    return { left: Math.max(4, Math.min(left, maxLeft)), top: Math.max(4, Math.min(top, maxTop)) };
  }

  async function persistPanelLayout() {
    const rect = panel.getBoundingClientRect();
    await window.desertLink.command('set-panel-layout', {
      value: { left: rect.left, top: rect.top, width: rect.width, height: rect.height, hidden: panelHidden }
    });
  }

  function applyPanelLayout() {
    const v = state.settings?.panelLayout;
    const width = Math.max(290, Math.min(Number(v?.width) || 360, Math.max(290, window.innerWidth - 8)));
    const height = Math.max(220, Math.min(Number(v?.height) || 520, Math.max(220, window.innerHeight - 8)));
    panel.style.width = width + 'px';
    panel.style.height = height + 'px';
    const p = clampPanel(Number.isFinite(+v?.left) ? +v.left : 18, Number.isFinite(+v?.top) ? +v.top : 18, width, height);
    panel.style.left = p.left + 'px';
    panel.style.top = p.top + 'px';
    panelTab.style.left = p.left + 'px';
    panelTab.style.top = p.top + 'px';
    setPanelHidden(!!v?.hidden, false);
  }

  function setPanelHidden(hidden, persist = true) {
    panelHidden = !!hidden;
    panel.style.display = panelHidden ? 'none' : 'block';
    panelTab.style.display = panelHidden ? 'block' : 'none';
    if (persist) persistPanelLayout();
  }

  function togglePanel() { setPanelHidden(!panelHidden, true); }

  let drag = null;
  $('dl-head').addEventListener('pointerdown', e => {
    if (e.button !== 0 || e.target.closest('button,input,a')) return;
    const r = panel.getBoundingClientRect();
    drag = { dx: e.clientX - r.left, dy: e.clientY - r.top };
    try { $('dl-head').setPointerCapture(e.pointerId); } catch {}
    e.preventDefault();
    e.stopPropagation();
  }, true);

  $('dl-head').addEventListener('pointermove', e => {
    if (!drag) return;
    const p = clampPanel(e.clientX - drag.dx, e.clientY - drag.dy);
    panel.style.left = p.left + 'px';
    panel.style.top = p.top + 'px';
    panelTab.style.left = p.left + 'px';
    panelTab.style.top = p.top + 'px';
    e.preventDefault();
    e.stopPropagation();
  }, true);

  const finishDrag = e => {
    if (!drag) return;
    drag = null;
    persistPanelLayout();
    try { $('dl-head').releasePointerCapture(e.pointerId); } catch {}
  };
  $('dl-head').addEventListener('pointerup', finishDrag, true);
  $('dl-head').addEventListener('pointercancel', finishDrag, true);

  let resizing = null;
  $('dl-resize').addEventListener('pointerdown', e => {
    if (e.button !== 0) return;
    const r = panel.getBoundingClientRect();
    resizing = { x: e.clientX, y: e.clientY, width: r.width, height: r.height };
    try { $('dl-resize').setPointerCapture(e.pointerId); } catch {}
    e.preventDefault();
    e.stopPropagation();
  }, true);

  $('dl-resize').addEventListener('pointermove', e => {
    if (!resizing) return;
    const maxW = Math.max(290, window.innerWidth - panel.getBoundingClientRect().left - 4);
    const maxH = Math.max(220, window.innerHeight - panel.getBoundingClientRect().top - 4);
    panel.style.width = Math.max(290, Math.min(maxW, resizing.width + e.clientX - resizing.x)) + 'px';
    panel.style.height = Math.max(220, Math.min(maxH, resizing.height + e.clientY - resizing.y)) + 'px';
    e.preventDefault();
    e.stopPropagation();
  }, true);

  const finishResize = e => {
    if (!resizing) return;
    resizing = null;
    persistPanelLayout();
    try { $('dl-resize').releasePointerCapture(e.pointerId); } catch {}
  };
  $('dl-resize').addEventListener('pointerup', finishResize, true);
  $('dl-resize').addEventListener('pointercancel', finishResize, true);

  $('dl-collapse').onclick = e => { e.preventDefault(); e.stopPropagation(); setPanelHidden(true, true); };
  panelTab.onclick = () => setPanelHidden(false, true);
  $('dl-bindings-toggle').onclick = () => { bindingsVisible = !bindingsVisible; bindingCaptureAction = null; renderBindings(); };

  window.addEventListener('resize', () => {
    const r = panel.getBoundingClientRect();
    const width = Math.min(r.width, Math.max(290, window.innerWidth - 8));
    const height = Math.min(r.height, Math.max(220, window.innerHeight - 8));
    panel.style.width = width + 'px';
    panel.style.height = height + 'px';
    const p = clampPanel(r.left, r.top, width, height);
    panel.style.left = p.left + 'px';
    panel.style.top = p.top + 'px';
    panelTab.style.left = p.left + 'px';
    panelTab.style.top = p.top + 'px';
    persistPanelLayout();
  });

  function findMap() {
    const okay = o => o && typeof o.getCenter === 'function' && typeof o.project === 'function' && typeof o.unproject === 'function' && (typeof o.easeTo === 'function' || typeof o.flyTo === 'function');
    const obvious = [window.map, window.mapboxMap, window.mapManager?.map, window.mapManager];
    for (const o of obvious) if (okay(o)) return o;
    for (const key of Object.getOwnPropertyNames(window)) {
      if (!/map/i.test(key)) continue;
      try {
        const o = window[key];
        if (okay(o)) return o;
        if (okay(o?.map)) return o.map;
      } catch {}
    }
    return null;
  }

  function mapRect() {
    if (!map) return null;
    try {
      const container = typeof map.getContainer === 'function' ? map.getContainer() : map.getCanvas?.()?.parentElement;
      return container?.getBoundingClientRect?.() || null;
    } catch {
      return null;
    }
  }

  function pointFromEvent(e, canvas) {
    if (!map || !canvas) return null;
    const rect = canvas.getBoundingClientRect();
    try {
      const ll = map.unproject([e.clientX - rect.left, e.clientY - rect.top]);
      if (!ll || !Number.isFinite(+ll.lng) || !Number.isFinite(+ll.lat)) return null;
      return { lng: +ll.lng, lat: +ll.lat, realm: state.position?.realm || 'pywel' };
    } catch {
      return null;
    }
  }

  async function runAction(action, context = {}) {
    const result = await window.desertLink.command('run-action', { action, context });
    if (!result?.ok && result?.error) showToast(result.error);
    else if (action === 'teleportMapClick' || action === 'teleportMapCenter') showToast('Teleport sent');
    else if (action === 'returnPrevious') showToast('Returning…');
    else if (action === 'saveCurrentPosition' && result?.name) showToast(`Saved ${result.name}`);
    return result;
  }

  function bindMap() {
    if (!map || mapListenersBound) return;
    let canvas = null;
    try { canvas = map.getCanvas(); } catch {}
    if (!canvas) return;
    mapListenersBound = true;

    canvas.addEventListener('pointermove', e => {
      const p = pointFromEvent(e, canvas);
      if (!p) return;
      lastCursorPoint = p;
      const now = Date.now();
      if (now - lastCursorSentAt > 120) {
        lastCursorSentAt = now;
        window.desertLink.command('set-map-cursor', p);
      }
    }, { passive: true });

    canvas.addEventListener('pointerdown', async e => {
      const binding = mouseBindingFromButton(e.button);
      const action = binding ? actionForBinding(binding) : null;
      if (!action) {
        lastFollowAt = Date.now() + 1000;
        return;
      }
      e.preventDefault();
      e.stopPropagation();
      const p = pointFromEvent(e, canvas) || lastCursorPoint;
      const y = Number($('dl-ty').value);
      await runAction(action, { mapPoint: p, y: Number.isFinite(y) ? y : undefined, realm: state.position?.realm });
    }, true);

    canvas.addEventListener('contextmenu', e => {
      if (actionForBinding('MouseRight')) e.preventDefault();
    }, true);

    canvas.addEventListener('wheel', () => { lastFollowAt = Date.now() + 1400; }, { passive: true });
  }

  function ensureWaypointMarker(w) {
    let el = waypointMarkers.get(w.id);
    if (!el) {
      el = document.createElement('div');
      el.className = 'dl-wp-marker';
      const dot = document.createElement('div');
      dot.className = 'dl-wp-dot';
      const label = document.createElement('div');
      label.className = 'dl-wp-label';
      el.append(dot, label);
      document.body.appendChild(el);
      waypointMarkers.set(w.id, el);
    }
    el.querySelector('.dl-wp-label').textContent = w.name || 'Waypoint';
    return el;
  }

  function syncWaypointMarkers() {
    const ids = new Set(waypoints.map(w => w.id));
    for (const [id, el] of waypointMarkers) {
      if (!ids.has(id)) {
        el.remove();
        waypointMarkers.delete(id);
      }
    }
    for (const w of waypoints) ensureWaypointMarker(w);
  }

  function restoreMapViewOnce() {
    if (restoredMapView || !map) return;
    restoredMapView = true;
    const v = state.settings?.mapView;
    if (!v || !Number.isFinite(+v.lng) || !Number.isFinite(+v.lat) || !Number.isFinite(+v.zoom)) return;
    const currentRealm = state.position?.realm || v.realm || 'pywel';
    if (v.realm && currentRealm && v.realm !== currentRealm) return;
    try {
      const fn = typeof map.jumpTo === 'function' ? 'jumpTo' : (typeof map.easeTo === 'function' ? 'easeTo' : 'flyTo');
      map[fn]({ center: [+v.lng, +v.lat], zoom: +v.zoom, duration: 0, essential: true });
    } catch {}
  }

  function updateMapVisuals() {
    if (!map) {
      map = findMap();
      if (map) {
        bindMap();
        syncWaypointMarkers();
        restoreMapViewOnce();
        showToast('Map connected');
      }
    }

    if (!map) {
      marker.style.display = 'none';
      cross.style.display = 'none';
      for (const el of waypointMarkers.values()) el.style.display = 'none';
      return;
    }

    const rect = mapRect();
    if (!rect) return;
    cross.style.display = 'block';
    cross.style.left = (rect.left + rect.width / 2 - 9) + 'px';
    cross.style.top = (rect.top + rect.height / 2 - 9) + 'px';

    try {
      const c = map.getCenter();
      const zoom = typeof map.getZoom === 'function' ? +map.getZoom() : null;
      if (c && Number.isFinite(+c.lng) && Number.isFinite(+c.lat)) {
        window.desertLink.command('set-map-center', { lng: +c.lng, lat: +c.lat, realm: state.position?.realm });
        if (Number.isFinite(zoom)) window.desertLink.command('set-map-view', { lng: +c.lng, lat: +c.lat, zoom, realm: state.position?.realm });
      }
    } catch {}

    const realm = state.position?.realm || 'pywel';
    for (const w of waypoints) {
      const el = ensureWaypointMarker(w);
      if (w.realm && w.realm !== realm) {
        el.style.display = 'none';
        continue;
      }
      if (!Number.isFinite(+w.lng) || !Number.isFinite(+w.lat)) {
        el.style.display = 'none';
        continue;
      }
      try {
        const p = map.project([+w.lng, +w.lat]);
        el.style.display = 'block';
        el.style.left = (rect.left + p.x) + 'px';
        el.style.top = (rect.top + p.y) + 'px';
      } catch {
        el.style.display = 'none';
      }
    }

    if (!state.position) {
      marker.style.display = 'none';
      return;
    }

    try {
      const p = map.project([state.position.lng, state.position.lat]);
      marker.style.display = 'block';
      marker.style.left = (rect.left + p.x) + 'px';
      marker.style.top = (rect.top + p.y) + 'px';
      const bearing = typeof map.getBearing === 'function' ? +map.getBearing() : 0;
      const heading = Number.isFinite(+state.position.heading) ? +state.position.heading : 0;
      $('dl-arrow').style.transform = `rotate(${heading - bearing}deg)`;

      if (follow && Date.now() > lastFollowAt) {
        const now = Date.now();
        if (now - lastFollowAt > 250) {
          lastFollowAt = now;
          const fn = typeof map.easeTo === 'function' ? 'easeTo' : 'flyTo';
          map[fn]({ center: [state.position.lng, state.position.lat], duration: 180, essential: true });
        }
      }
    } catch {
      marker.style.display = 'none';
    }
  }

  async function renameWaypoint(w) {
    const next = prompt('Waypoint name:', w.name || 'Waypoint');
    if (next == null) return;
    const r = await window.desertLink.command('rename-waypoint', { id: w.id, name: next });
    if (!r?.ok) showToast(r?.error || 'Could not rename waypoint');
    else showToast(`Renamed to ${r.name}`);
  }

  function renderWaypoints() {
    syncWaypointMarkers();
    const box = $('dl-waypoints');
    box.textContent = '';
    if (!waypoints.length) {
      box.innerHTML = '<div class="dl-mini">No saved waypoints yet.</div>';
      return;
    }

    [...waypoints].reverse().slice(0, 40).forEach(w => {
      const row = document.createElement('div');
      row.className = 'dl-wp';

      const name = document.createElement('span');
      name.className = 'dl-wp-name';
      name.textContent = w.name;
      name.title = `X ${fmt(w.x)}  Y ${fmt(w.y)}  Z ${fmt(w.z)} · double-click to rename`;
      name.ondblclick = () => renameWaypoint(w);

      const rename = document.createElement('button');
      rename.className = 'dl-iconbtn';
      rename.textContent = '✎';
      rename.title = 'Rename';
      rename.onclick = () => renameWaypoint(w);

      const go = document.createElement('button');
      go.className = 'dl-iconbtn';
      go.textContent = '◎';
      go.title = 'Teleport';
      go.onclick = async () => {
        const r = await window.desertLink.command('teleport-waypoint', { id: w.id });
        showToast(r?.ok ? 'Teleport sent' : r?.error);
      };

      const del = document.createElement('button');
      del.className = 'dl-iconbtn dl-delete';
      del.textContent = '×';
      del.title = 'Delete';
      del.onclick = () => window.desertLink.command('delete-waypoint', { id: w.id });

      row.append(name, rename, go, del);
      box.appendChild(row);
    });
  }

  function renderWindowState() {
    const mode = state.window?.mode || state.settings?.windowMode || 'app';
    $('dl-mode-app').classList.toggle('active', mode === 'app');
    $('dl-mode-overlay').classList.toggle('active', mode === 'overlay');
    const focus = !!state.window?.gameFocusMode;
    $('dl-gamefocus').textContent = `Game Focus: ${focus ? 'ON' : 'OFF'}`;
    $('dl-gamefocus').classList.toggle('active', focus);
    $('dl-gamefocus').disabled = mode !== 'overlay';
  }

  function applyState(s) {
    if (!s) return;
    if (s.toast) showToast(s.toast);
    state = { ...state, ...s };

    const p = state.position;
    $('dl-x').textContent = fmt(p?.x);
    $('dl-y').textContent = fmt(p?.y);
    $('dl-z').textContent = fmt(p?.z);
    $('dl-realm').textContent = (p?.realm || '—').toUpperCase();

    if (p && currentY == null) {
      currentY = p.y + 1.5;
      if (!$('dl-ty').value) $('dl-ty').value = currentY.toFixed(1);
    }

    const tel = !!state.telemetry?.connected;
    chip('dl-tel', tel ? 'dl-ok' : 'dl-bad', tel ? 'Telemetry OK' : 'Telemetry OFF');

    const tp = state.teleport || {};
    chip('dl-tp', tp.physicsReady ? 'dl-ok' : (tp.hookInstalled ? 'dl-warn' : 'dl-bad'), tp.physicsReady ? 'Teleport Ready' : (tp.hookInstalled ? 'Waiting Physics' : 'Teleport Offline'));

    const exact = !!state.game?.testedExactBuild;
    chip('dl-build', exact ? 'dl-ok' : (state.telemetry?.health ? 'dl-warn' : 'dl-bad'), exact ? '2.02 Exact' : (state.telemetry?.health ? 'Other Build' : 'No Game'));

    follow = state.settings?.follow !== false;
    $('dl-follow').textContent = 'Follow: ' + (follow ? 'ON' : 'OFF');
    $('dl-follow').classList.toggle('active', follow);

    if (Number.isFinite(+state.settings?.teleportY) && document.activeElement !== $('dl-ty')) {
      $('dl-ty').value = (+state.settings.teleportY).toFixed(1);
    }

    $('dl-statusline').textContent = tp.message || (!tel ? 'Install/start CrimsonDesertTelemetry and load into the world.' : 'Ready.');
    renderWindowState();
    renderBindings();
  }

  $('dl-follow').onclick = () => runAction('toggleFollow');
  $('dl-reload').onclick = () => runAction('reloadMap');
  $('dl-gamefocus').onclick = () => runAction('gameFocusMode');
  $('dl-usey').onclick = async () => {
    const r = await runAction('useCurrentY');
    if (r?.ok && Number.isFinite(+r.y)) $('dl-ty').value = (+r.y).toFixed(1);
  };
  $('dl-ty').onchange = () => {
    const y = Number($('dl-ty').value);
    if (Number.isFinite(y)) window.desertLink.command('set-y', { y });
  };
  $('dl-center').onclick = () => runAction('teleportMapCenter', { y: Number($('dl-ty').value), realm: state.position?.realm });
  $('dl-abort').onclick = () => runAction('returnPrevious');
  $('dl-save').onclick = async () => {
    const defaultName = `Waypoint ${waypoints.length + 1}`;
    const name = prompt('Waypoint name:', defaultName);
    if (name == null) return;
    await runAction('saveCurrentPosition', { name });
  };
  $('dl-mode-app').onclick = async () => {
    if ((state.window?.mode || state.settings?.windowMode || 'app') === 'app') return;
    const r = await window.desertLink.command('set-window-mode', { mode: 'app' });
    if (!r?.ok) showToast(r?.error || 'Could not switch mode');
    else showToast('App Mode saved. Restarting DesertLink is required.');
  };
  $('dl-mode-overlay').onclick = async () => {
    if ((state.window?.mode || state.settings?.windowMode || 'app') === 'overlay') return;
    const r = await window.desertLink.command('set-window-mode', { mode: 'overlay' });
    if (!r?.ok) showToast(r?.error || 'Could not switch mode');
    else showToast('Overlay Mode saved. Restarting DesertLink is required.');
  };

  window.desertLink.onAction(message => {
    const action = message?.action;
    if (action === 'togglePanel') togglePanel();
  });

  window.desertLink.onState(applyState);
  window.desertLink.onWaypoints(list => {
    waypoints = Array.isArray(list) ? list : [];
    renderWaypoints();
  });

  window.desertLink.getState().then(s => {
    applyState(s);
    applyPanelLayout();
  }).catch(() => {});
  window.desertLink.getWaypoints().then(list => {
    waypoints = Array.isArray(list) ? list : [];
    renderWaypoints();
  }).catch(() => {});

  setInterval(updateMapVisuals, 100);
  setTimeout(updateMapVisuals, 500);
})();
