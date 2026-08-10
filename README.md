# Mario Kart 64 for Nintendo 3DS

An experimental native Nintendo 3DS port of the vanilla Mario Kart 64 game code, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart). The port targets the top screen at its native
400x240 resolution with a 5:3 viewport; the bottom screen is intentionally black.

This repository contains no ROM, extracted game data, or copyrighted Nintendo assets. Put your legally owned
Mario Kart 64 USA ROM in `sd:/3ds/MK64/`. The 3DS app creates and uses that folder as the local game-data
workspace.

## Current status

The native ARM11 executable, on-device O2R generator, Citro3D Fast3D renderer, O2R resource runtime, 3DS input,
NDSP audio output, and 3DSX/CIA packaging pipeline compile successfully. The vanilla game remains a work in
progress and is not yet a stable release. The original game simulation and the current 3DS renderer remain at 30 Hz.
The unbounded desktop matrix-recording interpolation path used by v0.11 is disabled because it terminated the
Old 3DS build during startup. A bounded 3DS-specific 60 Hz path remains future work; sustained 60 FPS is not claimed.

The v0.12 test build enables native 26.8 kHz stereo NDSP output, corrects independent two-texture UVs and
second-cycle RDP texture selection, removes the extra per-upload texture staging allocation, and initializes the
game memory arena before resource and renderer allocations. The CIA uses the supplied custom icon and a compact
flat HOME Menu banner.

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
build-3ds/game/mk64-3ds-game-v0.12.3dsx
build-3ds/game/mk64-3ds-v0.12.cia
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

The log records the ROM check, RomFS metadata copy, O2R archive progress, finalization, and SD-card I/O error codes.
It is created on the SD card only; do not attach it to a public issue or release because it may contain local device
details.

During this first-run step the top screen shows the bundled loading image with an overlaid progress bar, while the
bottom screen mirrors the live installer output. The game memory arena is allocated only after game-data preparation,
the ROM is loaded after the Torch configuration is parsed, and the 3DS O2R writer spools the ZIP central directory to
the SD card instead of keeping it in RAM. The 3DS cartridge-header reader does not duplicate the full ROM, incremental
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
| L | L |
| Select | Z |
| D-Pad | D-Pad |
| X / Y | C-Up / C-Left |
| ZL / ZR | C-Down / C-Right |
| Start | Start |

## Runtime diagnostics

Every game launch writes a local stage log to:

```text
sd:/3ds/MK64/dump/runtime.log
```

After a forced reboot, copy this file before launching the port again because the next launch replaces it.

Hold `L + R + A` together to create a timestamped diagnostic session under:

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
