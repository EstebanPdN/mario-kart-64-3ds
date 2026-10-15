# Developing the native 3DS port

This guide describes the 2.0 experimental line. Start from a recursive clone;
the pinned submodules are build inputs, not optional reference downloads.

## Build prerequisites

Install devkitPro's devkitARM toolchain, libctru, Citro3D, Citro2D and tex3ds.
The host needs CMake 3.20+, Python 3, a C/C++ compiler, Make, pkg-config and the
usual archive/download tools. CIA packaging additionally needs `makerom` and
`bannertool`; without them the script reports that only the full 3DSX is ready.

```sh
git clone --recursive https://github.com/EstebanPdN/mario-kart-64-3ds.git
cd mario-kart-64-3ds
./platform/3ds/build.sh
```

The script verifies/applies the checked-in Torch compatibility patch. Do not
manually reset that submodule during a build. It builds the updater's pinned
curl/mbedTLS/Jansson dependencies when they are absent, configures the complete
engine and writes results under `build-3ds/game/`. Internet access is needed
when dependencies have not yet been downloaded.

Use `mk64-3ds-game-vVERSION.3dsx` for the complete game. A small probe/bootstrap
target is not a replacement for the game. The CIA is `mk64-3ds-vVERSION.cia`.
`platform/3ds/version.txt` is the shared application/updater version input.

| Build variable | Meaning |
| --- | --- |
| `DEVKITPRO` | Toolchain root; defaults to `/opt/devkitpro`. |
| `MK64_3DS_BUILD_DIR` | Separate build directory; use an absolute path. |
| `MK64_3DS_JOBS` | Parallel build jobs; defaults to four. |
| `MK64_3DS_UPDATE_DEPS` | Existing target dependency prefix. |
| `MAKEROM`, `BANNERTOOL` | Explicit packaging executable paths. |
| `MK64_3DS_BANNER_BNR` | Existing combined banner/audio packaging input. |
| `MK64_3DS_BANNER_CGFX`, `MK64_3DS_BANNER_AUDIO` | Optional model/audio packaging inputs. |

No ROM or extracted asset is required to compile. Running the game still needs
the owner's compatible ROM/archive as described in the root README. Keep those
files, saves, diagnostics and build output out of source control.

## Source map and generated adaptations

`platform/3ds/source/` owns platform services and integration. Portable policies
live in `include/`; small ownership and timing policies can be tested on a host.
Data-only artwork is private implementation in `source/art/`, separate from
service headers. Its editable producers live in `assets/`. The
[source map](SOURCE_MAP.md) identifies every runtime/build family and the
[documentation index](README.md) separates use, reference and explanation.
`cmake/` modules adapt pinned upstream files into the build's `generated/`
directory by responsibility: engine/core, rendering, resources, audio, solo/AUTO
modes, CPU/items and menu behavior. Edit the generator or patch that owns a
change, then rebuild. Editing a generated file is temporary and will be lost.

The bottom interface owns its textures, snapshots and modal state.
`options_actions_3ds.inc.cpp` contains option activation and input;
`options_view_3ds.inc.cpp` contains labels and rendering. They are private
implementation fragments included by `bottom_ui_3ds.cpp`, matching the existing
updater integration pattern. `options_layout_3ds.hpp` is the common ordering and
geometry contract, including touch coordinates.

Use one asset-free host command before a target build:

```sh
python3 platform/3ds/tests/check-project.py
python3 platform/3ds/tests/run-host-tests.py --output /tmp/mk64-host-tests
```

The first validates maintained local documentation links, Python syntax and
standard-library-generated artwork without changing the source. `--artwork`
also verifies course masks when Pillow is installed. The second compiles the
portable fixtures with AddressSanitizer/UndefinedBehaviorSanitizer and runs UI,
settings, recovery/ownership and renderer/dispatch contracts. Its output must
be outside the checkout. `--suite ui` and `--suite renderer` select a smaller
relevant group. No ROM, target SDK or proprietary fixture is needed for these
checks; the renderer group needs the pinned recursive submodules and CMake.

After source relocation, use a fresh target build directory instead of relying
on old objects. Existing downloaded dependencies can remain a separate cache:

```sh
MK64_3DS_BUILD_DIR=/tmp/mk64-clean-game ./platform/3ds/build.sh
```

Record actual compiler/SDK versions, package hash and test route for a candidate;
do not call a host or target build an emulator/hardware test.

## A first menu change

1. Identify the setting's persisted identity in `settings_3ds.cpp`.
2. Change the shared page order in `options_layout_3ds.hpp` if needed.
3. Update its action and label using that identity in the options fragments.
4. Run the UI integration and layout checks in [Validation](VALIDATION.md).
5. Build and inspect both screens, touch activation, back navigation and
   intermediate transition frames. Mocked rendering cannot prove pixel output.
6. Update [Configuration](CONFIGURATION.md) when the user-facing contract changes.

For original menu artwork, run `assets/menu-hints/generate.py`. The generated
RGBA header is checked in so a normal build does not need Pillow or an image
service. Review dimensions, color order and opaque backing before committing.

## Review discipline

Keep dependency upgrades, resource lifetime changes and visual changes small
enough to compare. Document ownership, units, bounds and failure recovery where
they matter. Do not rename pinned upstream APIs merely for style. Changes to
rendering, simulation, save formats or threading require a reproducible scene
or fixture and the corresponding validation level.

The [architecture](ARCHITECTURE.md) records actual boundaries and budgets;
[troubleshooting](TROUBLESHOOTING.md) describes diagnostic evidence. Compilation,
host tests, emulator tests and console measurements establish different facts.
