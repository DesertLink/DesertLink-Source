# DesertLink v1.0.7 Security / Distribution Notes

## Public installer

The public installer is built with standard Inno Setup.

It:
- installs only the DesertLink desktop application;
- creates standard Desktop and Start Menu shortcuts;
- does not modify the game;
- does not install ASI files automatically;
- performs no Internet downloads on the user's computer;
- uses no PowerShell;
- does not open or write to the Crimson Desert process.

The official Electron runtime is fetched and SHA-256 verified only during the release build process and is then bundled into the installer.

## DesertLinkCore.asi

DesertLinkCore is loaded through the user's ASI loader and performs the game-side teleport integration in-process.
It communicates with the desktop application over a local Windows named pipe.

Imported Win32 APIs are limited to KERNEL32 functions required for threading, local named-pipe IPC, memory protection/querying and the in-process teleport hook.

It does not import WinHTTP, WinINet, Winsock, OpenProcess, ReadProcessMemory or WriteProcessMemory.
