# Mario Kart 64 for Nintendo 3DS

<img width="1672" height="941" alt="Mario Kart 64 running on Nintendo 3DS" src="https://github.com/user-attachments/assets/6b367030-2f4d-4c5d-b295-7d6d7f7d2ceb" />

Native Nintendo 3DS port of Mario Kart 64, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart). It is designed
specifically for the 3DS family, with a dual-screen interface and hardware-aware
rendering, audio, and memory profiles.

> [!WARNING]
> This is an experimental fan port. Save your work before launching it and
> report crashes or rendering issues with a diagnostic dump whenever possible.

No ROM, ROM fragment, save file, or extracted Nintendo game data is distributed
with this project. You must provide your own legally obtained USA Mario Kart 64
ROM.

Made with help from Codex.

## Community

Join the Discord for project updates, support, bug reports, suggestions, and
other Nintendo 3DS homebrew projects:

https://discord.gg/SMW49UMkw

## Features

- Native 400x240 gameplay and an optional 800x240 high-density top-screen mode.
- Wide 5:3 and Original 4:3 display modes.
- Dual-screen interface, bottom-screen race HUD, and touch menu navigation.
- Hardware-aware Old 3DS and New 3DS resource, HUD, audio, and presentation
  profiles.
- Original 30 Hz game simulation on every model, with an optional adaptive
  midpoint presentation path on New 3DS systems in 400-pixel mode.
- On-device ROM validation and resource extraction.
- Diagnostic dumps created on demand with `SELECT`.

Multiplayer is planned but is not currently available.

## Performance

Mario Kart 64 retains its original 30 Hz game simulation. On New Nintendo 3DS
systems using the 400-pixel top-screen mode, the port may render an additional
matrix-interpolated midpoint frame when recent frame, GPU, audio, and resource
activity leave enough headroom. It automatically falls back to the required
30 Hz keyframes under pressure.

The midpoint path is adaptive; it is not a promise of a fixed or sustained
60 FPS mode. The 800-pixel quality mode presents keyframes only.

## Installation

Download either the CIA for installation with FBI or the full 3DSX application
for the Homebrew Launcher from the
[latest release](https://github.com/EstebanPdN/mario-kart-64-3ds/releases/latest).

Create this folder on your SD card:

```text
sd:/3ds/MK64/
```

Place your USA Mario Kart 64 ROM in that folder and name it either:

```text
mk64.z64
```

or:

```text
Mario Kart 64.z64
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

Attach the complete folder to your bug report and describe the console model,
display mode, game mode, and what happened immediately before the issue.

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

Build output is written under `build-3ds/game/` by default.

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
