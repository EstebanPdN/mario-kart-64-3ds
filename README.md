# Mario Kart 64 3DS

![Mario Kart 64 3DS](platform/3ds/assets/banner.png)

Native Nintendo 3DS port work based on the
[n64decomp/mk64](https://github.com/n64decomp/mk64) decompilation.

This is not an N64 emulator. The project will replace the original libultra,
RSP/RDP, and video paths with native 3DS platform code.

## Status

`v0.1` is the platform bootstrap. It establishes a native 400x240 top-screen
PICA200/Citro2D presentation, a deliberately black bottom screen, Home/sleep
lifecycle handling, and an Old 3DS-compatible package layout. It does **not**
yet load a ROM or run Mario Kart 64 gameplay.

No claim of playable performance or stable 60 FPS is made at this stage. The
60 FPS goal will be measured on real Old and New Nintendo 3DS hardware only
after the game loop and renderer are integrated.

## Building

Requirements:

- devkitPro with devkitARM, libctru, Citro2D, and Citro3D
- CMake
- `makerom` and `bannertool` for CIA packaging

Build both 3DS packages:

```sh
./platform/3ds/build.sh
```

Outputs are written under `build-3ds/game/`.

## Legal notice

This repository contains only port source code, build scripts, and original
port artwork. It contains no Nintendo ROM, ROM fragment, extracted texture,
audio, course data, save, or other copyrighted game asset.

Future users will need to supply their own legally obtained compatible USA
Mario Kart 64 ROM on their 3DS SD card. The ROM must never be copied into the
CIA, 3DSX, repository, or release assets.

Nintendo owns Mario Kart 64 and its game content. This is an unofficial fan
project.

## Credits

- [n64decomp/mk64](https://github.com/n64decomp/mk64) — Mario Kart 64
  decompilation and primary source reference.
- EstebanPdN — 3DS port direction and release maintenance.
