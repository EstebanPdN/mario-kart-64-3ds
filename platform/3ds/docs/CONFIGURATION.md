# Configuration reference — 2.0

Options are saved in `sd:/3ds/MK64/mk64-3ds.cfg`. Menu changes use the same
validated settings implementation as loading that file. The writer uses a
temporary file and recoverable replacement; existing game saves and the O2R are
separate. If the main file is missing, a complete `.bak` is loaded. See
[recovery and compatibility](COMPATIBILITY.md#settings-replacement-and-recovery).

## Display

| Row | Values and purpose |
| --- | --- |
| Profile | Performance, Quality, Custom. Applying a preset restores its display values. |
| Aspect ratio | Wide or Original. |
| HUD layout | Clean, MK7, MKDS, MKDS 2, Classic; Y cycles the complete presets in a manual race. |
| Texture | Original, Improved, Menu. Menu improves UI/HUD/maps and selection portraits while retaining original race drivers/world artwork. |
| Render | 50–100% internal scale, in 5% steps; touch or left/right adjusts the rail. |
| Distance | Low, Normal, High. |
| Frame limiter | 30 FPS, 60 FPS or Off presentation target. The original simulation remains 30 Hz. |
| Advanced | Opens the remaining display settings. |

Advanced contains Filter (Bilinear/Blur/CRT), Show loading screens, HUD meters,
Menu blur (Off/Light/Medium/Strong), Scrolling maps, Warning widget and Back to
Display. B returns to the Advanced row of the main page. HUD meters may be
unavailable for a layout; the UI marks that state rather than silently changing
the setting. LIGHT retains the original background detail; stronger blur uses
the cached treatment. Options use common row geometry for drawing and touch. Each option has an
original square pixel icon with equal padding. Rows are contiguous and the
selected plate follows the native greyscale blink, with cyan/magenta lettering.

| Preset | Both models | New 3DS | Old 3DS |
| --- | --- | --- | --- |
| Performance | 100% scale, Bilinear, MKDS 2, Light blur, static maps | Low distance, 30 FPS target | Low distance, 30 FPS target |
| Quality | 100% scale, Bilinear, MKDS 2, Light blur, scrolling maps | High distance, 60 FPS target | Normal distance, 30 FPS target |

Manual display edits select Custom and preserve other current values. Gameplay,
volume, SELECT bindings and the texture style survive preset changes.
Texture style is a visual preference independent of the Performance/Quality profile. Older settings without a
profile infer a matching preset when their display values match it; otherwise
they use Custom. A saved profile inconsistent with its individual
values also becomes Custom; the label must describe the active configuration.

The 60 FPS choice permits an additional presentation when CPU/GPU/audio/resource
headroom allows it. It is not a fixed-rate guarantee or a 60 Hz physics mode.

## Gameplay and Developer

Gameplay contains the 2x–5x Turbo speed rail, master volume and Modern Logic.
C-Stick holds Turbo; release returns to normal speed. Modern Logic controls the
additional item/CPU rules and held-B reverse. ZL/ZR have no game binding.

Developer contains Memory dump, Show FPS, Select for dump, Select for overlay and
Clean dumps. SELECT's dump/overlay settings are exclusive; with both off SELECT
uses the manual-race camera action. Dump captures are disabled while AUTO is active.
The expanded L+R+SELECT chord takes precedence in manual play, including menus.
Manual dumps include a six-MiB VRAM snapshot before progress rendering.
Overlay is temporary live state and is not restored as an open modal on boot.

Pause contains Continue game, Quit and Update in the same typography and row
style as the other options. Continue preserves held acceleration. The updater
is also reachable from the normal Game tab.

## Saved keys and compatibility

The file is line-oriented `key=value` text. Keys are case-sensitive; enum values
and named booleans are case-insensitive. Full comment lines begin with `#` or
`;`. Boolean values accept `on/off`, `true/false` and `1/0`. Unknown keys,
invalid enum/boolean values and malformed integers are ignored; the previous
valid value or initial model default remains. Later valid duplicate keys win.
SELECT's two exclusive bindings resolve in file order: the last enabled action
wins, and saving writes one normalized exclusive pair.

| Saved key | Accepted values | Initial/reset default |
| --- | --- | --- |
| `profile` | `performance`, `quality`, `custom` | `custom`; rechecked against individual display values after loading |
| `aspect_ratio` | `wide`, `original` | `wide` |
| `hud_layout` | `clean`, `mk7`, `mkds`, `mkds2`, `classic` | `mkds2` |
| `resolution` | Normalized to 400/800, midpoint 600 | 400; effective output still respects hardware support |
| `render_scale` | 50–100, normalized in 5% steps | 100 |
| `display_filter` | `bilinear`, `blur`, `crt` | `bilinear` (Normal in the menu) |
| `render_distance` | `low`, `normal`, `high` | Old: `low`; New: `normal` |
| `frame_limit` | `30`, `60`, `off` | Old: `30`; New: `60` |
| `hud_meters` | `off`, `distance`, `speed`, `position_lap` | `off` |
| `menu_blur` | Integer clamped to 0–3 (Off/Light/Medium/Strong) | 1 |
| `menu_maps` | Boolean | on |
| `threat_warnings` | Boolean | on |
| `ui_textures` | `original`, `improved`, `menu` | `improved` |
| `show_loading_screens` | Boolean | off |
| `turbo_speed` | Integer clamped to 2–5 | 2 |
| `master_volume` | Nearest supported 25, 50, 75, 100, 150 or 200; ties use lower | 100 |
| `new_items` | Boolean; Modern Logic | on |
| `show_fps` | Boolean | off |
| `select_for_dump` | Boolean, exclusive with overlay | off |
| `select_for_overlay` | Boolean, exclusive with dump | off |

Existing saved values take precedence over model defaults. Display presets
change their own display values, while preserving gameplay, volume, SELECT and
texture choice. Settings take effect through the normal frame/UI boundary;
changing a texture style selects prepared images without a restart.

Legacy HUD names `focus/race/hybrid` map to MK7/MKDS/MKDS 2. `top_hud` is accepted
only as a fallback when a modern layout key is absent. Legacy `overlay`,
`zr_debug` and `logging` keys do not restore modal state, assign ZR or disable
diagnostics, and are not written back. Saving normalizes supported keys; it is
not a lossless editor for arbitrary comments or unknown preferences.

For example, this complete small override keeps the other model defaults:

```ini
profile=custom
hud_layout=mkds2
render_scale=100
frame_limit=30
ui_textures=improved
select_for_dump=off
select_for_overlay=on
```

`ui_textures=unknown` leaves the current/default art mode unchanged.
`turbo_speed=99` clamps to 5. `select_for_dump=on` followed by
`select_for_overlay=on` resolves to overlay and is saved as dump off/overlay on.
The host settings fixtures cover these compatibility categories. Edit the
file while the game is closed, preserving the main and recovery copies first.

`ui_textures=original` uses the archive artwork. `ui_textures=improved` selects
optional prepared art for supported UI, maps and characters. Missing or invalid
replacements retain authentic source artwork. `ui_textures=menu` improves the
interface, including race HUD/maps and selection portraits, while keeping race
drivers and world effects original. The Texture row uses the former Render
icon; Render uses the same chip as Memory dump. Legacy `scaled` and `modified`
values both normalize to `improved`. The approved options dialog, five HUD
layouts and controls keep their design. See [native artwork](NATIVE_SPRITES.md)
for preparation, fallback and credits.

Frame Limiter cycles through 30 FPS, 60 FPS and Off. Off removes presentation
synchronization; GPU completion fences and the original fixed 30 Hz simulation
remain active. The 60 FPS choice uses existing adaptive intermediate
presentation when supported. Neither choice guarantees a sustained rate or
changes physics speed. Physical display refresh is still a hardware limit.

Legacy `frame_limiter=on` migrates to 30 and `frame_limiter=off` migrates to 60,
matching the old meaning. A valid modern `frame_limit` key takes precedence
regardless of position relative to a legacy key. Invalid values are ignored.
Saving writes the explicit modern choice.

### Presentation FPS display

The top counter uses a completed one-second presentation window, updates once
per second and uses the lime HUD time color. The Developer overlay shows that
average and the last image-interval rate separately. Skipped simulation ticks
are excluded; successful interpolation images are included. These counters
measure submitted game images, not the original 30 Hz simulation or an emulator
host speed indicator. Loading/dump pauses reset the history. Existing diagnostic
log summaries retain their separately labelled 2-second/10-second windows.
