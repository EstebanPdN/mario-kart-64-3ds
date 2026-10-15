# Contributing to the native 3DS port

Start with the [development guide](platform/3ds/docs/DEVELOPMENT.md) and
[source map](platform/3ds/docs/SOURCE_MAP.md). The platform layer is maintained
here; upstream code and libraries remain at their pinned submodule revisions.
Adaptations of those files are generated into the build directory by the
corresponding CMake module. Change that module rather than editing a generated
file or silently changing a submodule revision.

Before changing behavior, identify the existing contract and the intended
correction. Preserve title identity, data paths, older saves/configuration,
input semantics, five HUD layouts and the original 30 Hz simulation unless a
specific change requires otherwise. See [compatibility](platform/3ds/docs/COMPATIBILITY.md).
Separate a structural move from an algorithm change so its effects can be
compared. Do not add per-frame file I/O, asset generation or unbounded caches.

Run the asset-free host checks with Python 3 and Clang:

```sh
python3 platform/3ds/tests/check-project.py
python3 platform/3ds/tests/run-host-tests.py --output /tmp/mk64-host-tests
./platform/3ds/build.sh
```

The host runner uses disposable synthetic settings and sanitizer-enabled
executables outside the checkout. It does not need a ROM. Optional map-art
verification requires Pillow; see [sprite authoring](platform/3ds/docs/NATIVE_SPRITES.md).
Updater and archive integration have additional inputs documented in
[validation](platform/3ds/docs/VALIDATION.md).

Document ownership, borrowing, units, failure recovery and unusual platform
constraints at the boundary that imposes them. Keep comments useful to the
next change rather than describing each assignment. An archive texture pointer
must not outlive its lease; a GPU image must not be freed while a submitted
frame can still read it. [Architecture](platform/3ds/docs/ARCHITECTURE.md) describes
these owners and the corresponding teardown rules.

Keep a change reviewable: describe its trigger, resulting behavior, relevant
checks and limits. Compilation, host tests, emulator observations and physical
Old/New 3DS measurements are different evidence. Do not claim that one proves
another. Include a repeatable scene or synthetic fixture for a rendering,
lifetime, timing or persistent-format correction.

Do not contribute ROMs, extracted Nintendo art/data, saves, dumps, caches,
credentials or diagnostic logs. Game data remains owner-provided runtime input.
Preserve upstream attribution and component licenses; see the root README's
[license notice](README.md#license-and-legal-notice).
