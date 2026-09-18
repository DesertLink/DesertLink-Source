DesertLink v1.2 - Crimson Desert Map Companion
=================================================

PUBLIC RELEASE CONTENTS
-----------------------
DesertLink_Setup_v1.2.exe
DesertLinkCore.asi
README.txt

REQUIREMENTS
------------
- Crimson Desert
- Working ASI loader
- CrimsonDesertTelemetry.asi
- DesertLinkCore.asi (included)

INSTALLATION
------------
1. Close Crimson Desert.
2. Install your ASI loader if it is not already installed.
3. Install CrimsonDesertTelemetry.asi.
4. Copy DesertLinkCore.asi to the same ASI folder as CrimsonDesertTelemetry.asi.
5. Run DesertLink_Setup_v1.2.exe.
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
Show/hide the whole Companion with its binding (default END).
Show/hide the internal panel with its binding (default Ctrl + Shift + M).

PANEL SIZE (resizable)
----------------------
Drag the small grip in the bottom-right corner of the menu to make it as small
or as large as you need. The size is remembered on the next launch. The menu
also auto-fits when the window is small, and its content scrolls when needed.

SAVED WAYPOINTS (name + rename)
-------------------------------
- Click "+ Save current position". You are asked for a name, pre-filled with
  "Waypoint N" - type any name you want and press Enter (or Save).
- Rename a saved waypoint at any time: click the pencil (rename) button next to
  it, or double-click its name. Enter = save, Esc = cancel.
- The name is stored with the waypoint and shown both in the list and on the map.
- Teleport (crosshair) and Delete (x) work exactly as before.

KEYBOARD + MOUSE BINDINGS (remappable)
--------------------------------------
Open the Companion panel, click "Show Bindings", then click a binding and press
a key or a mouse button. ESC cancels; Backspace/Delete clears the binding.
The line under the map buttons always shows the CURRENT bindings.

Action                      Default
--------------------------  -----------------
Teleport to Map Click       Right Mouse
Teleport Map Center         F5
Return / Abort Previous     Shift+F5
Show / Hide Companion       End
Show / Hide Panel           Ctrl+Shift+M
Toggle Follow               Unbound
Reload Map                  Unbound
Use Current Y               Unbound
Save Current Position       Unbound
Toggle App / Overlay Mode   Unbound
Game Focus Mode             Ctrl+Shift+G

Notes
- Keyboard bindings are global: they work while the game has focus.
- Mouse bindings are used on the map. Supported buttons: Left, Middle, Right,
  Mouse 4 (back), Mouse 5 (forward).
- One action per key / mouse button. Choosing an input that is already used
  shows a clear error instead of silently overwriting.
- Any action can be bound to a key or a mouse button. If "Teleport to Map Click"
  is bound to a key, that key teleports to the cursor position on the map
  (move the cursor over the map first).
- The map's right-click menu is only suppressed while Right Mouse is bound to an
  action, so remapping teleport off Right Mouse leaves the menu alone.
- Bindings are saved and restored on restart.

TELEPORT
--------
Default: right-click the map to teleport. This is remappable (see above).
Default: F5 teleports to the map center.
Default: Shift + F5 returns to the previous teleport position.

SECURITY / PACKAGING
--------------------
The public installer is built with standard Inno Setup.
It performs no Internet downloads and uses no PowerShell.
The official Electron runtime is downloaded and SHA-256 verified at BUILD TIME, then bundled into the installer.

DesertLinkCore.asi is a game-side plugin and uses an in-process runtime hook for teleport functionality. Its complete source is published in the DesertLink source repository for review and transparency.

The installer and companion app are version 1.2. DesertLinkCore.asi is the game-side teleport/telemetry plugin for Crimson Desert 2.02.x (it replaces the old stale v1.0.7 core that could not find the physics hook on this game build).

COMPATIBILITY
-------------
Built for Crimson Desert 2.02.00 (CrimsonDesert.exe 1.0.0.2850).
Future game updates may require a DesertLinkCore compatibility update.
