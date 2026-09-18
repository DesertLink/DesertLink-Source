# DesertLink v1.2

Source repository for **DesertLink – Crimson Desert Map Companion**.

DesertLink combines a live world map, real-time player tracking, map-based teleport, persistent waypoints, App/Overlay modes, and remappable keyboard and mouse controls.

## Current compatibility

- Crimson Desert 2.02.00
- `CrimsonDesert.exe` 1.0.0.2850
- Steam build 25246367

Game-side teleport functionality is fail-closed on unsupported builds.

## Components

### Desktop companion

Electron application source:

- `app/main.js`
- `app/preload.js`
- `app/inject.js`
- `app/package.json`

Current v1.2 features include:

- live player marker and XYZ coordinates;
- map-click, map-center, and return teleport;
- persistent named waypoints with rename support and map markers;
- resizable/movable companion panel with saved layout;
- App Mode and Overlay Mode;
- remappable keyboard and mouse controls with duplicate/unavailable binding checks;
- Game Focus Mode for overlay use;
- persistent map authentication through Electron's dedicated `persist:desertlink` partition.

### DesertLinkCore.asi

`DesertLinkCore.asi` is the in-process teleport bridge. It communicates with the desktop app through the local Windows named pipe:

`\\.\pipe\DesertLinkCore-v1`

The maintained game-side source is `core/DesertLinkCore.cpp`.

### Installer

The public installer is built with standard Inno Setup and installs only the desktop application. It creates Desktop and Start Menu shortcuts.

The installer:

- does not install ASI plugins automatically;
- does not use PowerShell;
- performs no runtime downloads;
- bundles the Electron runtime during the release build.

## External requirements

- a working Crimson Desert ASI loader;
- `CrimsonDesertTelemetry.asi` for live position telemetry;
- `DesertLinkCore.asi` from the DesertLink release for teleport integration.

`CrimsonDesertTelemetry.asi` is an external dependency and is not distributed in this repository.

## Network behavior

`DesertLinkCore.asi` uses local IPC only.

The desktop app loads the MapGenie Crimson Desert map/login service, so the desktop application requires normal web access while using that service. The installer itself performs no runtime downloads.

## Build instructions

See `BUILD.md`.

## Security and Nexus review

See `SECURITY.md` and `docs/NEXUS_REVIEW.md`.

## Release hashes

See `SHA256SUMS.txt`.
