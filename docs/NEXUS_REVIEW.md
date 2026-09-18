# Nexus Mods Review Notes

Project: DesertLink – Crimson Desert Map Companion  
Version: 1.2

## Release components

The public package contains:

1. `DesertLink_Setup_v1.2.exe`
2. `DesertLinkCore.asi`
3. `README.txt`

The installer is a standard Inno Setup package. It installs only the Electron desktop companion and creates normal Windows shortcuts.

`DesertLinkCore.asi` is loaded through the user's ASI loader and provides the in-process teleport bridge. It communicates with the desktop companion over a local Windows named pipe.

## Architecture

```text
CrimsonDesertTelemetry.asi
        |
        | local live telemetry
        v
DesertLink desktop companion
        |
        | local named-pipe IPC
        v
DesertLinkCore.asi
        |
        | in-process teleport integration
        v
Crimson Desert
```

## Network behavior

- `DesertLinkCore.asi`: no remote networking.
- Installer: no runtime downloads.
- Desktop companion: web access is used for the MapGenie map/login service.

## Build compatibility

Current target:

- Crimson Desert 2.02.00
- `CrimsonDesert.exe` 1.0.0.2850
- Steam build 25246367

The game-side component fails closed when its executable checks do not match.

## Source/build documentation

- `app/`
- `core/DesertLinkCore.cpp`
- `installer/DesertLink.iss`
- `BUILD.md`
- `SECURITY.md`
- `SHA256SUMS.txt`
