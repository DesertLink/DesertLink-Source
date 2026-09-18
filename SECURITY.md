# DesertLink v1.2 Security / Distribution Notes

## Public installer

The public installer is built with standard Inno Setup.

It:

- installs only the DesertLink desktop application;
- creates standard Desktop and Start Menu shortcuts;
- does not modify Crimson Desert;
- does not install ASI files automatically;
- performs no runtime downloads;
- uses no PowerShell;
- does not open or write to the Crimson Desert process.

The official Electron runtime is fetched and SHA-256 verified during the release build and is then bundled into the installer.

## DesertLinkCore.asi

DesertLinkCore is loaded in-process through the user's existing ASI loader and provides teleport integration for the supported Crimson Desert build.

The desktop companion communicates with DesertLinkCore through the local named pipe:

`\\.\pipe\DesertLinkCore-v1`

The plugin does not use external process-memory APIs. It validates the supported executable build before installing the teleport integration and fails closed when the target does not match.

## Desktop network access

The desktop companion reads Crimson Desert telemetry from the local telemetry service and loads the MapGenie web map/login service used by the interface.

The installer does not download executable payloads at runtime.
