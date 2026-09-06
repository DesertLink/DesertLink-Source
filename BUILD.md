# DesertLink v1.0.7 Public Build

The preferred public build uses the GitHub Actions workflow:

`.github/workflows/build-public.yml`

The workflow:
1. Downloads the official Electron v44.2.0 Windows x64 runtime.
2. Verifies SHA-256:
   `4021363e3090d67a144ebedb90765cf193b0e61f300c519c83f0174502a481da`
3. Places the DesertLink application in `resources/app`.
4. Downloads the official Inno Setup 7.1.0 compiler on the Windows build runner.
5. Builds a standard single-EXE installer.
6. Packages the installer, DesertLinkCore.asi and README into the Nexus ZIP.

No runtime downloader is shipped to end users.
No custom self-extracting executable format is used.
