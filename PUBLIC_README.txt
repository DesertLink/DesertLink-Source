DesertLink v1.0.7 - Crimson Desert Map Companion
=================================================

PUBLIC RELEASE CONTENTS
-----------------------
DesertLink_Setup_v1.0.7.exe
DesertLinkCore_v1.0.7.asi
README.txt

REQUIREMENTS
------------
- Crimson Desert
- Working ASI loader
- CrimsonDesertTelemetry.asi
- DesertLinkCore_v1.0.7.asi (included)

INSTALLATION
------------
1. Close Crimson Desert.
2. Install your ASI loader if it is not already installed.
3. Install CrimsonDesertTelemetry.asi.
4. Copy DesertLinkCore_v1.0.7.asi to the same ASI folder as CrimsonDesertTelemetry.asi.
5. Run DesertLink_Setup_v1.0.7.exe.
6. The installer creates Desktop and Start Menu shortcuts.
7. Start Crimson Desert and load your character into the world.
8. Start DesertLink.
9. Move your character for a few seconds and wait for:
   Telemetry OK
   Teleport Ready

OVERLAY
-------
App Mode is recommended for a second monitor.
Overlay Mode keeps DesertLink above the game.
Switching App/Overlay mode intentionally closes DesertLink; launch it again to apply the selected mode.
Default overlay show/hide key: END
Default internal panel show/hide key: Ctrl + Shift + M

TELEPORT
--------
Right-click the map to teleport.
F5: Teleport to map center
Shift + F5: Return to previous teleport position

SECURITY / PACKAGING
--------------------
The public installer is built with standard Inno Setup.
It performs no Internet downloads and uses no PowerShell.
The official Electron runtime is downloaded and SHA-256 verified at BUILD TIME, then bundled into the installer.

DesertLinkCore.asi is a game-side plugin and uses an in-process runtime hook for teleport functionality. Its complete source is published in the DesertLink source repository for review and transparency.

COMPATIBILITY
-------------
Built for the supported Crimson Desert 2.01.x game build used during development.
Future game updates may require a DesertLinkCore compatibility update.
