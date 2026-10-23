# Experimental native mesh interface

`native_mesh_format_3ds.hpp` defines a versioned little-endian native cache:
`NCM1`, vertex count, index count, stride (four 32-bit fields), followed by
16-byte vertices and 16-bit triangle indices. A vertex contains signed integer
XYZ/UV, four attribute bytes and two reserved bytes. Material plans determine
whether the four bytes represent shaded RGBA or normals.

`ValidateMesh` borrows its input, clears output on failure, and checks counts,
size/stride/version, every index and exact file length before GPU allocation.
It accepts unaligned file input; callers copy validated payloads to aligned
linear memory and clean the cache before submitting.

`native_mesh_draw_3ds.cpp` binds the native short/short/byte attribute layout
and submits indexed PICA triangles. `BindMesh` plus `DrawIndices` permits many
material ranges to share one persistent buffer. The caller owns the validated
ranges, camera, shader, materials, textures, frame and completion boundary.
Destroy GPU resources only after their submitted work has completed; VBlank
alone does not provide that guarantee.

This interface is used by separate development experiments. An opt-in Luigi-only development build now integrates static native course
views with a temporary legacy-actor state bridge. The default game route
has not yet integrated the complete native pipeline, and this module does not establish complete native
scene coverage, visual parity or measured physical-console performance.

`native_scene_format_3ds.hpp` adds the startup-validated NLC3/NLC4/NLC5 native scene
tables: texture metadata, typed materials, indexed ranges, view selections and
shared range references. It checks cross-mesh limits before the adapter copies
typed runtime records. These records contain native parameters, not GBI words.

`native_material_3ds.cpp` applies the initial shaded/modulated techniques,
samplers, UV transforms, culling and cutout policy through a caller-owned state
cache. Reset it when another renderer changes GPU state. NLC3/NLC4 inherit
caller depth. The caller owns the shader/frame, texture lifetimes and the GPU
completion boundary in every version.

NLC5 retains the texture/range/view/reference tables and interleaves each
48-byte material with an 8-byte `RasterState` (56-byte record stride). It adds
explicit depth test/write/decal, alpha test/reference, blending, point/linear
filtering and alpha-output choices. Alpha output is separate from alpha test:
an inherited threshold does not itself enable the source shader's alpha
calculation. The legacy material `cutout` field must be zero in NLC5.
NLC3/NLC4 retain their 48-byte stride, inherited depth and existing behavior.

The NLC5 material set additionally covers RGB modulation with shade alpha,
unmodulated texture RGB with texture/shade alpha, and texture-alpha decal
interpolation. These are fixed native techniques, not a runtime combiner decoder.
Seven native techniques represent the supported single-effective-operation
equations. Source-cycle simplification belongs to the offline converter and
must be proven before selecting a technique. Fog, lighting, palette/format
changes and caller-state requirements need their own validated contracts;
successfully loading a native scene does not establish those contracts.

`GpuScene` copies validated interleaved records into owned runtime arrays before
drawing. A raster-owned scope must restore its caller's GPU state on exit.
Adjacent batching must compare the complete material/raster identity, not only
the legacy 48-byte prefix. The asset-free `native_material_test` checks SDK
calls, independent source equations and cache transitions; it is not a PICA
pixel test or console performance measurement.

`native_camera_3ds.hpp` prepares a conventional row-vector camera matrix for
PICA before upload. Its depth row avoids subtracting nearly equal clip Z/W on
float24, and its X/Y rows account for the physical top framebuffer. This is
once-per-camera work, separate from game-specific camera construction. Limited
development camera comparisons do not establish every clipping/depth case.


`native_gpu_scene_3ds.cpp` owns startup-validated immutable GPU meshes, textures,
materials and a native shader. Its Draw method allocates no resource objects.
The host must fence submitted work before destroying a loaded scene. The
`MK64_3DS_NATIVE_LUIGI_LAB` option defaults OFF and requires externally prepared
private resources. Only that test build compiles the course adapter, direct
Luigi AUTO boot and read-only save guards. It deliberately retains legacy
actors/UI during migration and is not the final native renderer.

The migration-only `native_exit_state_3ds.hpp` validates typed final state for
that boundary. It contains no source command stream. The backend handoff keeps
the current render target and restores legacy GPU state after a native draw;
using the ordinary external UI presentation helper here would end the scene
too early. Course-specific recognition, save policy and test boot remain in the
game adapter rather than the reusable GPU scene module.


`native_gpu_sprites_3ds.cpp` adds a compiled development sprite component with
fixed vertex/index storage and a bounded cache of persistent 64x64 GPU textures.
At 400 pixels either model may allocate up to 128 slots (2 MiB total), with
6 MiB New / 4 MiB Old linear reserves. Wide retains 32 slots. Extra allocation failure
falls back to the successfully allocated capacity once at least 32 exist. It consumes
pre-tiled pixels, validates payloads through a supplied decoder, and protects
textures referenced by the current submission. `BeginAfterCompletion` requires
an actual caller-owned GPU completion fence, never just VBlank or a CPU cache
flush. Limited Luigi development integration covers ordering and interpolation;
this does not establish complete character/effect/mode parity.

`native_sprite_pack_3ds.hpp` supports the MK64-specific NSP1 independent-image
and NSP2 wheel-page adapters, including palette-aware Original and Improved
variants. NSP2 stores four CRC-tagged images as one base and three lossless XOR
deltas in a checked compressed page. Four decoded pages are cached on CPU;
GPU residency remains separately completion-bound. A selected-mode compressed
range may be loaded before race construction with an 8 MiB ordinary-heap
reserve. Allocation/budget failure retains validated streaming; resource
pressure may reclaim resident bytes without invalidating uploaded GPU images.
Mode changes are best-effort loading boundaries and may fall back to streaming
when the current resource footprint leaves insufficient reserve. Neither pack
is a generic N64 container. Converted owner data/tools stay outside source.


The later `MK64_3DS_NATIVE_KART_LAB` option integrates native kart descriptors
in the Luigi development build. It defaults OFF and requires the private native
sprite pack and pre-tiled ordinary/credits shadow images. The generated game
adapter replaces body, ghost, trail, reflection and both shadow command tails;
a bounded address-anchoring bridge preserves ordering alongside remaining
legacy objects. Capture resets at graphics-buffer generation, not merely at the
outer logic tick, because race initialization can discard an earlier assembly.

The backend opens native sprite reuse only after its real GPU-completion
acquisition. Adjacent descriptors at the same migration anchor share a native
scope, retaining FIFO order without sorting across legacy draws. Each scope
binds the program and stable vertex base once; each submission owns appended
rebased indices and vertices until completion. The scope restores legacy state
at its end and on exceptions. Immutable startup-loaded textures have a separate
key namespace and never evict character slots. The generic material supports
one/two quads, kart colorization and constant RGB with texture-modulated alpha.
A temporary lazy
texture-ownership adapter handles any inherited legacy texture consumer. Limited
race integration does not establish all Original/effect/mirror/multiplayer cases,
transition safety, full native-only coverage or physical performance. The final
native route must remove the anchoring and legacy compatibility adapters.

`native_character_contract_3ds.hpp` keeps MK64 palette-write/crop semantics out
of the generic sprite material. Ghosts/body draws load palettes; reflections and
trails inherit them, with an explicit Original-key validation. Blended edges
normalize to regular alpha thresholds; opaque edges remain cutouts. Credits
shadows use an edge-padded 64x32 crop with their
own immutable key, alpha and source-byte/bounds contract.

The separately OFF-by-default `MK64_3DS_NATIVE_CHARACTER_SCENARIOS` option forces
character render cases with Original sprite resources in the Luigi test. It
restores temporary player flags and leaves saved/global UI settings unchanged.
This is a stress fixture, not normal race behavior, mode acceptance or a physical
performance result. Never enable it in ordinary builds.


NLC4 retains NLC3's table layout and extends the legacy-named `cutout` field
with `MaterialAlpha`: opaque, cutout, threshold blending and ordinary blending.
NLC3 rejects the two new values. The backend preserves cutout reference 48,
blended threshold reference 8, and alpha blend factors. Caller-owned depth,
viewport and scissor remain separate. `GpuScene::BeginBatch` resets cached
bindings; `Draw` defaults to standalone invalidation unless an explicit native
scope owns the GPU state. Changing point/linear filtering invalidates the sampler
cache. Native scene and sprite scopes cannot overlap or silently change owner.

`MK64_3DS_NATIVE_VEGETATION_LAB` defaults OFF and requires private converted
resources. It replaces eleven vegetation command tails and the shared shadow
with native model/view descriptors. A shared bounded `NativeDrawReference`
queue preserves character/vegetation ordering beside remaining legacy paths.
Typed migration exits retain source slots/state for those paths; they are not
part of final native-only completion. Its root IDs, compatibility owner names,
conversion assets and MK64 actor rules do not belong in the reusable renderer.
The ordinary build resets all native/forced fixture options to OFF.


`MK64_3DS_NATIVE_ITEM_BOX_LAB` extends that private object development path to
complete item-box states and fragments. It defaults OFF and requires the native
object integration and approved private texture pack. Primary-color materials
use a harmless white backing; texture selection retains both approved logical
Original/Improved variants. Source owners are prepared once with explicit leases
and replacement-buffer ownership. HD compatibility exits preserve scaled byte
counts and resource-height stride. The mixed queue also accepts typed caller-
state deltas without copying source GBI command words. Root selection retains
entry culling separately from restored exit culling. Complete native-only and
full visual/hardware acceptance remain unproved.

`MK64_3DS_NATIVE_PROJECTILE_LAB` extends the same development path with upright
and flat bananas, fake boxes and every green/red/blue shell frame and mirrored
orientation. It defaults OFF. The shared scene accepts one startup-owned texture
variant map; switching the approved Original/Improved images does not duplicate
geometry or allocate during drawing. The game adapter retains original resource
leases and replacement buffers. Palette conversion explicitly preserves the
signed N64 source behavior across host byte orders. Visibility exits retain the
production interpolation boundary, and shell palette state keeps its position
in the mixed queue even when the actor is culled.

For an executable development candidate, build the `mk64-3ds-game-3dsx` target.
Building only `mk64-3ds-game` updates the ELF without necessarily regenerating a
previous 3DSX. Verify the packaged executable against the exact ELF and resource
inputs before launching or reporting matching RAM evidence. A matching version
string or recent ELF timestamp does not establish package identity.

## Conservative native visibility

`native_visibility_3ds.hpp` prepares compact integer bounds from validated,
immutable native positions and index ranges at scene load. Ranges larger than
128 triangles also receive block bounds. Metadata has a 128 KiB ceiling; an
allocation or planning failure preserves full-range submission. Drawing allocates
no metadata. Changing the complete camera matrix invalidates cached range
classification; replacing the scene rebuilds the plan.

The classifier uses the native shader's PICA clip space, including depth
`[-w, 0]`, with outward slack for float24 arithmetic and cancellation. Uncertain
or nonfinite bounds remain visible. Up to four ordered index spans retain every
potentially visible triangle; merging gaps may draw extra invisible triangles.
This does not change game activation, distance settings, textures or simulation.
The game adapter still applies each root's typed exit state after a clipped draw.

Visibility precedes native shader/material setup. `SetVisibilityEnabled(false)`
retains the full-range path for matched diagnostics. Diagnostics export lifetime
range tests, reuse, rejected ranges/triangles, block tests and additional draws.
Host tests establish conservative selection and ordered SDK submission. Pixel
and physical performance acceptance remain separate checks.

Native matrix uniforms are cached only within an explicitly owned native batch.
PICA uniform registers are shared by shader programs: viewport changes inside a
native scene/sprite batch update viewport/scissor but defer Fast3D projection
uploads until `RestoreFast3DState` runs after batch exit. New batches invalidate
the matrix cache even when camera bytes are identical.

All native sprite batches now publish their written vertex span on batch exit,
including the exception path, before any command queue submission or split.
Indices remain immutable after their initialization clean. Queue completion
still governs reuse of vertices and texture slots. A failed vertex publication
retains the pending dirty span and rejects the batch.

## Development timing

The native dynamic sprite path publishes immutable indices at initialization
and dirty vertex spans at batch exit through the platform cache helper,
retaining the service fallback.
The GPU-completion fence still controls buffer and texture-slot reuse. Avoid a
clock syscall on every migration-queue lookup: most addresses have no native
packets. Start block timing only after the first matching descriptor.

Lab counters remain in bounded RAM during gameplay. Export after the measured
scene instead of performing synchronous filesystem writes at a fixed interval.
An export before the per-frame timer still consumes wall-clock frame budget.

`MK64_3DS_NATIVE_CACHE_LAB` and `MK64_3DS_NATIVE_COMPARE_LAB` are OFF by default.
The cache probe alternates equal owned-buffer ranges between service and direct
cleaning. The comparison temporarily replays one game frame with native versus
interpreted static-course dispatch, retaining typed characters and objects in
both paths. It restores owned RSP/RDP snapshots, warms both paths, alternates
order, checks input identity and records GPU/display waits separately from CPU
recording. Its repeated submissions are not gameplay FPS or whole-engine parity.
Normal builds explicitly disable both options and require no private lab data.

The gameplay performance trace v7 separates native kart file read time/bytes,
decompression time and residency misses from the legacy O2R archive counters.
Zero archive reads do not establish zero native image I/O. These counters are
RAM-only during the measured frame and are exported with diagnostics.

Decoded character pages start with four slots. Improved mode may grow this CPU
cache to eight or sixteen slots at a loading boundary when the existing heap
budget permits it. Original and Menu retain four. The policy preserves room for
the complete compressed range first, deducts the extra decoded-page allocation
from that range's replacement allowance, and retains the ordinary 8 MiB planning
reserve. Failed growth keeps the valid smaller cache. This adds at most 786,624
bytes above the former four-page allocation and does not change GPU residency,
texture bytes, shader state, command order or completion fences.

A 4,104-byte RAM history records the latest 1,024 character page requests, active
CPU capacity and decode decisions. It permits exact request-sequence replay at
different capacities without file I/O or clock sampling in a draw. Repeated
captures can contain overlapping history; their totals must not be summed as
independent requests. Cache replay validates all four decoded RGBA images against
the native pack CRCs. Decode-count savings are not whole-game FPS gains.

Citro3D starts its CPU recording timer after acquiring the GPU queue. The port
then waits for pending framebuffer swaps before reusing double-buffered output;
that later wait is inside the SDK timer. The adaptive presentation gate uses
the SDK duration minus precisely that pending-swap wait. Never subtract the
whole FrameBegin wait: its earlier queue/VBlank portion is already excluded.
Trace `present_wait_us` identifies this subset of `begin_wait_us`; v5's CPU
columns included it despite the older trace description. Preserve both queue
and framebuffer ownership fences when correcting timing.

## Mutable images

Preconverted geometry does not imply immutable pixels. `NMD1` descriptors list
bounded texture IDs, dimensions, source stride and resource names separately
from scene geometry/materials. A startup owner retains the source leases.
`GpuScene::BeginAfterCompletion` opens the permitted update boundary; any draw
closes it. `UpdateTextureRgba5551` validates ranges, converts directly into the
existing tiled GPU allocation and cleans that exact range before submission.
It allocates no memory during update and changes no material or GPU address.

The current development adapter publishes a generation only after all six
producer images are complete. It validates source identity, reuses an unchanged
generation across multiple presentations, and keeps a bounded exact source copy
to avoid identical-tile uploads. Failed transfers do not publish the completed
generation. Ownership must outlive all dependent draws and the completion fence.
An ordinary static-image hash check is insufficient for these mutable resources;
verification must compare the actual committed version and its pinned source.


Old targets the original 30 Hz image cadence without the optional midpoint
recorder. Its audio uses serialized core-0 synthesis; New keeps its core-2
worker. These policies do not establish physical frame rates. Manual native
playtests never export lab files at tick 900; bounded automatic lab runs retain
that export outside their measured window. Runtime/core/engine archives form
one link group so native function reachability does not depend on statistics
exports. Trace v7 records resident bytes and page decode counts in addition to
native file I/O. Native GPU image misses publish the exact 16 KiB allocation
through the existing direct-cache helper and retain its service fallback.
# Lossless fast character pages

An optional `native-karts.nsp.fast` companion uses the NSP3 header/version with
the same 168-byte entry identities, lane CRCs and fixed 64x64 GPU images as NSP2.
Each payload contains a CRC32 followed by an independent LZ4 block. The expanded
65,552-byte page stores its 16-byte lane-CRC header unchanged, followed by four
byte-channel planes of the original base/XOR-delta image bytes. Decoding restores
interleaved bytes and validates the original full-page CRC before publication.
The runtime decoder is bounded and independently implemented; no compression
library, allocation, or image resize occurs while drawing. Missing companions
retain NSP2 support; an invalid present companion fails validation explicitly.
The block representation follows the [LZ4 format specification](https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md).

Compressed selected-mode residency can use up to four immutable 1 MiB VRAM
chunks after the ordinary-heap allowance is exhausted, retaining a 1.5 MiB VRAM
planning reserve. These allocations never alias GPU-owned textures. Loading uses
aligned staging and word stores; arbitrary compressed-byte reads use aligned
word loads. Cross-chunk reads, partial allocation rollback and truncated files
are covered by host regressions. Residency replacement keeps GPU completion
ownership and accounts only the ordinary-heap portion against the heap budget.

Normal track loading now enters the existing serialized loading-audio service.
Nested credits/track scopes release the service only at the outer boundary;
native scene reads service it between bounded 64 KiB reads. This extends queue
maintenance during loading without moving game audio state onto another core.
Physical frame cadence and audible behavior still require console measurement.
