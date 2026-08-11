# Mario Kart 64 for Nintendo 3DS

![Mario Kart 64 for Nintendo 3DS](platform/3ds/assets/banner.png)

An experimental native Nintendo 3DS port of the vanilla Mario Kart 64 game code, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart). The top screen supports a Wide 5:3 viewport and a
centered Original 4:3 viewport. The default mode renders at 400x240; every model except the original Nintendo 2DS can
also select an 800-pixel horizontal rendering mode after restarting. The bottom screen provides a dimmed game backdrop,
touch-enabled options and diagnostics, and an automatic race HUD.

This repository contains no ROM, extracted game data, or copyrighted Nintendo assets. Put your legally owned
Mario Kart 64 USA ROM in `sd:/3ds/MK64/`. The 3DS app creates and uses that folder as the local game-data
workspace.

## Current status

The native ARM11 executable, on-device O2R generator, Citro3D Fast3D renderer, O2R resource runtime, bottom-screen
interface, 3DS input, NDSP audio output, and 3DSX/CIA packaging pipeline compile successfully. The vanilla game
remains a work in progress and is not yet a stable release.

The v0.18 pre-release adds a native bottom-screen interface using textures loaded from the owner-generated local O2R
archive. Game Select mirrors the `L Option` and `R Data` actions. Options captures controller and touch input without
changing the top-screen selection and provides Game, Screen, Gameplay, and Developer tabs. During a race, the bottom
screen uses the selected course preview as a cropped background and displays time, lap, current item, the leading five
racers, and the minimap. Pausing automatically replaces that race HUD with Options. Settings are stored locally in
`sd:/3ds/MK64/mk64-3ds.cfg`.

The renderer now implements the grayscale-tint state used by Game Select, Player Select, and Course Select, and it
restores the opaque black tiles behind character portraits. It batches packed-vertex cache flushes, avoids duplicate
texture-resource lookups and wrapper allocations on hot hits, and expands the bounded texture cache for animated kart
frames. New 3DS midpoint interpolation uses fixed-capacity recording and lookup tables instead of the unbounded desktop
recorder or per-matrix map allocations.

Mario Kart 64's simulation remains 30 Hz. At 400 pixels on New Nintendo 3DS, v0.18 can present a bounded
matrix-interpolated midpoint between simulation key frames. Old Nintendo 3DS and the 800-pixel quality mode present
key frames at a 30 Hz target. These are implementation targets, not measured guarantees: sustained 60 FPS and 30 FPS
are not claimed without representative physical-hardware captures.

First-run extraction now leaves the 8 MiB game arena unreserved until game data is ready and avoids duplicating
read-only Torch input buffers. The SD-card installer log mirrors progress and multi-line extractor diagnostics, adds
per-YAML stages and real heap/linear-memory checkpoints, and closes an interrupted O2R writer before validating its
partial output. These changes compile successfully, but their visual result, extraction completion, and performance
still require physical Nintendo 3DS testing.

Known incomplete effects include framebuffer-copy/readback features used by course video screens. Old Nintendo
3DS and New Nintendo 3DS performance still require real-hardware profiling.

## Build

Requirements:

- devkitPro with devkitARM, libctru, Citro3D, and the 3DS CMake toolchain
- CMake 3.20 or newer
- `makerom` and `bannertool` for CIA packaging (the 3DSX build does not require them)

Clone with submodules, then run:

```sh
git submodule update --init --recursive
./platform/3ds/build.sh
```

The CLI writes the game package to:

```text
build-3ds/game/mk64-3ds-game-v0.18.3dsx
build-3ds/game/mk64-3ds-v0.18.cia
```

If `makerom` and `bannertool` are not on `PATH`, set `MK64_3DS_TOOLS_ROOT` to a private directory containing their
executables. Packaging tools are not vendored in this repository.

## Install game data

Create this folder on your SD card:

```text
sd:/3ds/MK64/
```

Place your legally owned Mario Kart 64 USA ROM there. Supported names:

```text
sd:/3ds/MK64/Mario Kart 64.z64
sd:/3ds/MK64/mk64.z64
sd:/3ds/MK64/baserom.us.z64
```

On first boot, the 3DS build checks the ROM, copies its public extraction metadata into the local MK64 folder,
and shows a preparation screen while it writes the O2R directly to the SD card. If extraction succeeds, it creates:

```text
sd:/3ds/MK64/mk64.o2r
```

The final archive is only moved into place after it closes successfully, so an interrupted installation is retried on
the next launch. Later launches reuse that generated archive. This release does not embed or redistribute the ROM,
generated O2R archive, or extracted game data.

If preparation stops, copy the local diagnostic file from the SD card before retrying:

```text
sd:/3ds/MK64/mk64-install.log
```

The log records the ROM check, RomFS metadata copy, every bottom-screen progress line, per-YAML O2R stages,
heap/linear-memory checkpoints, finalization, multi-line errors, and SD-card I/O error codes.
It is created on the SD card only; do not attach it to a public issue or release because it may contain local device
details.

During this first-run step the top screen shows the bundled loading image with an overlaid progress bar, while the
bottom screen mirrors the live installer output. The game memory arena is reserved after game-data preparation,
the ROM is loaded after the Torch configuration is parsed, and the 3DS O2R writer spools the ZIP central directory to
the SD card instead of keeping it in RAM. The 3DS cartridge and read-only binary readers borrow their input buffers,
Torch hash-cache generation is disabled for this one-shot installer, and memory checkpoints are included in the local
installation log. Runtime memory is based on the 64 MiB Old 3DS application limit; the renderer uses the vanilla
archive's actual texture-size requirements rather than reserving a 1024x1024 upload surface.

Install the CIA with your normal homebrew installer, or place the 3DSX in a Homebrew Launcher application folder.

## Controls

| Nintendo 3DS | Nintendo 64 |
| --- | --- |
| Circle Pad | Control Stick |
| A / B | A / B |
| R | R (hop/drift) |
| L | L; opens bottom Options from Game Select or pause |
| Select | Save a diagnostic dump (not passed to the game) |
| D-Pad | D-Pad |
| X / Y | C-Up / C-Left |
| ZL / ZR | C-Down / C-Right |
| C Stick | Hold for the selected Turbo Speed during a race |
| Touch screen | Open and navigate bottom-screen controls |
| Start | Start |

## Bottom-screen options

Open Options with `L` from Game Select. The same panel opens automatically when a race is paused. Use the D-Pad and
`A`/`B`, switch tabs with `L`/`R`, or tap the touch screen. While the panel is open, its input is captured and does not
change the top-screen menu.

- **Screen:** Wide or Original 4:3 aspect ratio, top HUD on/off, and 400/800-pixel rendering. The 800-pixel mode is
  unavailable on the original Nintendo 2DS, requires a restart, and uses a 30 Hz presentation target. The default
  400-pixel mode is recommended on Old Nintendo 3DS for performance.
- **Gameplay:** C-Stick Turbo Speed from x1 through x5 and saturating master volume at 25, 50, 75, 100, 150, or 200%.
- **Developer:** request a local memory dump, show a two-second FPS value on the top screen, or open a diagnostic
  overlay with version, console model, two- and ten-second FPS, memory, renderer, audio, state, and course data.

## Runtime diagnostics

Every game launch writes a local stage log to:

```text
sd:/3ds/MK64/dump/runtime.log
```

After a forced reboot, copy this file before launching the port again because the next launch replaces it.

Press `Select`, or hold `L + R + A` together, to create a timestamped diagnostic session under:

```text
sd:/3ds/MK64/dump/dump-YYYYMMDD-HHMMSS/
```

The diagnostic input worker is independent of the main game loop, so the shortcut can still be detected when the
renderer thread is stuck. A session contains `info.txt`, a copy of `runtime.log`, the active game arena when it is
readable, and the current display list when available. `info.txt` records the current startup/rendering stage,
resource name, memory-region totals, heap use, display-list watchdog state, and the process memory map.

These files remain on the SD card and may contain private device details or owner-generated game data. Inspect or
share only the small text files privately when possible; never publish the raw binary dumps in a repository or
release.

## Upstream and legal notice

SpaghettiKart and its submodules retain their respective upstream licenses and notices. This project is an
unofficial fan port and is not affiliated with or endorsed by Nintendo or Harbour Masters. Mario Kart and Nintendo
3DS are trademarks of Nintendo.
