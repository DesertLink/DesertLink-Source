# Security Notes

## DesertLinkCore.asi

DesertLinkCore is an in-process Crimson Desert ASI plugin.

It uses runtime memory protection/allocation APIs because the teleport feature requires an in-process hook on supported game builds.

Relevant Win32 APIs include:

- CreateThread
- VirtualAlloc
- VirtualFree
- VirtualProtect
- VirtualQuery
- FlushInstructionCache
- CreateNamedPipeA
- ConnectNamedPipe
- ReadFile
- WriteFile

These APIs may resemble trainer/modding behavior to heuristic antivirus engines.

DesertLinkCore does **not** use:
- WinHTTP
- WinINet
- Winsock
- remote sockets
- OpenProcess
- ReadProcessMemory
- WriteProcessMemory
- external process injection

Communication with the DesertLink app is through the local named pipe:

`\\.\pipe\DesertLinkCore-v1`

## DesertLink App Setup

The public app-only installer:
- does not modify the game;
- does not copy/install ASI files;
- does not download anything;
- does not use PowerShell;
- creates local application files and Windows shortcuts;
- verifies the manually downloaded Electron runtime by SHA-256.

## Desktop app

The desktop app requires normal Internet access to load the external web map/login service shown in the DesertLink UI. This network access is part of the map companion functionality, not an updater or arbitrary file downloader.
