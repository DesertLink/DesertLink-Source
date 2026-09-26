# DesertLink Private Storage — Original Runtime

Standalone test build for Crimson Desert 2.03.02 / FileVersion 1.0.0.2976.

This branch is an independent DesertLink implementation of the game's native
StageChart warehouse protocol and runtime behavior. It does not include or
redistribute Private Storage Master source, binaries, assets, or third-party
hook libraries.

## Test

1. Put DesertLinkPrivateStorage.asi in Crimson Desert/bin64.
2. Do not load another private-storage ASI at the same time.
3. Enter normal gameplay and wait about 10 seconds.
4. Press Ctrl+F1 to open/toggle Private Storage.
5. Esc closes it.

Log:
DesertLinkPrivateStorage.log

## Integration API

- DLPS_ApiVersion
- DLPS_Ready
- DLPS_IsOpen
- DLPS_OpenPrivateStorage
- DLPS_ClosePrivateStorage
- DLPS_TogglePrivateStorage

The API is asynchronous. Requests are executed on the game's validated
ModeSwitch main-thread path.

## Compatibility

Exact target only:
- TimeDateStamp 0x6AB28F00
- SizeOfImage 0x173AB000

Unknown builds fail closed.

## Evidence status

Buildable and statically validated. Gameplay behavior remains OWNER TEST READY
until confirmed in-game.
