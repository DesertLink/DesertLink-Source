# No Bounty - DesertLink

Version 0.1 TEST

## What it does

Sets `wantedinfo.pabgb -> increase_price` to `0` for every wanted/bounty tier.

This is a pure data mod:

- No ASI binary
- No executable code
- No runtime hooks
- No PowerShell
- No external process memory writing

## Requirements

A current Crimson Desert mod manager that supports Format 3 `.field.json` mods, such as DMM / a compatible current manager.

## Install

1. Import `NoBounty_DesertLink.field.json`.
2. Enable/apply/mount the mod.
3. Start Crimson Desert.
4. Test both theft and assault.

## Test checklist

Please verify both crime paths:

1. Theft
2. Assault / attacking a civilian or allied guard

If either path still increases bounty, note which one. Crimson Desert has more than one crime-price path, so a follow-up build may also need the tribe/faction crime table.

## Compatibility

Developed for Crimson Desert 2.01.x. Because this uses a named field rather than a fixed byte offset, it should be more resilient to updates as long as the table/schema remains compatible.

## Uninstall

Disable/remove the mod in the mod manager and re-apply the remaining mods.
