# DesertLink v1.0.6

Source repository for **DesertLink - Crimson Desert Map Companion**.

This repository is published primarily for transparency, security review, and Nexus Mods moderation review.

## Components

### DesertLink desktop app
Electron-based map companion UI.

Source:
- `app/main.js`
- `app/preload.js`
- `app/inject.js`
- `app/package.json`

### DesertLinkCore.asi
Game-side Crimson Desert teleport bridge.

Source:
- `core/DesertLinkCore.cpp`

The ASI is loaded by the user's existing ASI loader and communicates with the DesertLink desktop app through the local Windows named pipe:

`\\.\pipe\DesertLinkCore-v1`

### DesertLink App Setup
Installs only the desktop application and creates Desktop / Start Menu shortcuts.

Source:
- `installer/DesertLinkSetup.cpp`
- `installer/chkstk.s`

The public app installer:
- does **not** install or modify ASI files;
- does **not** download files from the Internet;
- does **not** use PowerShell;
- asks a first-time user to select the official Electron v44.2.0 Windows x64 ZIP manually;
- verifies that ZIP by SHA-256 before installing it.

## External requirements

- CrimsonDesertTelemetry.asi
- A working Crimson Desert ASI loader
- Electron v44.2.0 Windows x64 runtime for the first desktop installation

CrimsonDesertTelemetry is an external dependency and is not included in this source repository.

## Network behavior

`DesertLinkCore.asi` does not use remote network APIs. Its app communication is local IPC only.

The desktop app embeds/loads the map web service used by DesertLink. This is the reason the desktop application itself requires normal web access while using the map/login service. The installer performs no Internet downloads.

## Release hashes

See `SHA256SUMS.txt`.

## Build instructions

See `BUILD.md`.

## Nexus Mods review

See `docs/NEXUS_REVIEW.md`.
