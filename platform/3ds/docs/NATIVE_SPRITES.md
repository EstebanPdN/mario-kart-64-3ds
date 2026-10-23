# Native interface artwork and optional improved textures

The upper screen uses a 400x240 logical canvas and the lower screen uses
320x240. Improved UI images are prepared for their settled rectangles, uploaded
with nearest sampling and drawn at integer coordinates. Transitions transform
the complete sprite, including its lettering, border and backing.

## Original and Improved

Display > Advanced > Textures has two choices: Original and Improved. Saving
writes `ui_textures=original` or `ui_textures=improved`. Legacy `scaled` and
`modified` values migrate to Improved. Display presets preserve the texture
choice and all five HUD layouts.

Original selects the owner-provided archive artwork. Improved selects an
optional native-size replacement. Missing or invalid replacements retain the
original. Lower-screen UI can prepare a premultiplied-alpha bilinear fallback
from that authentic source; it does not use the rejected hand-traced maps or
claim that fallback is newly authored HD artwork.

Replacement roles include menu and HUD art, minimaps, portraits, items, rank
markers and driver/kart frames. Scenery and environment textures are excluded.
Driver frames preserve their original canvas and registration. The approved
options dialog retains its original font and layout. LAP and its lap counter
use matching 17-pixel lower-screen rectangles.

## Private preparation and provenance

An optional external artwork source is MK64 Reloaded, maintained by GhostlyDark
and its credited contributors: [repository](https://github.com/GhostlyDark/MK64-Reloaded)
and [pack information](https://evilgames.eu/texture-packs/mk64-reloaded.htm).
The local reviewed input is the SpaghettiKart HD pack from v2026.04.03.
Downloaded images, owner archives, generated packs and derived pixels remain
outside the public source checkout.

Local preparation records input/output hashes, resource names, dimensions,
replacement roles and credits. Images are reduced with premultiplied-alpha
Lanczos sampling directly to their required grids. Wario Stadium uses the
owner-approved reference instead of the rejected Reloaded drawing. Its original
64x64 logical canvas and visible bearings are retained. The other nineteen
reviewed maps are unchanged. Marker registration still needs an in-game check.

The animated character-selection portraits use an opaque original material
(`D_02007708`). Their replacement RGBA pixels must be composited over the
original black canvas during preparation, after downsampling, with output alpha
255. Simply ignoring the alpha of a straight-RGBA PNG exposes bright RGB in
near-transparent edge pixels. Use `(channel * alpha + 127) / 255` for the black
matte; do not threshold away antialiased edges or add a second portrait draw.
This rule is specific to those portrait canvases, not driver sprites, maps,
items or the selection overlay, which retain their own transparency contracts.

The custom OPTION, SELECT RANDOM CUP and SELECT AUTO lettering was generated
with the built-in image generator, then composed on an 8x grid and reduced
once. OPTION uses the heavier DATA/RANDOM letterforms while preserving its
original L keycap and frame. The two SELECT hints share the same 66x29 keycap
and 55x20 white SELECT lettering. Complete sprites are 87x29, 192x29 and 122x29
respectively. DATA retains its approved 87x29 replacement. Prompts, HD inputs and native previews
remain private. A generated image is not itself evidence of in-game acceptance.

To include an already prepared local pack, configure the normal 3DS build with
`-DMK64_3DS_IMPROVED_TEXTURE_PACK=/private/path/improved-textures.bin`.
CMake copies it into that build's RomFS. Without that option, the runtime can
also read `sdmc:/3ds/MK64/improved-textures.bin`. Setting the CMake value to an empty string
removes a stale generated RomFS copy. The owner archive is required separately.

## Format and ownership

`include/improved_texture_pack_3ds.hpp` reads a 16-byte little-endian MUI3 header:
version 1, record count and record size 32. Records are sorted by the 64-bit
FNV-1a resource-name hash; the `__OTR__` prefix is normalized before hashing.
Each record stores hash, offset, compressed length, decoded CRC32, pixel width
and height, logical width and height, and a reserved word. Each payload is a
zlib-compressed RGBA8888 image. The native build reuses existing miniz code.

Dimensions must be nonzero and at most 512. File bounds, index ordering, decoded
size and CRC are validated before publishing pixels. Limits are 20,000 index
records, a 512 MiB file and a 2 MiB decoded-resource FIFO. Active shared borrowers
can use additional memory beyond that retention limit. Eviction or pack closure
does not invalidate those borrowers. A final-release callback invalidates renderer
cache keys before a CPU buffer address can be reused. GPU variants retire with their parent
texture through the existing GPU completion path. The updater mounts RomFS after extraction and before any native artwork lookup;
shutdown closes the pack before the updater unmounts RomFS.

UI variants are prepared at resource load, not in draw calls. The conveyor
retains one dedicated reduced map size per course, prepared directly from the
approved master rather than nearest-resizing the race variant. Its area is
80% of 149x184 and its center is 10% of screen height above the original center.
The active race map can retain three independent sizes. General
Fast3D replacements keep original canvas dimensions and convert original tile
byte strides to RGBA32. A main-thread policy supplied by the game selects the
mode; independent interpreter probes do not depend on the settings subsystem.

The resource manager keeps 1,024 bounded Improved lookup slots (at most 24 KiB
on the 32-bit target). Each holds a weak wrapper and immutable width, height and
format metadata. Slots use the normalized name hash and verify the full path;
collisions are misses and Original mode bypasses them. They retain no decoded
image allocation. Archive generation is checked before dereferencing a borrowed
canonical name, including after shutdown/reopen. A live hit avoids Original
acquisition, pack lookup and wrapper allocation. When only the wrapper expires,
valid metadata can recreate it from the replacement pack without decoding
Original pixels again. Missing replacements retain the Original fallback.
The optional pack is immutable for the process lifetime; replacing it requires
a restart.

Renderer source-address invalidation can re-enter the texture cache from a
resource deleter. Recycling must detach all old ownership identities before
the last shared resource is destroyed. Whole-cache clearing removes map/LRU
nodes before releasing their owners; otherwise the callback can invalidate the
active iterator and return the same GPU ID to the free list twice.

## Validation boundaries

Standings portraits rotate their black backing, face, border and rank around
the portrait center. The backing follows the same native-variant size and
center rounding as the face, and shares its fade alpha. The zero-angle path
keeps a rectangle; rotated states use two triangles at the backing depth.
The asset-free regression compares the submitted backing corners against the
actual portrait transform across 504 Original/Improved size, angle and fade
cases. A real collision still requires visual review on the target device.

Host tests cover malformed indices, truncated streams, CRC failures, missing
packs, hash normalization and shared ownership after closure. Settings/options
tests cover two-choice navigation and legacy migration. These tests do not
validate PICA pixels or physical console performance.

Renderer host regressions also exercise the actual patched cache functions
through 1,000 reentrant recycle/hit operations and scene clearing, with unique
GPU IDs and balanced ownership. The production resource-manager fixture checks
128 hot Improved lookups, name collisions, Original/Improved changes, weak
expiry and missing/out-of-memory behavior under address/undefined sanitizers.

Review the actual built game at 1:1 and integer inspection zoom, on light/dark
backgrounds, through intermediate transitions and mirror mode. Check letters,
map crossings, marker registration, item frames and portraits. Compilation,
asset decoding, emulator rendering and physical Old/New measurements are
separate evidence.

The older asset-free `assets/native-ui/generate.py` and `generate_maps.py`
fixtures are historical authoring/test tools. Their HUD/map set is no longer
the active Improved replacement source. Archive previews are inspection tools,
not a claim that every final sprite has been hand authored.


### Shared native kart storage

A MUI3 record with reserved flag bit 0 set borrows its compressed image from
`native-karts.nsp` alongside the MUI file. The companion header, payload bounds,
64x64 physical/logical dimensions and per-payload CRC are checked. Unknown flags
are rejected. NSP1 uses independent images. NSP2 aliases use flags `2 | (lane << 8)` for
lanes 0..3 and reference a CRC-checked four-wheel page. The shared page codec
reconstructs the exact selected pre-tiled image; its tagged image CRC must
match the alias. Unknown lane/flag combinations and companion-version
mismatches are rejected. Storage representation may change; all Original
palettes, wheel states, tumble frames and Improved pixels remain available.

The resource wrapper marks this image with `kNativeGpuTextureFlag`. The 3DS
Raw importer then uploads existing ABGR8 Morton tiles directly instead of
passing them through the linear-RGBA converter. Full images and aligned vertical
strips use memcpy. Other legal tile regions retain the same byte mapping,
padding and transparency. General UI requests can read logical pixels at load
time; native kart drawing retains its existing pack/residency path.

Native scene/sprite batches explicitly disable legacy secondary, fog and
procedural samplers before using a shader that exports only texcoord0. Leaving
a native batch restores selected legacy samplers and clears unused units.
