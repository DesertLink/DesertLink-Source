# DesertLink Project State

Last updated: 2026-09-07

This file is the canonical technical handoff for future DesertLink chats/sessions. Do not restart Crimson Desert reverse engineering from scratch without reading this first.

## Crimson Desert exact baseline

- Game: Crimson Desert
- Version: 2.01.00
- Steam build: 25116796
- CrimsonDesert.exe file version: 1.0.0.2760
- PE SizeOfImage observed live: `0x16F1F000`
- Known SHA256 from prior exact-build verification: `4D99C15C58BD20A94D354D10AE395D1FAC777D59EF52CBA8080DC3FC8DC6F454`
- Main game path used during development: `G:\SteamLibrary\steamapps\common\Crimson Desert\bin64\CrimsonDesert.exe`

## Telemetry / Companion known-good baseline

- Telemetry endpoint: `http://127.0.0.1:27311/v1/snapshot`
- ASP.NET Core Runtime 8 x64 was required for the working telemetry server on this build.
- Existing browser/Companion bridge polls the endpoint and can bridge to `ws://127.0.0.1:7891`.
- Preserve existing MapGenie calibration/progress; prefer read-only telemetry and avoid unnecessary game-folder hooks.

## NeverWanted goal

Exact success criterion is stronger than merely making bounty cost zero:

- committing a crime should not create the red wanted/search state at all;
- no bounty should be added;
- no witness escalation / wanted pursuit / arrest flow should be triggered by the crime.

## NeverWanted branches already disproved

Do NOT repeat these approaches without new evidence:

1. Request serializer blockers (`WantedAddWitnessActorReq`, `ChangeWantedStateReq`, etc.).
   - They are downstream of local crime/search state creation.
   - They can leave half-created state and do not reliably prevent the red zone.
2. Blocking only `WantedAddCrimeRecordReq`.
   - Tested; red zone/pursuit still occurred.
3. `WantedInfo._increasePrice = 0` alone.
   - Can zero bounty price but does not by itself prevent the red wanted/search state.
4. NeverWanted v1.0 DATA GATE.
   - FAILED / DEPRECATED.
   - It reused an outdated Trinity resolver-prologue assumption.
   - Live log showed `TribeInfo resolver RVA = 0` and `STATUS REFUSED`, so it performed no patch at all.

## Current 2760 live table mapping

Derived from the user's `NeverWantedDiag_v0.8_FASTXREF.log` plus `NeverWantedProbe_v0.9_STATICDUMP.log` on the exact running 2.01.00 / 2760 process.

### TribeInfo resolver

- Resolver function RVA: `0x00382060`
- Resolver key load: `movzx edi, word ptr [rcx]` (u16 key)
- Manager global RVA used by resolver: `0x06C2A088`
- Manager layout observed in machine code:
  - `manager + 0x08` = count
  - `manager + 0x58` = qword row-pointer array
- Table-name anchor: `tribeinfo`
- Helper/current row constructor RVA: `0x00385CA0`
- Current row allocation size seen in helper: `0x80` bytes
- Runtime `TribeInfo._wantedCrimeType` field: `row + 0x1C`
  - Historical reverse-engineering maps canonical `_wantedCrimeType` to in-memory `unk_28` (decimal 28 = 0x1C).
  - The current 2760 constructor preserves the same early field block: `_isBlocked` at +0x10, tribe-mass-level area at +0x16, following scalar block through +0x1C.

### WantedInfo resolver

- Resolver function RVA: `0x017C7940`
- Manager global RVA used by resolver: `0x06C4AC10`
- Same manager layout: count +0x08, row pointer array +0x58
- Table-name anchor: `WantedInfo`
- Helper/current row constructor RVA: `0x017C8920`
- Current row allocation size: `0x28` bytes
- Current runtime WantedInfo row fields confirmed directly from constructor writes:
  - `_isBlocked` = `row + 0x10`
  - `_increasePrice` = `row + 0x18` (u64)
  - `_useTargetPrice` = `row + 0x20`

## External evidence for the chosen architecture

Current Crimson Desert 2.01.00 Trinity maintenance changelog states:

- broad crime/steal AI hooks were removed after causing NPC-interaction crashes;
- No Bounty works by patching `WantedInfo` and `TribeInfo` data tables;
- current 0.19.0 supports EXE 1.0.0.2760;
- No Bounty was updated to cover theft as well as assault.

Another current trainer describes its session-safe No Bounty as dual-table patching that zeroes crime bounties and clears wanted crime types across roughly 370 factions, preventing guard aggro from the crime path.

## Current active build: NeverWanted v1.1 TEST

Status: **COMPILED / READY FOR IN-GAME TEST — NOT YET RELEASE**

Source:
- `mods/no-bounty/NeverWantedRuntime.cpp`

Build workflow:
- `.github/workflows/build-no-bounty.yml`

Artifact name:
- `NeverWanted-DesertLink-v1.1-TEST`

GitHub Actions successful build run:
- run id `34160864492`
- artifact id `10032544188`
- artifact digest `sha256:665bfdd11bd5c354e9ef8d0577f4755f5b18bd9d92131d81bf1015d2e73e842d`

### v1.1 runtime behavior

No hooks are installed. A small worker periodically patches only already-loaded table rows:

- `WantedInfo._isBlocked = 1`
- `WantedInfo._increasePrice = 0`
- `TribeInfo._wantedCrimeType = 0`

The build is guarded by:
- exact PE `SizeOfImage = 0x16F1F000`;
- exact 2760 resolver prologue/key-load bytes at both current resolver RVAs.

It does not call lazy table resolvers from its worker thread. Instead it patches rows that are already resident, then repeats every 100 ms so newly loaded rows are picked up without invoking engine loader functions from the wrong thread.

### v1.1 test procedure

1. Remove every older `NeverWanted*.asi` / diagnostic ASI from `bin64`.
2. Put only `NeverWanted_DesertLink_v1.1_TEST.asi` in `bin64` (normal ASI loader remains installed).
3. Start game, enter world, wait 2-3 seconds.
4. Check `NeverWanted_DesertLink_v1.1_TEST.log`.
5. It must show `STATUS ACTIVE` with non-zero `tribeSeen` and `wantedSeen`.
6. From a clean wanted state, attack a civilian/allied guard in sight of witnesses.
7. PASS = no red wanted/search state, no bounty addition, no wanted pursuit/arrest escalation caused by the crime.
8. If FAIL, return the log plus exactly which behavior remained: red zone, bounty, witness pursuit, arrest.

## Build-status discipline

Use these labels literally:

- `UNTESTED`
- `COMPILED / READY FOR TEST`
- `WORKING`
- `FAILED`
- `DEPRECATED`

Never call an untested build `RELEASE` again.

## Distribution / architecture rules

- Prefer standard loader/tooling and minimal permissions.
- Avoid packers/self-extractors/weird custom loaders.
- Avoid broad AI hooks and unnecessary injection patterns.
- Exact-build checks and fail-closed behavior are mandatory for memory modifications.
- Prefer source transparency and code signing for public releases.
- Anticipate Nexus/AV false positives and moderation/review constraints.
- For future frequent mod development, a single stable DesertLink DevLoader/hot-reload layer is preferable to repeatedly replacing many independent ASIs.
