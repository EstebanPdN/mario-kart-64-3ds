# Compatibility and preservation — 2.0

The experimental line changes menus and adds gameplay options while retaining
the existing title/data identity and original game integration. A passing host
fixture or emulator launch is not proof of physical Old/New 3DS compatibility.

| Contract | Current implementation and validation boundary |
| --- | --- |
| Installed identity | CIA title ID `0004000005A27000`, product code `CTR-P-MK63`. Updating this title retains its existing SD data. A second build with a different ID is a separate installed title. |
| Game input | An owner-provided compatible USA ROM or generated `sd:/3ds/MK64/mk64.o2r` remains required. No game-data conversion occurs merely from switching UI textures. |
| Preferences | `sd:/3ds/MK64/mk64-3ds.cfg`; older files can omit new keys. Parsing applies model defaults first, then accepted saved values. See [Configuration](CONFIGURATION.md). |
| EEPROM/ghost records | Native game storage remains separate from preferences, diagnostics and optional caches. A structural change must not reset it or serialize a C++ struct over the existing format. |
| HUD layouts | Clean, MK7, MKDS, MKDS 2 and Classic retain their identities. Y cycles complete layouts during manual races. Texture style is independent of layout/profile choice. |
| Simulation | The original 30 Hz simulation remains. Adaptive presentation can add an image between simulation frames; a 60 FPS target is not 60 Hz game logic. |
| Controls | A/B/R/L retain their gameplay roles, X holds rear view, SELECT defaults to camera distance, C-Stick holds configurable Turbo, and ZL/ZR are unassigned. Contextual AUTO/random-cup actions remain in their menus. |
| Diagnostics | Expanded L+R+SELECT has priority over SELECT's configured action. Captures pause game/audio work and remain private runtime data. They are not ordinary frame-time samples. |
| Updates | Stable/Experimental channels remain available. An experimental release must not silently become the Stable/latest release. Preserve the executable filename expected by the updater. |
| Launch formats | Release the complete-game 3DSX or CIA, not a probe/bootstrap executable. Their launch context and application memory allowance still need separate device checks. |

## Settings replacement and recovery

The writer finishes and closes `mk64-3ds.cfg.tmp` before replacing the active
file. Where rename-overwrite is supported, the normal replacement is direct.
Where FAT refuses it, the previous file is renamed to `mk64-3ds.cfg.bak` until
the prepared replacement succeeds. A failed second rename restores the old
file; if restoration also fails, the complete backup remains available. If
the main file is absent, loading falls back to that backup. An existing backup
is never overwritten by the fallback replacement path.

This is a recoverable replacement policy, not a guarantee of power-loss
atomicity on every SD/filesystem. If both main and backup exist and saving fails,
close the game and preserve both before resolving the SD error. Do not delete a
recovery file until a valid current configuration has been confirmed. Saving
preferences does not replace the owner's ROM, O2R, EEPROM or ghosts.

Host fault fixtures cover successful POSIX/FAT replacements, failed replacement,
failed restoration, existing backups and restart from a backup-only file. Real
SD faults and power interruption remain physical acceptance cases.

## Verification boundaries and remaining work

The asset-free runner covers model-dependent defaults, Original/Scaled restart,
profile isolation, UI routes, resource leases, memory/math policies and generated
dispatch. The cross-build verifies ARM sources, shader/link inputs and packages.
Azahar validates only the recorded executable and scenario. Physical Old/New
boot, full races, audio, HOME/sleep, long-session memory and sustained 30/60 FPS
need measurements on the named model and exact build.

Modified artwork currently includes hand-authored HUD labels/counters and all
twenty course/battle maps at three sizes. Portraits/items/ranks and the general
menu alphabet are not a complete hand-authored replacement set; they retain the
original in Modified. Scaled provides their bilinear path. See
[Native sprites](NATIVE_SPRITES.md) for the exact distinction.

Use the [validation route](VALIDATION.md) for acceptance and
[troubleshooting](TROUBLESHOOTING.md) for actionable reports. Keep verified
changes, experimental limits and pending device checks separate in a release.
