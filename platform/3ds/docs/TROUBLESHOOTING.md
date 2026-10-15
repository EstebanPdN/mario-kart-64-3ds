# Troubleshooting — 2.0 experimental

## Startup and data

The game needs the compatible USA ROM in `.z64` byte order, or the archive
created from it, in `sd:/3ds/MK64/`. Follow the root README's extraction route.
A successful build contains the executable and extraction logic, not game data.
If extraction fails, preserve the error text and check ROM validation, free SD
space and the expected paths. Do not replace a complete archive with an
incomplete extraction or distribute the ROM to report a bug.

The CIA can request a larger application-memory mode. A 3DSX launch may have a
smaller budget depending on its launcher. If compressed archive residency is
unavailable, the game retains its validated SD reader. Diagnostics distinguish
that fallback from a corrupt archive/read error. Console launch and exit can
also take longer while changing the application-memory mode.

For a startup failure, include `sd:/3ds/MK64/dump/startup.log`, the current
`runtime.log` from that directory and `sd:/3ds/MK64/mk64-install.log` when
extraction was involved. Each appended startup session records its build;
older `startup-e*.log` files belong to earlier builds. Updater errors use
`sd:/3ds/MK64/update.log`.

## Capturing a visible problem

Hold **L + R**, then press **SELECT** while the problem is visible. The expanded
dump works independently of the single-button SELECT preference. For smaller
routine captures, enable Developer > Select for dump, or choose Memory dump.
AUTO driving can continue between captures; dump writing pauses game/audio.

The numbered folder is under `sd:/3ds/MK64/dump/`. It includes `info.txt`, both
screens, performance histories, arena/display-list data, renderer state and
runtime logs. Expanded captures also include readable application-owned memory
with a region manifest. They exclude device/VRAM/shared mappings and are not
whole-system process samples. Keep dumps private; share only with a trusted
maintainer through an appropriate private channel.

Report the version, console model, CIA/3DSX route, course/mode, display profile,
steps, expected result and actual result. Include a capture made promptly after
the event: the ordinary rolling trace retains only the latest 256 ticks.

## Slow scenes and memory growth

Different sections of a course have different work. Reproduce the same route
with AUTO, compare a smooth section and the slowdown, then inspect CPU/GPU,
audio, resource/decompression and upload timing. The frame limiter is a target,
not a promise. Do not add nested timing columns or include dump-writing pauses
in a sustained FPS calculation. See [Validation](VALIDATION.md).

Compare memory at the same warmed-up state across repeated menu/race/quit
cycles. A retained cache or allocator arena is not by itself a leak. Investigate
unbounded growth and the resource owner's release path. A few stable samples
cannot certify the absence of all leaks.

After saving a compact analysis, delete captures that are no longer needed.
Developer > Clean dumps clears the diagnostic directory while preserving game
data, saves, settings and monotonic capture numbering. Do not run cleanup before
preserving evidence needed to diagnose an unresolved failure.

## Updates and lifecycle

Choose Stable or Experimental in Options > Game > Update. A failed network or
verification step must leave the installed game and data intact. Retry after
checking connectivity and the displayed error. Installation requires the
updater's explicit confirmation and exits the game. Keep the current CIA as a
recovery copy. Details are in [the updater guide](../update/README.md).

HOME/sleep/resume, audio continuity and shutdown need console testing. Emulator
pause/resume exercises a different lifecycle and cannot establish those results.
