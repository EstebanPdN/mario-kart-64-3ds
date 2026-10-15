# Validation and performance measurements

A successful build proves compilation and packaging. Host tests prove the
specified policies under their fixtures. Emulator tests exercise the executable
and guest resource paths. Only measurements on a physical Old/New 3DS establish
console behavior and sustained performance for that model.

## Host regression tests

Run from the repository root with Clang and Python 3:

```sh
python3 platform/3ds/tests/check-project.py
python3 platform/3ds/tests/run-host-tests.py --output /tmp/mk64-host-tests
```

The combined runner covers UI, configuration, file replacement, texture leases,
portable geometry/lighting and generated dispatch, using synthetic fixtures and
sanitizers. `--suite ui` or `--suite renderer` selects the corresponding group.
The CI host job runs the same commands separately from the ARM package job.

Individual existing harnesses remain available:

```sh
python3 platform/3ds/tests/options_integration_test.py --output /tmp/mk64-ui-test
python3 platform/3ds/tests/run-reuse-tests.py --output /tmp/mk64-reuse-test
clang++ -std=c++17 -g -fsanitize=address,undefined -I platform/3ds/include \
  -I platform/3ds/source platform/3ds/tests/options_layout_test.cpp -o /tmp/mk64-layout-test
/tmp/mk64-layout-test
clang++ -std=c++17 -g -fsanitize=address,undefined -I platform/3ds/include \
  platform/3ds/tests/runtime_policies_test.cpp -o /tmp/mk64-policy-test
/tmp/mk64-policy-test
```

The UI integration fixture extracts the actual production handlers and replaces
platform effects with mocks. It checks navigation, actions, modal dismissal,
held-button guards, profile isolation, sliders, SELECT chord priority and text
alignment. It does not claim that GPU pixels or hardware input were tested.

`texture_lease_test.cpp` models reclamation between image and palette acquisition,
plus early return/reset and exception unwinding. `file_replace_test.cpp` injects
rename/restore failures and checks that an old complete settings file survives.
The settings process fixtures also load a backup-only configuration and save it
back to the primary path. These checks do not emulate real SD power loss.

`native_sprite_test.cpp` covers exact bilinear interpolation and hidden-color
alpha fringes, transparent gaps, synthetic source-letter identity, complete y
and shadow bounds, sixty native map masks and malformed RLE rejection. The native-sprite
authoring guide describes private 1:1 image review.

`settings_profiles_test.cpp` covers texture-style persistence, model-dependent presets, legacy configuration
migration, manual Custom mode and save/reload behavior. Run configuration tests
in an isolated temporary directory: the host implementation deliberately uses a
synthetic `sdmc:/3ds/MK64` tree. Never use a real save as a disposable fixture.

`run-update-tests.py` covers manifest parsing and updater failures with host
mocks; its optional live check uses the actual public release endpoints.

## Emulator and hardware routes

Preserve the candidate hash, model, settings and exact route for every run:

1. Boot to the title and inspect both screens.
2. Enter Game Select, inspect fully visible buttons and their intermediate
   reveal/exit frames. Background and border must transform with each label.
3. Enter Display and Advanced with buttons and touch; change an ordinary
   setting and verify Custom. Return with B and the explicit Back row.
4. In Advanced, change Textures with both controller and touch. Compare the same
   scene in all three styles, restart and verify the stored preference. Inspect title
   credit, menu fonts, LAP/clock, four portrait sizes, items, rank, map tint and
   marker alignment. Check all five HUD layouts and both aspect modes.
5. Inspect Gameplay, Developer and race pause; confirm Continue resumes and
   Quit returns to the menu. Check held-A resume without an extra press.
6. Use AUTO for every race performance run. On the initial character grid,
   press SELECT before confirming a character; verify that the kart drives
   itself after the countdown. Keep course, character, class and settings
   identical for comparisons. Include CPU opponents, items, heavy scenes and
   scene transitions. A stationary starting-grid capture is not a race benchmark.
   Capture diagnostics promptly after a slowdown so the recent-frame history
   still contains the event; compare a nearby smooth segment as a control.
7. Repeat a fixed menu/race/quit cycle. Compare memory after the same warmed-up
   state, rather than comparing different courses or a cold cache with a warm one.
8. Test HOME/sleep/resume, audio continuity and exit on the real console. An
   emulator's pause/resume is a different lifecycle event.

## Diagnostic interpretation

SELECT can be assigned to a normal dump in Developer. L+R+SELECT requests the
expanded process-memory capture independently of that setting. Dump creation
pauses gameplay/audio and can take time; the captured screen precedes the
progress overlay. Preserve dumps privately because they contain game memory.

Each dump includes `info.txt`, screenshots, game arena/display-list data and
performance CSVs. Expanded dumps also include mapped process memory. Compare
heap used/free, linear free, resource counts, texture uploads, read traffic,
audio health and cache activity across repeated identical states.

A growing high-water mark can be a retained cache; it does not alone prove a
leak. Conversely, stable totals in a few dumps cannot prove the absence of all
leaks. Trace ownership and reproduce unbounded growth before changing lifetimes.
Use sanitizers for portable code and explicit resource counters on target.

Performance CSV times are in microseconds. Analyze uninterrupted epochs and
exclude diagnostic pauses, scene loads and explicit modal pauses from sustained
race results. Report sample count, median, high percentiles and worst frame,
along with simulation ticks and submitted images. `presents` is not a count of
physical display scanouts. Emulator frame rates are not Old/New 3DS benchmarks.
