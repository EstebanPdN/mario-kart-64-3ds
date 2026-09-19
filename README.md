# Mario Kart 64 for Nintendo 3DS

<img width="1672" height="941" alt="Mario Kart 64 running on Nintendo 3DS" src="https://github.com/user-attachments/assets/6b367030-2f4d-4c5d-b295-7d6d7f7d2ceb" />

Native Nintendo 3DS port of Mario Kart 64, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart). It is designed
specifically for the 3DS family, with a dual-screen interface and hardware-aware
rendering, audio, and memory profiles.

No ROM, ROM fragment, save file, or extracted Nintendo game data is distributed
with this project. You must provide your own legally obtained USA Mario Kart 64
ROM.

Made with help from Codex.

## Community

Join the Discord for project updates, support, bug reports, suggestions, and
other Nintendo 3DS homebrew projects:

https://discord.gg/SMW49UMkw

## Features

* Native 400×240 resolution
* Adjustable 0.50×–1.00× render scale
* Low, Normal, and High render distance
* Five HUD layouts
* Bilinear, Blur, and CRT filters
* Dual-screen HUD and touch controls
* Optimized PICA200 rendering
* Original 30 Hz simulation with adaptive frame interpolation
* On-device ROM validation and extraction
* Diagnostic dumps with `SELECT`

Multiplayer is planned but is not currently available.

## Performance

Mario Kart 64 retains its original 30 Hz game simulation. On Old and New Nintendo
3DS, in either supported resolution, the port may render an additional
matrix-interpolated midpoint frame when recent frame, GPU, audio, and resource
activity leave enough headroom. It automatically falls back to the required
30 Hz keyframes under pressure.

The midpoint path is adaptive; it is not a promise of a fixed or sustained
60 FPS mode.

The Display menu adjusts internal resolution from 0.50x through 1.00x and
scales it to the complete top screen. Bilinear uses a single GPU
presentation pass, Blur uses four lightweight samples, and CRT combines
bilinear scaling with a small procedural scanline and shadow-mask pattern.
A 1.00x render scale with Bilinear selected keeps the original direct presentation
path and does not allocate the intermediate target.

The renderer cleans exact Fast3D vertex and texture ranges through the local
ARM11 kernel path, with the established GSP service call retained as a
compatibility fallback. Citro2D frames capture and clean only their bounded
linear vertex/index allocation span through the same local path instead of
flushing the complete linear heap through a blocking GSP round trip. Unchanged
filter-presentation geometry is retained between frames.
Vertex colors use a GPU-native byte format, reducing the fixed Fast3D vertex
allocation and per-frame vertex traffic by 25% without changing the intended
color precision. Audio synthesis writes directly into a reserved NDSP wave
buffer and uses the same clean-only ownership transfer instead of producing and
copying an intermediate block. These changes reduce contention and memory
traffic on both hardware profiles; sustained performance still requires
physical-hardware measurement.

Render distance is independent of scale: Normal uses 75% and Low uses 50% of
that course's far distance, with a fog transition over the final quarter.
Only fully distant depth-tested triangles are rejected; intersecting polygons
remain to avoid holes. High disables this additional fog/culling pass. Gains
are scene-dependent and must be measured separately on Old and New hardware.

HUD layouts are selectable without restarting:

| Layout | Top screen | Bottom screen |
| --- | --- | --- |
| Clean | Clear race view; optional Y HUD cycle | Existing complete race HUD |
| MK7 | Item left, position upper right | Lap, time, standings, map |
| MKDS | Item left, lap upper right, position lower left | Time, standings, map |
| MKDS 2 | Item left, position lower left | Lap, time, standings, map |
| Classic | Rebalanced complete HUD at native display resolution | Complete race HUD |

Top-screen race widgets fade in over one second after the countdown. The empty
item frame remains visible between pickups. Top-screen item, lap,
and position elements are larger, with a dark position shadow. Lower standings
use separated portraits and a larger two-column finish layout. Multiplayer
keeps the native per-viewport HUD.

The renderer compiles immutable combiner plans once per material. Distance fog
skips batches entirely before the start of its ramp and avoids redundant fog
texture binds within a 3D pass. These changes reduce repeated CPU/state work;
real-hardware FPS gains have not yet been measured.

Extraction buffers archive writes and stores tiny entries without deflation.
All payload/CRC validation and atomic completion remain enabled. The progress
screen redraws at most ten times per second while retaining every log message.

Course Data uses an enlarged scrolling course list and a dual-screen details
view with records, preview and map above, and course actions below. Time Trial
and Grand Prix results use centered text and shadows. Time Trial ghosts use the
two original save slots in `sd:/3ds/MK64/controller-pak.bin`.

Startup shows a small animated Lakitu with a checkered flag at the center of
the upper screen, with black backgrounds on both screens. Its 32 native frames
are loaded from your O2R and cached in `sd:/3ds/MK64/cache/loading-lakitu-v1.bin`.
The cache rebuilds when missing, damaged, or when the archive changes. Its
memory and animation worker are released before gameplay. First-time ROM
extraction keeps its progress display. Course loading artwork remains optional
under Display > Show loading screens. Errors remain visible.

## Installation

Download either the CIA for installation with FBI or the full 3DSX application
for the Homebrew Launcher from the
[latest release](https://github.com/EstebanPdN/mario-kart-64-3ds/releases/latest).

Create this folder on your SD card:

```text
sd:/3ds/MK64/
```

Place your USA Mario Kart 64 ROM in that folder and name it:

```text
mk64.z64
```

The ROM must use the `.z64` byte order. ROMs in another byte order can be
converted with the [Hack64 ROM Swapper](https://hack64.net/tools/swapper.php).

### Automatic extraction

Launch the port with the ROM in place. The 3DS validates it and creates:

```text
sd:/3ds/MK64/mk64.o2r
```

The first extraction can take a long time, so keep the console charged. You can
close the lid while extraction continues. Press `START` to cancel, remove the
incomplete output, and exit safely. Later launches reuse the completed archive.

### Desktop extraction

For a faster setup, use
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) on a computer
to create `mk64.o2r`, then copy it to `sd:/3ds/MK64/` alongside your ROM:

```text
sd:/3ds/MK64/mk64.z64
sd:/3ds/MK64/mk64.o2r
```

## Controls

| Nintendo 3DS | Function |
| --- | --- |
| Circle Pad | Control Stick |
| A / B | A / B |
| R | Hop / Drift |
| L | Use item / Open Options from Game Select |
| X | Camera distance |
| Y | Cycle top-screen HUD during races |
| ZL / ZR | C-buttons |
| C-Stick | Turbo |
| Touch Screen | Navigate bottom-screen menus |
| SELECT | Create diagnostic dump |
| START | Start / Pause; cancel and exit during extraction |

## Diagnostics and bug reports

If you encounter a crash, graphical bug, or performance problem, press
`SELECT`. The port creates a diagnostic folder under:

```text
sd:/3ds/MK64/dump/
```

Performance captures include `performance.csv` and `performance.txt` with the
latest 256 simulation ticks: CPU/wait/audio timings, submitted images, adaptive
presentation reasons, resource reads, uploads and active settings. Dump pauses
are excluded from the next FPS measurement; `FPS --` indicates its warmup.

New captures are numbered `000-dump-...`, `001-dump-...`, and so on. The
sequence survives restarts while captures remain. An empty dump collection
restarts at `000`, including after manual deletion or Clean dumps. Older
captures retain their original names. A paused progress screen appears on the lower screen,
using the current race/menu background and the same font as the FPS counter,
without a separate dark panel.

Developer > Clean dumps deletes all contents of the diagnostic folder, including
expanded RAM captures, and starts a fresh runtime log. The next capture restarts at `000`; game data, settings and saves are unaffected.

Before entering the game, the port attempts to load every compressed resource
from the owner-generated O2R archive into RAM in small blocks, without requiring
one large contiguous allocation. On success it closes the SD file handle; kart
angles, course resources and effects then use RAM with decompression and CRC
validation. ZIP names and headers are not duplicated in the payload blocks.
Optional interpolation storage uses the separate linear heap.
The CIA requests expanded application memory on Old 3DS (80MB system mode)
and New 3DS (124MB system mode). Old-model launch/exit can take longer while
the system changes memory mode. The ordinary heap retains at least an 8 MiB
reserve at preload time for later game allocations; this is not a guarantee
of sustained frame rate or a bound on every future allocation.

Use the installed CIA for this path. A 3DSX launcher must grant enough memory;
if the archive cannot fit with the reserve, the validated SD reader remains
available so startup can continue. Diagnostics explicitly report this fallback,
its memory budget and the required bytes; read errors are reported separately.
SD fallback can add loading/gameplay stalls. Existing O2R archives work without
re-extraction. Save data and controller pak reads happen at boot;
intentional save writes and requested diagnostics still use the SD card.

During the game, diagnostic logging uses a bounded 64 KiB RAM ring. Automatic
heartbeats and buffer overflow do not write to SD. Explicit dumps and orderly
shutdown flush it; overflow drops the oldest complete lines with a marker in
the next written log. Resource decoding, texture conversion and GPU work still
have CPU/GPU costs even when physical archive I/O is zero.

The trace includes interpolation acceptance/rejection reasons and matrix counts.
`interpolation.txt` and `matrices-current.csv` / `matrices-previous.csv` describe
the recorder state. `race-start.csv` retains the latest race's initial driving
ticks plus up to 32 countdown ticks even after the rolling trace has moved on.
Do not calculate frame rates across diagnostic-pause epochs.

Hold `L` while pressing `SELECT` for an expanded application RAM snapshot with a
region manifest. This additionally captures readable application-owned code,
heaps and stacks; it excludes device registers, VRAM and service shared mappings.
It is larger and slower than a normal capture, and is not a whole-system snapshot.

Share diagnostic folders privately and describe the console model, display mode,
game mode, and what happened immediately before the issue.

## Releases

GitHub releases provide an installable CIA, a full Homebrew Launcher 3DSX, an
FBI QR code, and SHA-256 checksums. GitHub also generates source archives from
each release tag.

[Download the latest release](https://github.com/EstebanPdN/mario-kart-64-3ds/releases/latest)

## Building

Requirements:

- devkitPro
- devkitARM
- libctru
- Citro3D
- CMake 3.20 or newer
- `makerom` and `bannertool` for CIA packaging

Clone the repository with its submodules, then run:

```sh
git submodule update --init --recursive
./platform/3ds/build.sh
```

Build output is written under `build-3ds/game/` by default. Local CIA packaging
may use `MK64_3DS_BANNER_CGFX=/absolute/model.cgfx` and
`MK64_3DS_BANNER_AUDIO=/absolute/sound.wav` to supply a 3D banner and audio.
The assets are packaging inputs and do not need to be added to the source tree.

## Credits

This project is based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) and the work of
the N64 reverse-engineering, decompilation, and Nintendo 3DS homebrew
communities.

Additional optimization and 3DS implementation techniques were studied from
other open-source Nintendo 64 ports, including
[Super Mario 64 3DS Port Ultimate](https://github.com/Epic0522/Super-Mario-64-3ds-port---Ultimate).

## Legal

This repository contains source code, build scripts, redistributable
port-specific artwork, and extraction logic. It does not distribute Mario Kart
64 ROMs, `mk64.o2r`, saves, or extracted copyrighted Nintendo game assets.

Users are responsible for providing their own legally obtained compatible ROM.
This is an unofficial fan project and is not affiliated with or endorsed by
Nintendo or Harbour Masters.

### Built-in updater

Open **Options > Game > Update** to check Stable or Experimental releases.
The upper screen shows the changelog; L/R changes pages. Use A or touch to
select and B to return. Installation requires confirmation and closes the game.
Updates verify their download and preserve your ROM/O2R, saves and settings.
See the [updater documentation](platform/3ds/update/README.md) for details.
