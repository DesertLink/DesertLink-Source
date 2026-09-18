# DesertLink v1.2 Public Build

The public build is defined by:

`.github/workflows/build-public.yml`

The workflow runs on a Windows GitHub runner and:

1. downloads the official Electron v44.2.0 Windows x64 runtime;
2. verifies its SHA-256 before use;
3. places the files from `app/` into `resources/app`;
4. builds `core/DesertLinkCore.cpp` as an x64 Windows ASI plugin with MSVC;
5. installs the official Inno Setup compiler on the build runner;
6. builds `DesertLink_Setup_v1.2.exe`;
7. packages the installer, `DesertLinkCore.asi`, and public README into the Nexus ZIP.

The end-user installer performs no runtime downloads and uses no PowerShell.

## Local source checks

JavaScript syntax:

```text
node --check app/main.js
node --check app/preload.js
node --check app/inject.js
```

The game-side source is x64 Windows code and is compiled by the public workflow with MSVC.
