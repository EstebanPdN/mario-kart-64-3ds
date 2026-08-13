# Mario Kart 64 for Nintendo 3DS

<img width="1672" height="941" alt="mariokart2" src="https://github.com/user-attachments/assets/6b367030-2f4d-4c5d-b295-7d6d7f7d2ceb" />


Native Nintendo 3DS port of Mario Kart 64, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart).

This port is designed specifically around the Nintendo 3DS, with dual-screen
features and performance optimizations for real hardware.

No ROM or extracted Nintendo game data is distributed with this project.
You must provide your own legally obtained USA Mario Kart 64 ROM.

## Community

Join the Discord for project updates, support, bug reports, suggestions, and
other Nintendo 3DS homebrew projects:

https://discord.gg/SMW49UMkw

## Features

- Native 400x240 gameplay.
- Press `SELECT` at any time to create a diagnostic dump for bug reports.
- Wide 5:3 and Original 4:3 display modes.
- Dual-screen interface
- Bottom-screen race HUD
- Touch menu
  
- MULTIPLAYER IS A PLANNED FEATURE
  
## Installation

Install the CIA normally, then create:

```text
sd:/3ds/MK64/
```

Place your USA Mario Kart 64 ROM inside that folder.

Name it:

```text
mk64.z64
```
OR

```text
Mario Kart 64.z64
```

### Option 1 — Automatic Extraction

Simply place the ROM in the folder and launch the game.

The 3DS will validate the ROM and generate:

```text
sd:/3ds/MK64/mk64.o2r
```

This is done entirely on the console.

The first extraction can take a long time, so keep the console charged.
You can close the lid while it is working and extraction will continue.

Press `START` during extraction to cancel it, delete the incomplete output,
and exit the application safely.

Once extraction finishes successfully, later launches will reuse the generated
`mk64.o2r` file.

### Option 2 — Recommended

For a faster and more reliable setup, use
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) on your computer
to extract the game resources from your ROM.

After SpaghettiKart creates `mk64.o2r`, copy it to:

```text
sd:/3ds/MK64/
```

Your folder should then contain both:

```text
sd:/3ds/MK64/mk64.z64
sd:/3ds/MK64/mk64.o2r
```

Launch the port and you're ready to play.

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

## Diagnostics

If you find a crash, graphical bug, performance issue, or other problem, press:

```text
SELECT
```

The port will create a diagnostic folder under:

```text
sd:/3ds/MK64/dump/
```

These dumps can help diagnose problems on real Nintendo 3DS hardware.

## Releases

GitHub releases include:

- Installable CIA
- Homebrew Launcher 3DSX
- Source code archive

Latest release:

https://github.com/EstebanPdN/mario-kart-64-3ds/releases/latest

## Building

Requirements:

- devkitPro
- devkitARM
- libctru
- Citro3D
- CMake 3.20+
- `makerom` and `bannertool` for CIA packaging

Clone the repository with its submodules and run:

```sh
git submodule update --init --recursive
./platform/3ds/build.sh
```

Builds are generated under:

```text
build-3ds/game/
```

## Credits

This project is based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) and the work of
the N64 reverse-engineering, decompilation, and Nintendo 3DS homebrew communities.

Additional optimization and 3DS implementation techniques were studied
from other open-source Nintendo 64 ports, including
[Super Mario 64 3DS Port Ultimate](https://github.com/Epic0522/Super-Mario-64-3ds-port---Ultimate).

## Legal

This repository contains only source code, build tools, port-specific assets,
and extraction logic.

It does **not** distribute Mario Kart 64 ROMs, `mk64.o2r`, or extracted
copyrighted Nintendo game assets.

Users are responsible for providing their own legally obtained compatible ROM.

This is an unofficial fan project and is not affiliated with or endorsed by
Nintendo or Harbour Masters.
