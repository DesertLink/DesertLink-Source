# Build Instructions

These instructions describe how to rebuild the two compiled components from source.

## Prerequisites

Use a Windows x64 machine with:

- Visual Studio 2022 Build Tools / Windows SDK
- LLVM/Clang 17 (clang-cl and lld-link)
- Python 3.x for the final installer payload append step

Run the commands from an **x64 Native Tools / Developer Command Prompt** so the Windows SDK import libraries are available.

---

## 1. Build DesertLinkCore.asi

Create a build folder:

```bat
mkdir build
```

Compile:

```bat
clang-cl /c /O2 /GS- /GR- /EHs-c- /Fo:build\DesertLinkCore.obj core\DesertLinkCore.cpp
```

Link as a DLL/ASI without the CRT startup:

```bat
lld-link /dll /entry:DllMain /nodefaultlib /out:build\DesertLinkCore.asi build\DesertLinkCore.obj kernel32.lib
```

The source explicitly imports the Win32 APIs it uses. No third-party binary library is linked into DesertLinkCore.

---

## 2. Build the DesertLink desktop installer

Compile the small stack-check helper:

```bat
clang --target=x86_64-pc-windows-msvc -c installer\chkstk.s -o build\chkstk.obj
```

Compile the installer:

```bat
clang-cl /c /O2 /GS- /GR- /EHs-c- /Fo:build\DesertLinkSetup.obj installer\DesertLinkSetup.cpp
```

Link the base GUI executable:

```bat
lld-link /entry:entry /subsystem:windows /nodefaultlib ^
  /out:build\DesertLink_Setup_base.exe ^
  build\DesertLinkSetup.obj build\chkstk.obj ^
  kernel32.lib user32.lib comdlg32.lib
```

The installer dynamically resolves the shell/COM APIs used for shortcut creation.

---

## 3. Create the embedded app payload

The payload layout used by Setup is:

```text
resources/
  app/
    inject.js
    main.js
    package.json
    preload.js
    THIRD_PARTY_NOTICES.txt
README.txt
```

Create a normal ZIP named `payload.zip` containing that layout.

Do not put another ZIP/archive inside the payload.

---

## 4. Append the payload to the base installer

The installer format is:

```text
[base Windows EXE]
[payload ZIP bytes]
[8-byte little-endian payload size]
```

Example Python:

```python
from pathlib import Path
import struct

base = Path("build/DesertLink_Setup_base.exe").read_bytes()
payload = Path("build/payload.zip").read_bytes()

Path("build/DesertLink_Setup.exe").write_bytes(
    base + payload + struct.pack("<Q", len(payload))
)
```

The final Setup reads its own last 8 bytes, extracts the embedded payload to the user's local DesertLink install folder, and creates shortcuts.

---

## Runtime verification

For first-time installation, Setup asks the user to manually select:

`electron-v44.2.0-win32-x64.zip`

It accepts the file only if its SHA-256 is:

`4021363e3090d67a144ebedb90765cf193b0e61f300c519c83f0174502a481da`

Setup itself performs no runtime download.

## Notes on byte-for-byte hashes

Compiler/linker versions, timestamps, and ZIP metadata can change the final binary hash even when rebuilt from identical source. `SHA256SUMS.txt` records the hashes of the binaries uploaded for DesertLink v1.0.6.
