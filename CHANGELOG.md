# Changelog

## v2.0-E1

Experimental 2.0 candidate.

- Organize platform responsibilities, generated artwork, documentation and asset-free host checks.
- Retain UI image/palette source bytes through decoding with scoped resource leases.
- Preserve recoverable settings when FAT replacement or restoration fails.
- Add Original, bilinearly Scaled and hand-authored Modified UI texture modes.
- Redraw all twenty course/battle maps at three native sizes with continuous smooth curves.
- Recompose the port credit from native lettering with a complete y descender and shadow.
- Author native-size HUD labels, lap counters and timer cells; retain original art where replacements are unfinished.
- Add original square per-option pixel icons with balanced padding and native bevel/blinking selection; preserve native hint lettering.
- Rebuild compact Display/Advanced, Gameplay and Pause menus with shared touch geometry and clear selection.
- Generate native-size menu hints in Python with integrated animated backings; remove the redundant All Random caption.
- Keep the original-resolution background for Light blur and retain bounded cached stronger treatments.
- Allow diagnostic SELECT and its expanded chord during AUTO without unlocking driving inputs.
- Document runtime ownership, configuration, development and validation routes.
- Add Solo VS/Battle with CPUs, AUTO spectator play and four-course random cups.
- Add Modern Logic item/CPU rules and held-B reverse, while retaining original reverse when disabled.
- Add held-X rear view, SELECT camera distance, full Y HUD cycling and held C-Stick Turbo with a 2x–5x slider. ZL/ZR are unassigned.
- Add exclusive SELECT dump/overlay bindings and an always-available L+R+SELECT expanded dump. The live lower-screen overlay preserves racing inputs.
- Add Performance/Quality/Custom profiles, a dedicated Advanced display menu and explicit 30/60 FPS presentation targets.
- Add scrolling course maps with cached light blur, a centered cursor and consistent pause/menu typography without row separators.
- Preserve EstebanPdN's five HUD layouts, title/HOME branding and Stable/Experimental updater. Add the title credit "Ported by EstebanPdN".
- Integrate resource ownership, memory/cache recovery, GPU fence/combiner/depth/alpha, Lakitu/jumbotron, race/result/fade and positional audio fixes.
- Correct AUTO/CPU inventory, targeting, collisions, Battle navigation/standings, spectator feedback and race time accounting across pauses and Turbo.
- Preserve E14-local used-texture material reuse, verified audio CPU-reservation restoration and startup trace suspension behavior.

Adapted features and corrections credit Epic0522's MarioKart64-3DSPort-Complete.
The original 30 Hz simulation is retained. Native builds and host regressions
are verified separately from physical-console stability, visual alignment and
matched performance measurements, which remain pending.

## v1.6-E7

Changes since v1.5.

- Default to 1.0 render scale on both Old and New 3DS.
- Clean the actual HUD buffer allocations instead of the gaps between them.
- Move texture scaling and tile clamping to the GPU vertex shader.
- Service HOME before starting game logic and audio rendering work.
- Stop the audio producer and release its shared-core reservation during suspension.
- Restart audio and reset presentation timing after returning from HOME or sleep.
- Suspend diagnostic input polling while the application is inactive.
- Bound shutdown waits and prevent live worker stacks from being freed on timeout.
- Record startup stages, shared-core reservations, vertex packing and HUD cache costs.
- Fix Old 3DS startup aborts caused by mandatory archive RAM residency.
- Keep optional interpolation storage out of the ordinary archive heap.
- Load compressed resources in small blocks to tolerate heap fragmentation.
- Distinguish memory limits from archive read errors in startup diagnostics.
- Show a small animated Lakitu flag on black screens while the game starts.
- Cache the 32 loading frames locally and release their memory before gameplay.
- Show instantaneous FPS from consecutive presented images in the corner counter.
- Add a touch-adjustable volume slider matching the render slider.
- Allow adaptive interpolation on Old and New 3DS in both supported resolutions.
- Skip interpolation recording when CPU or GPU work exceeds its budget.
- Batch O2R startup reads while retaining compact RAM residency and CRC checks.
- Avoid repeated black-screen redraws during course loading.
- Combine initial EEPROM repairs into one atomic save and skip unchanged writes.
- Refill audio throughout long rendering work, with a barrier before game logic resumes.
- Restore early CPU face rejection before texture processing and vertex submission.
- Avoid renderer state changes for rejected triangles and cache copies for small vertex loads.
- Add eligible GPU vertex transforms and directional lighting with CPU compatibility paths.
- Add bounded matrix and vertex-depth reuse, faster command dispatch, and diagnostic counters.
- Fix the red lower screen during the Nintendo logo and synchronize the white title transition.
- Remove menu labels and race HUD text behind the Update changelog.
- Fix updater certificates, offline notes, flicker, backgrounds, and page navigation.
- Add Update below Close Options with the native Mario Kart menu font.
- Add Stable and Experimental channels with verified CIA/3DSX updates and installation confirmation.

The game simulation remains at 30 Hz; adaptive presentation can reach the 60 Hz display limit when workload permits. E7 performance and compatibility still require physical-console validation.

## v1.5

- Fixed startup, shutdown, and gameplay crashes
- Fixed the graphics command-buffer crash on race results
- Improved Old and New 3DS performance and memory use
- Fixed frame interpolation and frame pacing
- Preloaded game resources into RAM to remove in-race resource reads from SD
- Improved rendering, textures, colors, and audio processing
- Faster on-device game-data extraction
- Always show ROM extraction progress independently of game loading screens
- Added adjustable render scale, render distance, and display filters
- Added five HUD layouts and improved race HUD presentation
- Redesigned Data and Course Data across both screens
- Centered Grand Prix and Time Trial results with text shadows
- Fixed record display and ghost save/load persistence
- Added Race Ghost and Erase Ghost actions in Course Data
- Expanded diagnostic dumps with numbering, RAM capture, and Clean dumps
- Updated the HOME Menu banner and sound
