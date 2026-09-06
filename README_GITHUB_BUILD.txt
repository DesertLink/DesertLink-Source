DesertLink v1.0.7 - Clean Public Build Kit
==========================================

WHY THIS BUILD EXISTS
---------------------
The old custom self-extracting installer has been removed.
The public installer is now designed to be built with standard Inno Setup.
There is no end-user downloader, no PowerShell installer, and no appended custom ZIP payload.

HOW TO BUILD THE PUBLIC INSTALLER ON GITHUB
-------------------------------------------
1. Extract this ZIP.
2. Upload all files/folders into your DesertLink GitHub repository root.
   IMPORTANT: include the hidden .github folder.
3. Open the repository's Actions tab.
4. Select: Build DesertLink Public Release
5. Click: Run workflow
6. Wait for the Windows build to finish.
7. Open the completed workflow run and download the artifact:
   DesertLink-v1.0.7-public

The artifact will contain:
- DesertLink_Setup_v1.0.7.exe
- DesertLinkCore_v1.0.7.asi
- DesertLink_v1.0.7_NEXUS.zip
- README.txt

The NEXUS ZIP is the file intended for Nexus upload.

WHAT HAPPENS DURING THE BUILD
-----------------------------
GitHub's Windows runner downloads the official Electron v44.2.0 Windows x64 runtime,
verifies its SHA-256, bundles the DesertLink app into it, downloads the official
Inno Setup compiler, and creates a standard Windows installer.

The final end-user installer itself performs no Internet downloads.
