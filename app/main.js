'use strict';

const { app, BrowserWindow, ipcMain, globalShortcut, shell, dialog, screen } = require('electron');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const net = require('net');

const APP_NAME = 'DesertLink – Crimson Desert Companion';
const MAP_URL = 'https://mapgenie.io/crimson-desert/maps/pywel';
const TELEMETRY_SNAPSHOT = 'http://127.0.0.1:27311/v1/snapshot';
const TELEMETRY_HEALTH = 'http://127.0.0.1:27311/v1/health';
const CORE_PIPE = '\\\\.\\pipe\\DesertLinkCore-v1';

// Two measured reference pairs per realm. These are data points, not copied UI/code.
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

let win = null;
let coreSocket = null;
let coreConnected = false;
let coreBuffer = '';
let coreReconnectTimer = null;
let quitting = false;
let lastBackendStatus = { attached: false, hookInstalled: false, physicsReady: false, supportedBuild: false, hookMode: 'none', message: 'Waiting for DesertLinkCore.asi…' };
let lastTelemetryHealth = null;
let lastSnapshot = null;
let lastPosition = null;
let lastPositionPacket = null;
let preTeleport = null;
let telemetryWs = null;
let telemetryPollTimer = null;
let backendEnsureTimer = null;
let mapCenter = null;
let lastBackendLogKey = '';
let authWindows = new Set();
let browserUserAgent = null;

const userData = () => app.getPath('userData');
const waypointsPath = () => path.join(userData(), 'waypoints.json');
const settingsPath = () => path.join(userData(), 'settings.json');
const logPath = () => path.join(userData(), 'desertlink.log');
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
  try { fs.mkdirSync(path.dirname(file), { recursive: true }); fs.writeFileSync(file, JSON.stringify(value, null, 2), 'utf8'); } catch {}
}

let settings = { follow: true, teleportY: null, rightClickTeleport: true, site: 'mapgenie', windowMode: 'app', overlayHotkey: 'End', appBounds: null };
let overlayHotkeyRegistered = false;
let waypoints = [];

function loadPersistence() {
  settings = { ...settings, ...readJson(settingsPath(), {}) };
  const w = readJson(waypointsPath(), []);
  waypoints = Array.isArray(w) ? w : [];
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

function broadcastState() {
  if (!win || win.isDestroyed()) return;
  const state = {
    app: { name: APP_NAME, version: app.getVersion() },
    telemetry: {
      connected: !!lastSnapshot,
      health: lastTelemetryHealth,
      supportedBuild: !!lastTelemetryHealth?.supportedBuild,
      gameBuild: lastTelemetryHealth?.gameBuild ?? null
    },
    game: {
      testedExactBuild: !!lastBackendStatus.supportedBuild && !!lastTelemetryHealth?.supportedBuild
    },
    teleport: lastBackendStatus,
    position: lastPositionPacket,
    settings,
    mapCenter,
    window: {
      mode: settings.windowMode || 'app',
      overlayHotkey: settings.overlayHotkey || 'End',
      overlayHotkeyRegistered,
      visible: !!win && !win.isDestroyed() && win.isVisible()
    }
  };
  win.webContents.send('dl-state', state);
}
function broadcastWaypoints() {
  if (win && !win.isDestroyed()) win.webContents.send('dl-waypoints', waypoints);
}

function coreWrite(line) {
  if (!coreSocket || !coreConnected || coreSocket.destroyed || !coreSocket.writable) return false;
  try { coreSocket.write(String(line).replace(/[\r\n]+/g, '') + '\n'); return true; } catch { return false; }
}

function sendBackend(obj) {
  const cmd = String(obj?.cmd || '').toLowerCase();
  if (cmd === 'ensure') return coreWrite('ENSURE');
  if (cmd === 'teleport') {
    const c = obj.current || {}, t = obj.target || {};
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
    const bk = `${hookInstalled}|${physicsReady}|${supportedBuild}|${hookMode}|${message}`;
    if (bk !== lastBackendLogKey) { lastBackendLogKey = bk; log(`Core status: ${bk}`); }
    lastBackendStatus = { attached: true, hookInstalled, physicsReady, supportedBuild, hookMode, message };
    broadcastState();
    return;
  }
  if (type === 'T') {
    const ok = parts[0] === '1';
    const error = parts.slice(1).join('|') || '';
    if (win && !win.isDestroyed()) win.webContents.send('dl-state', { toast: ok ? 'Teleport complete' : `Teleport failed: ${error || 'unknown error'}` });
    coreWrite('ENSURE');
    return;
  }
  if (type === 'E') log(`Core error: ${parts.join('|')}`);
}

function scheduleCoreReconnect() {
  if (quitting || coreReconnectTimer) return;
  coreReconnectTimer = setTimeout(() => { coreReconnectTimer = null; connectCore(); }, 1200);
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
    lastBackendStatus = { attached: false, hookInstalled: false, physicsReady: false, supportedBuild: false, hookMode: 'none', message: 'Waiting for DesertLinkCore.asi - start Crimson Desert and check the ASI installation' };
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
  const x = +pos.x, y = +pos.y, z = +pos.z;
  const realm = y > 1400 ? 'abyss' : 'pywel';
  const [lng, lat] = gameToMap(x, z, realm);
  const heading = Number.isFinite(+snap.player?.orientation?.headingDegrees) ? +snap.player.orientation.headingDegrees : null;
  lastPosition = { x, y, z, realm };
  lastPositionPacket = { x, y, z, realm, lng, lat, heading };
  if (settings.teleportY == null) settings.teleportY = y + 1.5;
  broadcastState();
}

async function startTelemetry() {
  if (telemetryWs) { try { telemetryWs.close(); } catch {} telemetryWs = null; }
  clearInterval(telemetryPollTimer);
  telemetryPollTimer = null;
  await fetchHealth();
  startTelemetryPolling();
}

function startTelemetryPolling() {
  if (telemetryPollTimer) return;
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

async function handleCommand(msg) {
  const cmd = msg?.cmd;
  if (cmd === 'teleport-map') return teleportMap(msg.lng, msg.lat, msg.y, msg.realm);
  if (cmd === 'teleport-absolute') return teleportAbsolute({ x: +msg.x, y: targetY(msg), z: +msg.z });
  if (cmd === 'abort') {
    if (!preTeleport) return { ok: false, error: 'No previous teleport position' };
    const p = preTeleport; preTeleport = null;
    return teleportAbsolute({ x: p.x, y: p.y, z: p.z }, false);
  }
  if (cmd === 'set-y') {
    if (Number.isFinite(+msg.y)) { settings.teleportY = +msg.y; saveSettings(); broadcastState(); return { ok: true }; }
    return { ok: false };
  }
  if (cmd === 'set-follow') { settings.follow = !!msg.value; saveSettings(); broadcastState(); return { ok: true }; }
  if (cmd === 'set-map-center') {
    if (Number.isFinite(+msg.lng) && Number.isFinite(+msg.lat)) mapCenter = { lng: +msg.lng, lat: +msg.lat, realm: msg.realm || lastPosition?.realm || 'pywel' };
    return { ok: true };
  }
  if (cmd === 'save-waypoint') {
    if (!lastPositionPacket) return { ok: false, error: 'No player position' };
    const name = String(msg.name || `Waypoint ${waypoints.length + 1}`).trim().slice(0, 64) || `Waypoint ${waypoints.length + 1}`;
    waypoints.push({ id: crypto.randomUUID(), name, ...lastPositionPacket, createdAt: new Date().toISOString() });
    saveWaypoints(); return { ok: true };
  }
  if (cmd === 'delete-waypoint') {
    const before = waypoints.length; waypoints = waypoints.filter(w => w.id !== msg.id); saveWaypoints(); return { ok: waypoints.length !== before };
  }
  if (cmd === 'teleport-waypoint') {
    const w = waypoints.find(x => x.id === msg.id); if (!w) return { ok: false, error: 'Waypoint not found' };
    return teleportAbsolute({ x: +w.x, y: +w.y, z: +w.z });
  }
  if (cmd === 'set-window-mode') {
    const mode = String(msg.mode || '').toLowerCase();
    if (mode !== 'app' && mode !== 'overlay') return { ok: false, error: 'Window mode must be app or overlay' };
    if (settings.windowMode === mode) return { ok: true };
    if ((settings.windowMode || 'app') === 'app') saveAppBounds();
    settings.windowMode = mode; saveSettings();
    setTimeout(() => recreateWindow(), 80);
    return { ok: true };
  }
  if (cmd === 'set-overlay-hotkey') {
    const accel = String(msg.hotkey || '').trim().slice(0, 64);
    if (!accel) return { ok: false, error: 'Choose a hotkey' };
    const old = settings.overlayHotkey || 'End';
    settings.overlayHotkey = accel;
    registerHotkeys();
    if (!overlayHotkeyRegistered) {
      settings.overlayHotkey = old;
      registerHotkeys();
      return { ok: false, error: `Hotkey ${accel} is unavailable. Try another key.` };
    }
    saveSettings(); broadcastState();
    return { ok: true, hotkey: settings.overlayHotkey };
  }
  if (cmd === 'toggle-window') { toggleOverlayWindow(); return { ok: true }; }
  if (cmd === 'reload-map') { win?.reload(); return { ok: true }; }
  if (cmd === 'open-external') { if (msg.url) shell.openExternal(String(msg.url)); return { ok: true }; }
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
}

function toggleOverlayWindow() {
  if (!win || win.isDestroyed()) return;
  if ((settings.windowMode || 'app') !== 'overlay') {
    if (win.isMinimized()) win.restore();
    win.show();
    win.focus();
    return;
  }
  if (win.isVisible() && win.isFocused()) {
    win.hide();
  } else if (win.isVisible()) {
    win.hide();
  } else {
    applyWindowMode();
    win.show();
    win.setAlwaysOnTop(true, 'screen-saver');
    win.focus();
  }
  broadcastState();
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
  if (overlay) { opts.x = ob.x; opts.y = ob.y; }
  else if (saved && Number.isFinite(saved.x) && Number.isFinite(saved.y)) { opts.x = saved.x; opts.y = saved.y; }

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
    setTimeout(() => { broadcastState(); broadcastWaypoints(); }, 700);
  });
  win.webContents.on('did-navigate-in-page', () => setTimeout(injectUi, 300));
  win.on('move', () => { if (settings.windowMode === 'app') saveAppBounds(); });
  win.on('resize', () => { if (settings.windowMode === 'app') saveAppBounds(); });
  win.on('show', broadcastState);
  win.on('hide', broadcastState);
  win.on('close', () => { if (settings.windowMode === 'app') saveAppBounds(); });
  win.loadURL(MAP_URL);
  win.once('ready-to-show', () => { if (win && !win.isDestroyed()) win.show(); });
}

function recreateWindow() {
  const old = win;
  if (old && !old.isDestroyed()) {
    try { old.destroy(); } catch {}
  }
  win = null;
  createWindow();
  registerHotkeys();
}

function registerHotkeys() {
  globalShortcut.unregisterAll();
  overlayHotkeyRegistered = false;
  try { globalShortcut.register('F5', async () => {
    if (!mapCenter) return;
    await teleportMap(mapCenter.lng, mapCenter.lat, settings.teleportY, mapCenter.realm);
  }); } catch {}
  try { globalShortcut.register('Shift+F5', async () => { await handleCommand({ cmd: 'abort' }); }); } catch {}
  try { globalShortcut.register('CommandOrControl+Shift+M', () => {
    if (!win || win.isDestroyed()) return;
    win.webContents.send('dl-panel-toggle');
  }); } catch {}
  try {
    overlayHotkeyRegistered = !!globalShortcut.register(settings.overlayHotkey || 'End', toggleOverlayWindow);
  } catch { overlayHotkeyRegistered = false; }
  broadcastState();
}

ipcMain.handle('dl-command', (_e, msg) => handleCommand(msg));
ipcMain.handle('dl-get-state', () => ({ telemetry: lastTelemetryHealth, teleport: lastBackendStatus, position: lastPositionPacket, settings, mapCenter, window: { mode: settings.windowMode || 'app', overlayHotkey: settings.overlayHotkey || 'End', overlayHotkeyRegistered, visible: !!win && !win.isDestroyed() && win.isVisible() } }));
ipcMain.handle('dl-get-waypoints', () => waypoints);

if (!app.requestSingleInstanceLock()) {
  app.quit();
} else {
  app.on('second-instance', () => { if (win) { if (settings.windowMode === 'overlay') toggleOverlayWindow(); else { if (win.isMinimized()) win.restore(); win.show(); win.focus(); } } });
}

app.whenReady().then(async () => {
  app.setName(APP_NAME);
  log('DesertLink starting');
  loadPersistence();
  createWindow();
  registerHotkeys();
  startBackend();
  startTelemetry();
  setInterval(() => { fetchHealth().then(broadcastState); }, 5000);
});

app.on('before-quit', () => {
  quitting = true;
  globalShortcut.unregisterAll();
  clearInterval(backendEnsureTimer);
  clearInterval(telemetryPollTimer);
  if (telemetryWs) try { telemetryWs.close(); } catch {}
  if (coreReconnectTimer) clearTimeout(coreReconnectTimer);
  if (coreSocket) { try { coreSocket.destroy(); } catch {} }
  coreSocket = null;
  coreConnected = false;
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
