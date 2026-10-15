# Native interface sprite authoring

The physical UI grids are 400x240 on the upper screen and 320x240 on the lower
screen. A native sprite is authored for its settled rectangle, then sampled with
nearest filtering at integer coordinates. Animations transform the complete
image, including its border, backing and shadow.

## Three independent texture modes

Display → Advanced → Textures cycles through Original, Scaled and Modified.
The saved values are `ui_textures=original`, `scaled` and `modified`.
Performance/Quality presets preserve this preference and all five HUD layouts.

- Original uses the archive artwork and the original drawing path.
- Scaled resamples the archive artwork with bilinear interpolation. RGB is
  interpolated with premultiplied alpha so invisible source colors do not bleed
  into the visible edge. It does not redraw letters, shapes or facial features.
- Modified uses hand-authored artwork where a replacement exists. An unfinished
  replacement falls back to the original; it must not silently use a different
  automatic reconstruction algorithm and describe that as hand-authored art.

The current hand-authored set contains eleven HUD canvases and sixty course-map
canvases. Character portraits, item windows, rank/place sprites and the general
menu alphabet are not yet a complete hand-authored replacement set. Scaled has
bilinear variants for those archive illustrations; Modified retains their
original appearance until suitable replacement art is available.

The port credit is shared by all three modes. It is composed at runtime from
the owner's actual copyright lettering plus stroke recipes for missing letters.
Its body, rounded descender, outline and shadow fit in a 166x18 native canvas.
Changing texture mode does not restore the older mismatched credit font.

## Reproducible port artwork

Run from the repository root:

```sh
python3 platform/3ds/assets/native-ui/generate.py
python3 platform/3ds/assets/native-ui/generate_maps.py --preview /tmp/mk64-maps
```

The HUD generator needs only Python's standard library; its optional `--preview`
needs Pillow. It authors LAP (40x10 and 48x12), TIME/TOTAL (32x16), three lap
counters (34x17 and 41x20) and twelve timer cells in a 96x16 strip. Pixel stencils,
contours, color ramps and shadow offsets are editable in the script. Its output
is `source/art/native_hud_art_3ds.hpp`.

The map generator requires Pillow, without a ROM or reference bitmap input.
Twenty independently drawn course/battle maps use M/L/C/Z path instructions,
stroke widths, filled road regions and explicit cutouts. Each is rasterized on a
fresh four-times coverage canvas and reduced directly to its final native size.
The union of brush discs fills short-segment joins, preventing tiny interior
pinholes. The original coordinate frame and transparent bearings are retained
for player and finish-marker registration. Intentional course jumps and battle
platform openings are separate geometry, not defects to fill automatically.

Each map fits three areas: 149x184, 110x140 and 68x176, preserving its frame's
aspect ratio. The output header `source/art/native_map_art_3ds.hpp` stores four-bit
coverage as bounded RLE; all sixty masks together occupy 126,284 encoded bytes.
`native_mask_3ds.hpp` rejects zero-length runs, overrun, underrun and invalid
sizes before upload. LA4 supplies white luminance and alpha in one byte per GPU
texel, so the same map supports white/blue vertex tinting without a black box.

Edit the generators and regenerate the headers. Review 1:1 and at an integer
nearest-neighbor zoom, including all three sizes. Check dark/bright backgrounds,
crossings, widths, narrow gaps, jump discontinuities, mirroring and markers.

## Private archive previews

```sh
python3 platform/3ds/assets/native-ui/preview.py \
  --archive /private/path/mk64.o2r --output /tmp/mk64-scaled-review
python3 platform/3ds/assets/native-ui/preview_credit.py \
  --archive /private/path/mk64.o2r --output /tmp/mk64-credit-review
```

These require Pillow and Clang with C++17 support. They compile the same portable
scaler/compositor used on the console and read an owner-provided archive. Output
must be outside the source checkout: PNGs, extracted pixels and inventory files
remain private. No source copyright or character bitmap is embedded in a public
header. The console reads its own archive; a PC-generated texture pack is not
needed at runtime.

The credit preview places the original Nintendo word and the new credit at the
same six-times zoom. Compare proportions, grayscale edge coverage, lowercase
baseline, y hook and complete drop shadow. Source letters are reused exactly;
missing letters are constructed, so a resemblance claim still needs visual
review. The canvas height is calculated from the glyphs and shadow margin rather
than clipped to the copyright image's fixed height.

## Ownership and validation

Variants are prepared only at resource load. Toggling modes selects existing GPU
images without file access, rasterization or allocation. The conveyor retains
one map size per style/course; only the active race map retains all three sizes.
Every allocation is retired and freed with its owning original texture. A
failed optional allocation leaves the original usable.

`native_sprite_test.cpp` checks exact bilinear color/alpha interpolation,
transparent gaps, source-letter identity with synthetic pixels, complete y and
shadow bounds, all sixty mask streams and malformed RLE. UI integration tests
exercise three-mode cycling in both directions and Advanced touch/back routes.
Configuration tests cover persisted Scaled/Original values, invalid input and
preset isolation. These tests do not replace visible console/emulator review.
