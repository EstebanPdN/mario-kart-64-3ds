# Changelog

## v2.1-E2

- Fixed a console freeze at the start of courses with fog, including Choco Mountain.

## v2.1-E1

- Added VRAM capture to manual dumps.
- Captured VRAM before diagnostic progress rendering.
- Disabled scheduled and manual dumps during AUTO.

## v2.0

Based on the 2.0-E6 candidate. Cumulative changes since v1.5.

### Artwork and presentation

- Add Original, Menu and Improved texture modes, with the approved menu artwork and Improved kart texture companion included in both the CIA and full-game 3DSX.
- Redraw all twenty course and battle maps at three native sizes, with smoother continuous outlines.
- Add native-size HUD labels, lap counters and timer cells, improved portraits, and animated menu hints with integrated backings.
- Add per-option pixel icons, native-style bevels and blinking selection highlights.
- Rebuild the Display, Advanced, Gameplay and Pause menus with shared touch geometry and consistent typography.
- Add Performance, Quality and Custom display profiles, a touch-adjustable volume slider, and clearer render and presentation controls.
- Add HUD distance, speed, position and lap displays, scrolling course maps, a centered map cursor and an animated item-box selection cursor.
- Preserve the original-resolution background for Light blur and cache stronger blur treatments within a bounded budget.
- Improve standings, finish transitions, spectator feedback and the layout of results across both screens.
- Match the Nintendo copyright and port credit font, colors, outline and shadow. Keep Nintendo on the first line and the smaller port credit below it.
- Restore the Nintendo title copyright in Menu and Improved modes, and correct the P/s/a credit lettering on a native pixel grid.
- Retain the original HOME Menu banner, icon, sound and all five HUD layouts.

### Modes, controls and optional gameplay changes

- Add Solo VS with one player and seven CPU opponents.
- Add Solo Battle with one player and three CPU opponents, navigation for all four original arenas, and spectator play after elimination.
- Add random cups containing four different courses, with driver and difficulty shortcuts.
- Add AUTO driving and spectator play, camera choices and automatic confirmation of results and ceremonies.
- Add optional Modern Logic for CPU item-box collection, inventory, rank-based roulette, Star and mushroom use, terrain handling, collisions, targeting, stuck recovery, road-following shells and protection after a hit.
- Add held-B reverse with Modern Logic, held-X rear view and configurable SELECT camera distance.
- Add held C-Stick Turbo with an adjustable 2x–5x multiplier; preserve Y cycling through all five HUD layouts.
- Add a threat warning widget and improve CPU behavior, Battle standings and race results.
- Correct race-time accounting across pauses and Turbo, CPU finishing behavior and manual result controls when AUTO is disabled.

### Rendering and resource use

- Fix distance fog hiding track textures on Old 3DS with Low and Normal draw distance. Preserve the distance-off path and the E6 correction to fog-ramp orientation.
- Cache hot decoded textures and resource wrappers to avoid repeated reads, decompression and lookup work.
- Route ordinary texture-image commands through the texture lookup cache and retain texture and palette owners for the lifetime of submitted work.
- Reuse eligible static geometry, matrix and vertex state, including ordinary single-effective-texture fog materials in Choco Mountain and Toad's Turnpike.
- Separate effective sampler use from legacy vertex-layout flags, keep complex dual-texture fog on its guarded compatibility path, and avoid redundant fog-state writes when only unused alpha changes.
- Improve repeated-corner vertex-cache hashing, bypass the cache for very small draws and sample packing timers instead of reading the clock on every draw.
- Skip eligible kart visual preparation when a presentation frame is omitted, while retaining game simulation updates.
- Restore early face rejection, avoid state changes for rejected triangles, and add bounded matrix and vertex-depth reuse.
- Move eligible texture scaling, tile clamping, vertex transforms and directional lighting to the GPU, with compatibility paths for unsupported cases.
- Correct UV, combiner, alpha, depth, Lakitu, jumbotron and third/fourth viewport kart texture-buffer handling.
- Fence GPU work before buffer reuse, resource release and texture-mode changes; clear saved renderer state at scene and mode boundaries.
- Add bounded Improved artwork mirrors, decoded pages and inactive-mode metadata instead of repeatedly loading and decoding the same artwork.
- Release replaceable stream-cache memory before archive residency admission, and keep optional artwork allocation failures from evicting a resident game archive.
- Keep archive streaming available when Old 3DS cannot admit full RAM residency, use smaller allocation blocks and distinguish memory limits from read errors.
- Batch startup archive reads with CRC validation and retain a reserve for game allocations.

### Audio, startup and reliability

- Restore the Old 3DS shared-core audio reservation and retain a synchronized fallback when the worker is unavailable.
- Wake audio from NDSP progress, validate DMA ranges and cache interval lookups.
- Refill audio during long render work and synchronize before game logic resumes.
- Service HOME before starting new logic and audio work, suspend the producer and shared-core reservation while inactive, and reset presentation timing on resume.
- Bound shutdown waits and preserve live worker stacks until they can be released safely.
- Remove repeated per-frame settings allocation and reduce timer overhead.
- Preserve UI image and palette source bytes through decoding with scoped resource leases.
- Preserve recoverable settings if replacement or restoration fails.
- Combine initial EEPROM repairs into one atomic save and skip unchanged writes.
- Add an animated loading flag, cache its frames locally, release that cache before gameplay and avoid redundant black-screen redraws during loading.
- Correct the lower-screen Nintendo/title transitions and keep menu or HUD text from appearing behind the update changelog.
- Extend adaptive presentation to both console models and supported resolutions, with budget checks that skip interpolation recording when work exceeds its allowance.

### Updates, installation and diagnostics

- Add built-in Stable and Experimental update channels for CIA and 3DSX, with installation confirmation and preservation of game data, saves and settings.
- Fix updater certificates, offline notes, flicker, backgrounds, page navigation and release-version comparison for 2.0.
- Add portable MK64 Extractor packages for Windows x64, macOS Apple Silicon and Linux x64. Create your own game archive on a computer from your legally obtained USA ROM.
- Keep on-device ROM validation and extraction available; no ROM or extracted game archive is included in the download.
- Add configurable SELECT dump or live-overlay bindings and an always-available L+R+SELECT expanded dump, including during AUTO.
- Expand diagnostic state, RAM, resource history, integrity checks and renderer/audio timing. Exclude export pauses from normal timing epochs and separate preparation work from overlapping audio/render measurements.
- Keep automatic laboratory captures disabled in this normal-play build.
- Organize platform code, generated artwork and documentation, and add asset-free sanitizer regressions and reproducible dependency compatibility patches.

### Credits and validation

Solo VS/Battle, AUTO, random cups, Modern Logic, control and HUD ideas, and runtime corrections are adapted from [Epic0522's MarioKart64-3DSPort-Complete](https://github.com/Epic0522/MarioKart64-3DSPort-Complete). [Epic0522's Super Mario 64 3DS Port Ultimate](https://github.com/Epic0522/Super-Mario-64-3ds-port---Ultimate) also provided 3DS implementation and optimization references. These changes were integrated with this port's artwork, five HUD layouts, controls, branding and updater.

The port is based on [Harbour Masters' SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart), the N64 decompilation community and the Nintendo 3DS homebrew libraries. The updater retains its Legend of Doom 3DS attribution and component licenses.

The game simulation remains at 30 Hz. Adaptive presentation can use additional display frames when the workload permits; this is not a guarantee of sustained 30 or 60 FPS on a physical console. Host checks, target compilation and emulator checks do not establish physical Old/New 3DS frame rates, pixel accuracy or long-run stability.

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
