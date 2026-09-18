'use strict';

const { contextBridge, ipcRenderer } = require('electron');

function subscribe(channel, cb, transform = value => value) {
  if (typeof cb !== 'function') return () => {};
  const listener = (_event, value) => cb(transform(value));
  ipcRenderer.on(channel, listener);
  return () => ipcRenderer.removeListener(channel, listener);
}

contextBridge.exposeInMainWorld('desertLink', {
  command: (cmd, payload = {}) => ipcRenderer.invoke('dl-command', { cmd, ...payload }),
  getState: () => ipcRenderer.invoke('dl-get-state'),
  getWaypoints: () => ipcRenderer.invoke('dl-get-waypoints'),
  onState: cb => subscribe('dl-state', cb),
  onWaypoints: cb => subscribe('dl-waypoints', cb),
  onAction: cb => subscribe('dl-action', cb)
});
