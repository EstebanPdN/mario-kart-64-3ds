# MK64 Extractor

A portable desktop utility for preparing Mario Kart 64 game data for the
Nintendo 3DS port. Drag in your ROM or select **CHOOSE ROM**, choose where to
save `mk64.o2r`, then copy it to `SD:/3ds/MK64/` alongside `mk64.z64`.

The portable packages include their own Python runtime, Qt interface and native
extraction engine. No Python, compiler or dependency installation is needed.
Unpack the whole package before opening it; keep its supporting folders together.

- **macOS:** open `MK64 Extractor.app` (Apple Silicon, macOS 14 or later).
- **Windows:** open `MK64 Extractor.exe` (64-bit Windows 10/11).
- **Linux:** open `MK64 Extractor` (x86-64 desktop, Ubuntu 22.04 / Debian 12 or
  equivalent with glibc 2.35 or later).

The utility accepts the unmodified USA ROM in `.z64`, `.n64` or `.v64` byte
order. It checks the complete normalized ROM SHA-1 before extraction and reads
every generated archive member back to verify its CRC. Other regions, patched
ROMs and renamed unrelated files are rejected.

Conversion runs locally without uploads, telemetry or downloads. **CANCEL**,
Escape or closing the window stops the worker safely. Existing output files
are replaced only after explicit confirmation and successful validation.
The input ROM is never overwritten. Keep the target drive connected until the
utility reports completion.

The square window uses the exact background layer from the port's 3DS ROM
extraction screen, a transparent port logo and a centered menu. The background
preserves the console's RGB565 colors and existing attenuation, without adding
another tint or opacity effect. A centered crop preserves its proportions.
AI-generated headings and action tiles use the existing MK64 Reloaded
menu textures as their visual references. White text uses a newly generated
transparent HD atlas matching the original MK64 menu lettering: all 26 capital
letters, ten digits and twelve punctuation marks, including matching slashes,
colons and percent signs. No original low-resolution glyph data is required.
The application icon uses the original master artwork of the 3DS port's icon.
The window opens centered at 512 x 512 logical pixels, or smaller when needed
to fit within 80% of the screen's available width and height. High-density
displays retain their normal operating-system scaling.

## Command line

From the source directory, after preparing the native engine:

```sh
python app.py --rom "Mario Kart 64.z64" --output "mk64.o2r"
```

The command line refuses an existing destination. The converter core requires
only Python's standard library and the bundled native engine.

## Build from source

Use Python 3.11+, CMake 3.24+, Git and a C++20 compiler. Initialize the port's
submodules first. Windows native builds use Visual Studio 2022's C++ tools;
Linux builds can use GCC or Clang; macOS builds use Xcode Command Line Tools.

```sh
python -m venv .venv
# Activate the environment using your operating system's normal command.
python -m pip install -r requirements.txt
python build.py
python build.py --prepare-only --package
```

The source launchers can set up this development environment, but they are not
the portable end-user packages. Native dependencies are pinned by the existing
Torch build. CMake may download those dependencies during a development build.
The distributed application itself does not download anything.

```sh
python -m unittest -v test_core.py
```

The 3DS game, ROM handling and archive format are unchanged. This utility creates
the existing O2R format; it is not a CIA installer. Component-specific credits
and notices are in `CREDITS.txt`, `licenses/` and the original dependency sources.
