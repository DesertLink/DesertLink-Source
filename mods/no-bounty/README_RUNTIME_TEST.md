# NeverWanted - DesertLink v1.1 TEST

Target: Crimson Desert 2.01.00 / CrimsonDesert.exe 1.0.0.2760 only.

Status: TEST BUILD. This is not marked RELEASE until the in-game pass/fail test succeeds.

## What changed

This build does NOT hook wanted request serializers and does NOT hook NPC/AI ticks.
It patches the game's already-loaded data-table rows in memory, session only:

- WantedInfo runtime rows: `_isBlocked = 1`
- WantedInfo runtime rows: `_increasePrice = 0`
- TribeInfo runtime rows: `_wantedCrimeType = 0`

The build is guarded by the exact 2760 image size plus the exact current resolver prologue shape. If the build does not match it refuses to patch.

## Install

1. Remove all older NeverWanted / NeverWantedDiag ASI files from `Crimson Desert\bin64`.
2. Keep your normal ASI loader installed.
3. Copy `NeverWanted_DesertLink_v1.1_TEST.asi` into `Crimson Desert\bin64`.
4. Start the game and enter the world.
5. Wait 2-3 seconds.
6. Open `NeverWanted_DesertLink_v1.1_TEST.log` next to the ASI.

For a valid test the log must contain `STATUS ACTIVE` and non-zero `tribeSeen` / `wantedSeen` counts.

## Pass criterion

From a clean/no-wanted state, attack a civilian or allied guard in view of witnesses.
PASS means: no red wanted/search state is created, no bounty is added, and guards do not enter the wanted/pursuit/arrest flow because of the crime.

If it fails, send the generated log and say exactly what still happened (red zone / bounty / witness pursuit / arrest).
