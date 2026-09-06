'use strict';
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('desertLink', {
  command: (cmd, payload = {}) => ipcRenderer.invoke('dl-command', { cmd, ...payload }),
  getState: () => ipcRenderer.invoke('dl-get-state'),
  getWaypoints: () => ipcRenderer.invoke('dl-get-waypoints'),
  onState: (cb) => {
    if (typeof cb !== 'function') return () => {};
    const fn = (_event, state) => cb(state);
    ipcRenderer.on('dl-state', fn);
    return () => ipcRenderer.removeListener('dl-state', fn);
  },
  onWaypoints: (cb) => {
    if (typeof cb !== 'function') return () => {};
    const fn = (_event, data) => cb(data);
    ipcRenderer.on('dl-waypoints', fn);
    return () => ipcRenderer.removeListener('dl-waypoints', fn);
  },
  onPanelToggle: (cb) => {
    if (typeof cb !== 'function') return () => {};
    const fn = () => cb();
    ipcRenderer.on('dl-panel-toggle', fn);
    return () => ipcRenderer.removeListener('dl-panel-toggle', fn);
  }
});
