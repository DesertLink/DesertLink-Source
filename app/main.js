'use strict';

const { app, BrowserWindow, ipcMain, globalShortcut, shell, screen } = require('electron');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const net = require('net');

const APP_NAME = 'DesertLink – Crimson Desert Companion';
const MAP_URL = 'https://mapgenie.io/crimson-desert/maps/pywel';
const TELEMETRY_SNAPSHOT = 'http://127.0.0.1:27311/v1/snapshot';
const TELEMETRY_HEALTH = 'http://127.0.0.1:27311/v1/health';
const CORE_PIPE = '\\\\.\\pipe\\DesertLinkCore-v1';

const CAL = {
  pywel: [
    { game: [-12127.138259887695, 7.692434787750244], map: [-0.9052420615140191, 0.7787327582867241] },
    { game: [-3690.7935791015625, -6117.512298583984], map: [-0.5555426902317491, 0.5248899410143244] }
  ],
  abyss: [
    { game: [-10679.2001953125, -3686.5693359375], map: [-1.3021820027444733, 0.6476022163899415] },
    { game: [-12273.085479736328, -4988.257263183594], map: [-1.3517201468401367, 0.6072151985198246] }
  ]
};

const ACTIONS = Object.freeze({
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
});

const DEFAULT_BINDINGS = Object.freeze({
  teleportMapClick: 'MouseRight',
  teleportMapCenter: 'F5',
  returnPrevious: 'Shift+F5',
  toggleCompanion: 'End',
  togglePanel: 'CommandOrControl+Shift+M',
  toggleFollow: null,
  reloadMap: null,
  useCurrentY: null,
  saveCurrentPosition: null,
  toggleWindowMode: null,
  gameFocusMode: 'CommandOrControl+Shift+G'
});

let win = null;
let browserUserAgent = null;
let authWindows = new Set();
let quitting = false;

let coreSocket = null;
let coreConnected = false;
let coreBuffer = '';
let coreReconnectTimer = null;
let backendEnsureTimer = null;
let lastBackendStatus = {
  attached: false,
  hookInstalled: false,
  physicsReady: false,
  supportedBuild: false,
  hookMode: 'none',
  message: 'Waiting for DesertLinkCore.asi…'
};

let lastTelemetryHealth = null;
let lastSnapshot = null;
let lastPosition = null;
let lastPositionPacket = null;
let telemetryPollTimer = null;
let preTeleport = null;
let mapCenter = null;
let mapCursor = null;
let lastBackendLogKey = '';
let registeredKeyboardBindings = new Set();

const userData = () => app.getPath('userData');
const settingsPath = () => path.join(userData(), 'settings.json');
const waypointsPath = () => path.join(userData(), 'waypoints.json');
const logPath = () => path.join(userData(), 'desertlink.log');

const baseSettings = () => ({
  follow: true,
  teleportY: null,
  site: 'mapgenie',
  windowMode: 'app',
  appBounds: null,
  gameFocusMode: false,
  panelLayout: null,
  mapView: null,
  bindings: { ...DEFAULT_BINDINGS }
});

let settings = baseSettings();
let waypoints = [];

function log(message) {
  try {
    fs.mkdirSync(userData(), { recursive: true });
    fs.appendFileSync(logPath(), `[${new Date().toISOString()}] ${message}\n`, 'utf8');
  } catch {}
}

function readJson(file, fallback) {
  try { return JSON.parse(fs.readFileSync(file, 'utf8')); } catch { return fallback; }
}

function writeJson(file, value) {
  try {
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, JSON.stringify(value, null, 2), 'utf8');
  } catch {}
}

function normalizeBinding(value) {
  if (value == null) return null;
  const raw = String(value).trim();
  if (!raw) return null;
  const mouse = {
    mouseleft: 'MouseLeft',
    mousemiddle: 'MouseMiddle',
    mouseright: 'MouseRight',
    mouse4: 'Mouse4',
    mouse5: 'Mouse5'
  };
  const m = mouse[raw.toLowerCase()];
  if (m) return m;
  return raw
    .replace(/^Ctrl\+/i, 'CommandOrControl+')
    .replace(/^Control\+/i, 'CommandOrControl+');
}

function isMouseBinding(binding) {
  return /^Mouse(?:Left|Middle|Right|4|5)$/.test(String(binding || ''));
}

function migrateSettings(raw) {
  const next = { ...baseSettings(), ...(raw && typeof raw === 'object' ? raw : {}) };
  next.bindings = { ...DEFAULT_BINDINGS, ...(raw?.bindings && typeof raw.bindings === 'object' ? raw.bindings : {}) };

  if (raw?.overlayHotkey && !raw?.bindings?.toggleCompanion) {
    next.bindings.toggleCompanion = normalizeBinding(raw.overlayHotkey);
  }
  if (raw?.rightClickTeleport === false && !raw?.bindings?.teleportMapClick) {
    next.bindings.teleportMapClick = null;
  }

  for (const key of Object.keys(DEFAULT_BINDINGS)) {
    next.bindings[key] = normalizeBinding(next.bindings[key]);
  }

  delete next.overlayHotkey;
  delete next.rightClickTeleport;
  return next;
}

function loadPersistence() {
  settings = migrateSettings(readJson(settingsPath(), {}));
  const loaded = readJson(waypointsPath(), []);
  waypoints = Array.isArray(loaded) ? loaded.filter(w => w && typeof w === 'object') : [];
  saveSettings();
}

function saveSettings() { writeJson(settingsPath(), settings); }
function saveWaypoints() { writeJson(waypointsPath(), waypoints); broadcastWaypoints(); }

function gameToMap(x, z, realm) {
  const pts = CAL[realm] || CAL.pywel;
  const [p0, p1] = pts;
  const sx = (p1.map[0] - p0.map[0]) / (p1.game[0] - p0.game[0]);
  const sz = (p1.map[1] - p0.map[1]) / (p1.game[1] - p0.game[1]);
  return [x * sx + (p0.map[0] - p0.game[0] * sx), z * sz + (p0.map[1] - p0.game[1] * sz)];
}

function mapToGame(lng, lat, realm) {
  const pts = CAL[realm] || CAL.pywel;
  const [p0, p1] = pts;
  const sx = (p1.map[0] - p0.map[0]) / (p1.game[0] - p0.game[0]);
  const sz = (p1.map[1] - p0.map[1]) / (p1.game[1] - p0.game[1]);
  if (Math.abs(sx) < 1e-12 || Math.abs(sz) < 1e-12) return null;
  const ox = p0.map[0] - p0.game[0] * sx;
  const oz = p0.map[1] - p0.game[1] * sz;
  return [(lng - ox) / sx, (lat - oz) / sz];
}

function publicState() {
  return {
    app: { name: APP_NAME, version: app.getVersion() },
    telemetry: {
      connected: !!lastSnapshot,
      health: lastTelemetryHealth,
      supportedBuild: !!lastTelemetryHealth?.supportedBuild,
      gameBuild: lastTelemetryHealth?.gameBuild ?? null
    },
    game: {
      testedExactBuild: !!lastBackendStatus.supportedBuild && !!lastTelemetryHealth?.supportedBuild,
      targetVersion: '2.02.00',
      targetExeVersion: '1.0.0.2850',
      targetSteamBuild: '25246367'
    },
    teleport: lastBackendStatus,
    position: lastPositionPacket,
    settings,
    bindings: settings.bindings,
    actions: ACTIONS,
    mapCenter,
    mapCursor,
    window: {
      mode: settings.windowMode || 'app',
      visible: !!win && !win.isDestroyed() && win.isVisible(),
      gameFocusMode: !!settings.gameFocusMode,
      keyboardBindingsRegistered: [...registeredKeyboardBindings]
    }
  };
}

function broadcastState(extra = null) {
  if (!win || win.isDestroyed()) return;
  win.webContents.send('dl-state', extra ? { ...publicState(), ...extra } : publicState());
}

function broadcastWaypoints() {
  if (win && !win.isDestroyed()) win.webContents.send('dl-waypoints', waypoints);
}

function sendRendererAction(action, context = {}) {
  if (!win || win.isDestroyed()) return false;
  win.webContents.send('dl-action', { action, context });
  return true;
}

function coreWrite(line) {
  if (!coreSocket || !coreConnected || coreSocket.destroyed || !coreSocket.writable) return false;
  try {
    coreSocket.write(String(line).replace(/[\r\n]+/g, '') + '\n');
    return true;
  } catch {
    return false;
  }
}

function sendBackend(obj) {
  const cmd = String(obj?.cmd || '').toLowerCase();
  if (cmd === 'ensure') return coreWrite('ENSURE');
  if (cmd === 'teleport') {
    const c = obj.current || {};
    const t = obj.target || {};
    const vals = [c.x, c.y, c.z, t.x, t.y, t.z].map(Number);
    if (!vals.every(Number.isFinite)) return false;
    return coreWrite(`TELEPORT ${vals.join(' ')}`);
  }
  return false;
}

function handleCoreLine(line) {
  line = String(line || '').trim();
  if (!line) return;
  const parts = line.split('|');
  const type = parts.shift();

  if (type === 'HELLO') {
    log(`DesertLinkCore connected: ${parts[0] || 'unknown version'}`);
    coreWrite('ENSURE');
    return;
  }

  if (type === 'S') {
    const hookInstalled = parts[0] === '1';
    const physicsReady = parts[1] === '1';
    const supportedBuild = parts[2] === '1';
    const hookMode = parts[3] || 'none';
    const message = parts.slice(4).join('|') || '';
    const key = `${hookInstalled}|${physicsReady}|${supportedBuild}|${hookMode}|${message}`;
    if (key !== lastBackendLogKey) {
      lastBackendLogKey = key;
      log(`Core status: ${key}`);
    }
    lastBackendStatus = { attached: true, hookInstalled, physicsReady, supportedBuild, hookMode, message };
    broadcastState();
    return;
  }

  if (type === 'T') {
    const ok = parts[0] === '1';
    const error = parts.slice(1).join('|') || '';
    broadcastState({ toast: ok ? 'Teleport complete' : `Teleport failed: ${error || 'unknown error'}` });
    coreWrite('ENSURE');
    return;
  }

  if (type === 'E') log(`Core error: ${parts.join('|')}`);
}

function scheduleCoreReconnect() {
  if (quitting || coreReconnectTimer) return;
  coreReconnectTimer = setTimeout(() => {
    coreReconnectTimer = null;
    connectCore();
  }, 1200);
}

function connectCore() {
  if (quitting || coreConnected || (coreSocket && !coreSocket.destroyed)) return;
  const socket = net.createConnection(CORE_PIPE);
  coreSocket = socket;
  coreBuffer = '';
  socket.setEncoding('utf8');

  socket.on('connect', () => {
    if (socket !== coreSocket) return;
    coreConnected = true;
    lastBackendStatus = { ...lastBackendStatus, attached: true, message: 'Connected to DesertLinkCore.asi' };
    broadcastState();
    coreWrite('ENSURE');
  });

  socket.on('data', chunk => {
    if (socket !== coreSocket) return;
    coreBuffer += chunk;
    for (;;) {
      const i = coreBuffer.indexOf('\n');
      if (i < 0) break;
      const line = coreBuffer.slice(0, i);
      coreBuffer = coreBuffer.slice(i + 1);
      handleCoreLine(line);
    }
    if (coreBuffer.length > 8192) coreBuffer = coreBuffer.slice(-4096);
  });

  socket.on('error', () => {});
  socket.on('close', () => {
    if (socket !== coreSocket) return;
    coreConnected = false;
    coreSocket = null;
    lastBackendStatus = {
      attached: false,
      hookInstalled: false,
      physicsReady: false,
      supportedBuild: false,
      hookMode: 'none',
      message: 'Waiting for DesertLinkCore.asi - start Crimson Desert and check the ASI installation'
    };
    broadcastState();
    scheduleCoreReconnect();
  });
}

function startBackend() {
  connectCore();
  clearInterval(backendEnsureTimer);
  backendEnsureTimer = setInterval(() => {
    if (coreConnected) coreWrite('ENSURE');
    else connectCore();
  }, 1200);
}

async function fetchHealth() {
  try {
    const r = await fetch(TELEMETRY_HEALTH, { cache: 'no-store', signal: AbortSignal.timeout(1200) });
    if (!r.ok) throw new Error(String(r.status));
    lastTelemetryHealth = await r.json();
  } catch {
    lastTelemetryHealth = null;
  }
}

function handleSnapshot(snap) {
  if (!snap || typeof snap !== 'object') return;
  lastSnapshot = snap;
  const pos = snap.player?.position;
  if (!pos || !Number.isFinite(+pos.x) || !Number.isFinite(+pos.y) || !Number.isFinite(+pos.z)) return;

  const x = +pos.x;
  const y = +pos.y;
  const z = +pos.z;
  const realm = y > 1400 ? 'abyss' : 'pywel';
  const [lng, lat] = gameToMap(x, z, realm);
  const heading = Number.isFinite(+snap.player?.orientation?.headingDegrees) ? +snap.player.orientation.headingDegrees : null;

  lastPosition = { x, y, z, realm };
  lastPositionPacket = { x, y, z, realm, lng, lat, heading };
  if (settings.teleportY == null) {
    settings.teleportY = y + 1.5;
    saveSettings();
  }
  broadcastState();
}

function startTelemetryPolling() {
  clearInterval(telemetryPollTimer);
  telemetryPollTimer = setInterval(async () => {
    try {
      const r = await fetch(TELEMETRY_SNAPSHOT, { cache: 'no-store', signal: AbortSignal.timeout(900) });
      if (!r.ok) throw new Error(String(r.status));
      handleSnapshot(await r.json());
      if (!lastTelemetryHealth || Math.random() < 0.03) await fetchHealth();
    } catch {
      lastSnapshot = null;
      lastPosition = null;
      lastPositionPacket = null;
      if (Math.random() < 0.15) await fetchHealth();
      broadcastState();
    }
  }, 50);
}

async function startTelemetry() {
  await fetchHealth();
  startTelemetryPolling();
}

function targetY(payload) {
  if (Number.isFinite(+payload?.y)) return +payload.y;
  if (Number.isFinite(+settings.teleportY)) return +settings.teleportY;
  if (lastPosition) return lastPosition.y + 1.5;
  return 1000;
}

async function teleportAbsolute(target, rememberReturn = true) {
  if (!lastPosition) return { ok: false, error: 'No live player position yet' };
  if (!lastBackendStatus.physicsReady) return { ok: false, error: lastBackendStatus.message || 'Teleport hook is not ready yet.' };
  if (![target.x, target.y, target.z].every(Number.isFinite)) return { ok: false, error: 'Invalid teleport coordinates' };

  if (rememberReturn) preTeleport = { ...lastPosition };
  log(`Teleport request: ${target.x.toFixed(2)}, ${target.y.toFixed(2)}, ${target.z.toFixed(2)}`);

  const ok = sendBackend({
    cmd: 'teleport',
    current: { x: lastPosition.x, y: lastPosition.y, z: lastPosition.z },
    target: { x: target.x, y: target.y, z: target.z }
  });

  return ok ? { ok: true } : { ok: false, error: 'Teleport backend is unavailable' };
}

async function teleportMap(lng, lat, y, realm) {
  realm = realm || lastPosition?.realm || 'pywel';
  const p = mapToGame(+lng, +lat, realm);
  if (!p) return { ok: false, error: 'Map calibration failed' };
  return teleportAbsolute({ x: p[0], y: targetY({ y }), z: p[1] });
}

function nextWaypointName() {
  let i = 1;
  const names = new Set(waypoints.map(w => String(w.name || '').toLowerCase()));
  while (names.has(`waypoint ${i}`)) i += 1;
  return `Waypoint ${i}`;
}

function saveCurrentWaypoint(name = null) {
  if (!lastPositionPacket) return { ok: false, error: 'No player position' };
  const finalName = String(name || nextWaypointName()).trim().slice(0, 64) || nextWaypointName();
  waypoints.push({
    id: crypto.randomUUID(),
    name: finalName,
    ...lastPositionPacket,
    createdAt: new Date().toISOString()
  });
  saveWaypoints();
  return { ok: true, name: finalName };
}

function applyGameFocusMode() {
  if (!win || win.isDestroyed()) return;
  const active = settings.windowMode === 'overlay' && settings.gameFocusMode === true;
  try { win.setIgnoreMouseEvents(active, { forward: true }); } catch {}
  broadcastState();
}

function toggleOverlayWindow() {
  if (!win || win.isDestroyed()) return;
  if ((settings.windowMode || 'app') !== 'overlay') {
    if (win.isMinimized()) win.restore();
    win.show();
    win.focus();
    return;
  }
  if (win.isVisible()) win.hide();
  else {
    applyWindowMode();
    win.show();
    if (!settings.gameFocusMode) win.focus();
  }
  broadcastState();
}

async function executeAction(action, context = {}) {
  switch (action) {
    case 'teleportMapClick': {
      const p = context.mapPoint || mapCursor;
      if (!p || !Number.isFinite(+p.lng) || !Number.isFinite(+p.lat)) return { ok: false, error: 'Move the cursor over the map first' };
      return teleportMap(+p.lng, +p.lat, context.y, context.realm || p.realm || lastPosition?.realm);
    }
    case 'teleportMapCenter': {
      if (!mapCenter) return { ok: false, error: 'Map center is not available yet' };
      return teleportMap(mapCenter.lng, mapCenter.lat, context.y, mapCenter.realm);
    }
    case 'returnPrevious': {
      if (!preTeleport) return { ok: false, error: 'No previous teleport position' };
      const p = preTeleport;
      preTeleport = null;
      return teleportAbsolute({ x: p.x, y: p.y, z: p.z }, false);
    }
    case 'toggleCompanion':
      toggleOverlayWindow();
      return { ok: true };
    case 'togglePanel':
      return { ok: sendRendererAction('togglePanel') };
    case 'toggleFollow':
      settings.follow = !settings.follow;
      saveSettings();
      broadcastState();
      return { ok: true, value: settings.follow };
    case 'reloadMap':
      if (win && !win.isDestroyed()) win.reload();
      return { ok: true };
    case 'useCurrentY':
      if (!lastPosition) return { ok: false, error: 'No player position' };
      settings.teleportY = lastPosition.y + 1.5;
      saveSettings();
      broadcastState();
      return { ok: true, y: settings.teleportY };
    case 'saveCurrentPosition':
      return saveCurrentWaypoint(context.name || null);
    case 'toggleWindowMode': {
      const next = (settings.windowMode || 'app') === 'overlay' ? 'app' : 'overlay';
      if ((settings.windowMode || 'app') === 'app') saveAppBounds();
      settings.windowMode = next;
      settings.gameFocusMode = false;
      saveSettings();
      setTimeout(() => app.quit(), 120);
      return { ok: true, mode: next, restartRequired: true };
    }
    case 'gameFocusMode':
      settings.gameFocusMode = !settings.gameFocusMode;
      saveSettings();
      applyGameFocusMode();
      return { ok: true, value: settings.gameFocusMode };
    default:
      return { ok: false, error: 'Unknown action' };
  }
}

function validateBindingSet(bindings) {
  const used = new Map();
  for (const action of Object.keys(DEFAULT_BINDINGS)) {
    const binding = normalizeBinding(bindings[action]);
    if (!binding) continue;
    const key = binding.toLowerCase();
    if (used.has(key)) return { ok: false, error: `${binding} is already assigned to ${ACTIONS[used.get(key)]}` };
    used.set(key, action);
  }
  return { ok: true };
}

function registerKeyboardBindings(bindings = settings.bindings) {
  globalShortcut.unregisterAll();
  registeredKeyboardBindings = new Set();

  const registered = [];
  for (const [action, bindingRaw] of Object.entries(bindings)) {
    const binding = normalizeBinding(bindingRaw);
    if (!binding || isMouseBinding(binding)) continue;
    let ok = false;
    try {
      ok = globalShortcut.register(binding, async () => {
        const r = await executeAction(action, {});
        if (!r?.ok && r?.error) broadcastState({ toast: r.error });
      });
    } catch {
      ok = false;
    }
    if (!ok) {
      globalShortcut.unregisterAll();
      registeredKeyboardBindings = new Set();
      return { ok: false, error: `Hotkey ${binding} is unavailable.` };
    }
    registered.push(binding);
    registeredKeyboardBindings.add(binding);
  }

  return { ok: true, registered };
}

function setBinding(action, bindingRaw) {
  if (!Object.prototype.hasOwnProperty.call(DEFAULT_BINDINGS, action)) return { ok: false, error: 'Unknown binding action' };
  const binding = normalizeBinding(bindingRaw);
  const candidate = { ...settings.bindings, [action]: binding };
  const valid = validateBindingSet(candidate);
  if (!valid.ok) return valid;

  const old = { ...settings.bindings };
  settings.bindings = candidate;
  const reg = registerKeyboardBindings(candidate);
  if (!reg.ok) {
    settings.bindings = old;
    registerKeyboardBindings(old);
    return reg;
  }

  saveSettings();
  broadcastState();
  return { ok: true, binding };
}

function resetBindings() {
  const candidate = { ...DEFAULT_BINDINGS };
  const reg = registerKeyboardBindings(candidate);
  if (!reg.ok) return reg;
  settings.bindings = candidate;
  saveSettings();
  broadcastState();
  return { ok: true };
}

async function handleCommand(msg) {
  const cmd = String(msg?.cmd || '');

  if (cmd === 'run-action') return executeAction(String(msg.action || ''), msg.context || {});
  if (cmd === 'teleport-map') return teleportMap(msg.lng, msg.lat, msg.y, msg.realm);
  if (cmd === 'teleport-absolute') return teleportAbsolute({ x: +msg.x, y: targetY(msg), z: +msg.z });
  if (cmd === 'abort') return executeAction('returnPrevious');

  if (cmd === 'set-y') {
    if (!Number.isFinite(+msg.y)) return { ok: false, error: 'Invalid Y value' };
    settings.teleportY = +msg.y;
    saveSettings();
    broadcastState();
    return { ok: true };
  }

  if (cmd === 'set-follow') {
    settings.follow = !!msg.value;
    saveSettings();
    broadcastState();
    return { ok: true };
  }

  if (cmd === 'set-map-center') {
    if (Number.isFinite(+msg.lng) && Number.isFinite(+msg.lat)) {
      mapCenter = { lng: +msg.lng, lat: +msg.lat, realm: msg.realm || lastPosition?.realm || 'pywel' };
    }
    return { ok: true };
  }

  if (cmd === 'set-map-cursor') {
    if (Number.isFinite(+msg.lng) && Number.isFinite(+msg.lat)) {
      mapCursor = { lng: +msg.lng, lat: +msg.lat, realm: msg.realm || lastPosition?.realm || 'pywel' };
    }
    return { ok: true };
  }

  if (cmd === 'set-map-view') {
    if (Number.isFinite(+msg.lng) && Number.isFinite(+msg.lat) && Number.isFinite(+msg.zoom)) {
      settings.mapView = { lng: +msg.lng, lat: +msg.lat, zoom: +msg.zoom, realm: msg.realm || lastPosition?.realm || 'pywel' };
      saveSettings();
    }
    return { ok: true };
  }

  if (cmd === 'save-waypoint') return saveCurrentWaypoint(msg.name || null);

  if (cmd === 'rename-waypoint') {
    const w = waypoints.find(x => x.id === msg.id);
    if (!w) return { ok: false, error: 'Waypoint not found' };
    const name = String(msg.name || '').trim().slice(0, 64);
    if (!name) return { ok: false, error: 'Waypoint name cannot be empty' };
    w.name = name;
    saveWaypoints();
    return { ok: true, name };
  }

  if (cmd === 'delete-waypoint') {
    const before = waypoints.length;
    waypoints = waypoints.filter(w => w.id !== msg.id);
    saveWaypoints();
    return { ok: waypoints.length !== before };
  }

  if (cmd === 'teleport-waypoint') {
    const w = waypoints.find(x => x.id === msg.id);
    if (!w) return { ok: false, error: 'Waypoint not found' };
    return teleportAbsolute({ x: +w.x, y: +w.y, z: +w.z });
  }

  if (cmd === 'set-window-mode') {
    const mode = String(msg.mode || '').toLowerCase();
    if (mode !== 'app' && mode !== 'overlay') return { ok: false, error: 'Window mode must be app or overlay' };
    if ((settings.windowMode || 'app') === mode) return { ok: true, mode, restartRequired: false };
    if ((settings.windowMode || 'app') === 'app') saveAppBounds();
    settings.windowMode = mode;
    settings.gameFocusMode = false;
    saveSettings();
    setTimeout(() => app.quit(), 120);
    return { ok: true, mode, restartRequired: true };
  }

  if (cmd === 'set-binding') return setBinding(String(msg.action || ''), msg.binding ?? null);
  if (cmd === 'reset-bindings') return resetBindings();

  if (cmd === 'set-panel-layout') {
    const v = msg.value;
    if (v && typeof v === 'object') {
      settings.panelLayout = v;
      saveSettings();
      broadcastState();
    }
    return { ok: true };
  }

  if (cmd === 'toggle-window') return executeAction('toggleCompanion');
  if (cmd === 'reload-map') return executeAction('reloadMap');
  if (cmd === 'open-external') {
    if (msg.url) shell.openExternal(String(msg.url));
    return { ok: true };
  }

  return { ok: false, error: 'Unknown command' };
}

function injectUi() {
  if (!win || win.isDestroyed()) return;
  try {
    const code = fs.readFileSync(path.join(__dirname, 'inject.js'), 'utf8');
    win.webContents.executeJavaScript(code, true).catch(() => {});
  } catch {}
}

function saveAppBounds() {
  if (!win || win.isDestroyed() || settings.windowMode !== 'app') return;
  try {
    if (!win.isMaximized() && !win.isMinimized()) {
      const b = win.getBounds();
      if (b.width >= 700 && b.height >= 500) {
        settings.appBounds = b;
        saveSettings();
      }
    }
  } catch {}
}

function overlayBounds() {
  try {
    const cursor = screen.getCursorScreenPoint();
    const d = screen.getDisplayNearestPoint(cursor) || screen.getPrimaryDisplay();
    return d.bounds;
  } catch {
    return { x: 0, y: 0, width: 1460, height: 900 };
  }
}

function applyWindowMode() {
  if (!win || win.isDestroyed()) return;
  const overlay = settings.windowMode === 'overlay';
  try {
    if (overlay) {
      const b = overlayBounds();
      win.setBounds(b, false);
      win.setAlwaysOnTop(true, 'screen-saver');
      win.setVisibleOnAllWorkspaces(true, { visibleOnFullScreen: true });
      win.setSkipTaskbar(true);
      win.setFullScreenable(false);
    } else {
      win.setAlwaysOnTop(false);
      win.setVisibleOnAllWorkspaces(false);
      win.setSkipTaskbar(false);
      win.setFullScreenable(true);
    }
  } catch {}
  applyGameFocusMode();
}

function createWindow() {
  const overlay = (settings.windowMode || 'app') === 'overlay';
  const saved = settings.appBounds && typeof settings.appBounds === 'object' ? settings.appBounds : null;
  const ob = overlay ? overlayBounds() : null;

  const opts = {
    width: overlay ? ob.width : (saved?.width || 1460),
    height: overlay ? ob.height : (saved?.height || 900),
    minWidth: overlay ? 700 : 900,
    minHeight: overlay ? 500 : 650,
    title: APP_NAME,
    backgroundColor: '#0b0f14',
    autoHideMenuBar: true,
    frame: !overlay,
    show: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false,
      partition: 'persist:desertlink'
    }
  };

  if (overlay) {
    opts.x = ob.x;
    opts.y = ob.y;
  } else if (saved && Number.isFinite(saved.x) && Number.isFinite(saved.y)) {
    opts.x = saved.x;
    opts.y = saved.y;
  }

  win = new BrowserWindow(opts);
  win.setMenuBarVisibility(false);
  applyWindowMode();

  browserUserAgent = win.webContents.getUserAgent().replace(/\sElectron\/[^ ]+/i, '');
  win.webContents.setUserAgent(browserUserAgent);

  win.webContents.setWindowOpenHandler(({ url }) => {
    if (!/^https?:\/\//i.test(String(url || ''))) return { action: 'deny' };
    return {
      action: 'allow',
      overrideBrowserWindowOptions: {
        width: 980,
        height: 780,
        minWidth: 620,
        minHeight: 520,
        parent: win,
        modal: false,
        autoHideMenuBar: true,
        backgroundColor: '#0b0f14',
        title: 'DesertLink – Sign in',
        webPreferences: {
          contextIsolation: true,
          nodeIntegration: false,
          sandbox: true,
          partition: 'persist:desertlink'
        }
      }
    };
  });

  win.webContents.on('did-create-window', child => {
    authWindows.add(child);
    child.setMenuBarVisibility(false);
    if (browserUserAgent) child.webContents.setUserAgent(browserUserAgent);

    const refreshParent = () => {
      if (!win || win.isDestroyed()) return;
      try { win.webContents.reloadIgnoringCache(); } catch {}
    };

    child.on('closed', () => {
      authWindows.delete(child);
      setTimeout(refreshParent, 250);
    });

    child.webContents.on('did-navigate', (_event, url) => {
      try {
        const u = new URL(url);
        const isMap = /(^|\.)mapgenie\.io$/i.test(u.hostname) && /\/crimson-desert\/maps\//i.test(u.pathname);
        if (isMap) {
          setTimeout(() => {
            refreshParent();
            if (!child.isDestroyed()) child.close();
          }, 350);
        }
      } catch {}
    });
  });

  win.webContents.on('did-finish-load', () => {
    setTimeout(injectUi, 300);
    setTimeout(() => {
      broadcastState();
      broadcastWaypoints();
    }, 700);
  });

  win.webContents.on('did-navigate-in-page', () => setTimeout(injectUi, 300));
  win.on('move', () => { if (settings.windowMode === 'app') saveAppBounds(); });
  win.on('resize', () => { if (settings.windowMode === 'app') saveAppBounds(); });
  win.on('show', broadcastState);
  win.on('hide', broadcastState);
  win.on('close', () => { if (settings.windowMode === 'app') saveAppBounds(); });

  win.loadURL(MAP_URL);
  win.once('ready-to-show', () => {
    if (!win || win.isDestroyed()) return;
    win.show();
    applyGameFocusMode();
  });
}

ipcMain.handle('dl-command', (_event, msg) => handleCommand(msg));
ipcMain.handle('dl-get-state', () => publicState());
ipcMain.handle('dl-get-waypoints', () => waypoints);

if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (!win || win.isDestroyed()) return;
    if (settings.windowMode === 'overlay') toggleOverlayWindow();
    else {
      if (win.isMinimized()) win.restore();
      win.show();
      win.focus();
    }
  });
}

app.whenReady().then(async () => {
  app.setName(APP_NAME);
  log('DesertLink starting');
  loadPersistence();

  const valid = validateBindingSet(settings.bindings);
  if (!valid.ok) settings.bindings = { ...DEFAULT_BINDINGS };
  const reg = registerKeyboardBindings(settings.bindings);
  if (!reg.ok) {
    log(`Binding registration fallback: ${reg.error}`);
    settings.bindings = { ...DEFAULT_BINDINGS };
    registerKeyboardBindings(settings.bindings);
    saveSettings();
  }

  createWindow();
  startBackend();
  startTelemetry();
  setInterval(() => { fetchHealth().then(broadcastState); }, 5000);
});

app.on('before-quit', () => {
  quitting = true;
  globalShortcut.unregisterAll();
  clearInterval(backendEnsureTimer);
  clearInterval(telemetryPollTimer);
  if (coreReconnectTimer) clearTimeout(coreReconnectTimer);
  if (coreSocket) {
    try { coreSocket.destroy(); } catch {}
  }
  coreSocket = null;
  coreConnected = false;
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
