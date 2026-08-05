# Mario Kart 64 for Nintendo 3DS

An experimental native Nintendo 3DS port of the vanilla Mario Kart 64 game code, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart). The port targets the top screen at its native
400x240 resolution with a 5:3 viewport; the bottom screen is intentionally black.

This repository contains no ROM, extracted game data, or copyrighted Nintendo assets. You must generate
`mk64.o2r` from a legally owned supported Mario Kart 64 dump by following the upstream SpaghettiKart extraction
instructions.

## Current status

The native ARM11 executable, Citro3D Fast3D renderer, O2R resource runtime, 3DS input, NDSP audio output, and
3DSX/CIA packaging pipeline compile successfully. The vanilla game remains a work in progress and is not yet a
stable release. The original game simulation runs at 30 Hz; the renderer presents that state at the 3DS display's
60 Hz refresh rate. A true interpolated 60 FPS mode and sustained real-hardware performance have not been claimed.

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
build-3ds/game/mk64-3ds-game-v0.1.3dsx
build-3ds/game/mk64-3ds-v0.1.cia
```

If `makerom` and `bannertool` are not on `PATH`, set `MK64_3DS_TOOLS_ROOT` to a private directory containing their
executables. Packaging tools are not vendored in this repository.

## Install game data

Generate `mk64.o2r` with an official SpaghettiKart desktop build, then copy only your locally generated archive to:

```text
sd:/3ds/mk64-3ds/mk64.o2r
```

Install the CIA with your normal homebrew installer, or place the 3DSX in a Homebrew Launcher application folder.
The port never embeds or redistributes the O2R archive.

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

## Upstream and legal notice

SpaghettiKart and its submodules retain their respective upstream licenses and notices. This project is an
unofficial fan port and is not affiliated with or endorsed by Nintendo or Harbour Masters. Mario Kart and Nintendo
3DS are trademarks of Nintendo.
