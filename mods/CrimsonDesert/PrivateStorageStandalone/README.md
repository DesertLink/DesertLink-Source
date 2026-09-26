# DesertLink Private Storage — standalone test module

A minimal Crimson Desert 2.03.02 ASI whose only gameplay feature is opening
Private Storage from anywhere during normal free play.

## Why this exists

The supplied reference archive led to the public MIT-licensed source for
Private Storage Master. This DesertLink test module uses that documented source
mechanism as the reference implementation, strips unrelated capacity/stack/loot
features, and keeps only the native StageChart warehouse-open path needed by
DesertLink.

No third-party ASI binary or assets are included.

## Target

- Crimson Desert 2.03.02
- FileVersion 1.0.0.2976
- TimeDateStamp 0x6AB28F00
- SizeOfImage 0x173AB000

Unknown builds fail closed.

## Test control

Ctrl+F1 toggles Private Storage.

The eventual unified DesertLink menu should call the exported API instead of
using the temporary test key:

- DLPS_ApiVersion()
- DLPS_Ready()
- DLPS_IsOpen()
- DLPS_OpenPrivateStorage()
- DLPS_ClosePrivateStorage()
- DLPS_TogglePrivateStorage()

Requests are asynchronous. Native UI work runs from the validated game-thread
ModeSwitch path.

## Runtime ownership

Three narrow hooks only:

1. Warehouse2 command handler — identifies/acknowledges the native warehouse UI.
2. StageClose — lets native Esc/B close our stage cleanly.
3. ModeSwitch — existing game-thread execution point.

The module does not change inventory capacity, stack sizes, loot, save data,
game files, or networking.

## Conflicts

Do not load this together with PrivateStorageMaster.asi or
PrivateStorageAnywhere.asi. They own the same game UI paths.

## Evidence status

Reference source mechanism: reviewed.
Build target/signature strategy: static validated.
This DesertLink binary: owner runtime test required before TESTED.

## Attribution

Runtime scanner, RTTI resolver, narrow far-hook implementation, and the native
warehouse StageChart mechanism are derived from Private Storage Master by Seth,
MIT licensed. The upstream license is preserved in
UPSTREAM_PRIVATE_STORAGE_MASTER_LICENSE.txt.

HDE64 comes from MinHook under its BSD-style license. See
THIRD_PARTY_NOTICES.md.
