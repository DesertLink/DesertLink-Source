# Nexus Mods Review Notes

Project: DesertLink - Crimson Desert Map Companion
Version: 1.0.6

## Why the upload may trigger automated review

The release contains compiled Windows code:

1. `DesertLink_App_Setup_v1.0.6_PUBLIC.exe`
2. `DesertLinkCore_v1.0.6.asi`

`DesertLinkCore.asi` is a game modification plugin and performs an in-process runtime hook required for teleport functionality. This may resemble trainer behavior to heuristic scanners.

The desktop installer does not download files and does not perform game-memory operations.

## Architecture

```text
CrimsonDesertTelemetry.asi
        |
        | live telemetry
        v
DesertLink desktop app
        |
        | local named pipe IPC
        v
DesertLinkCore.asi
        |
        | in-process teleport integration
        v
Crimson Desert
```

## Internet use

- DesertLinkCore: local IPC only, no remote networking.
- Installer: no downloads / no Internet communication.
- Desktop app: web access is used to load the map/login service that is part of the companion UI.

## Release hashes

Setup:
`5c73a28a69eeeda1852075aba11419876cfba64d0ec777c4515e2ff7658bd2c7`

Core ASI:
`41e94e45fe314fdf61d92fd4dbe6a357529c6e819fc0c7a91d1b67f815c6be1c`

## Source/build documentation

- `core/DesertLinkCore.cpp`
- `installer/DesertLinkSetup.cpp`
- `app/`
- `BUILD.md`
- `SECURITY.md`
- `SHA256SUMS.txt`
