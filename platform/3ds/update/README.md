# Built-in updater

Options > Game places Update below Close Options. During a race, pause first:
Continue Game, Update and Quit remain separate actions. The updater retains the
current course/menu backdrop below and the current rendered scene above, using
the native menu font. The upper screen shows
release notes on every presentation, including interpolated images, with side
L/R controls, centered page numbering and bundled notes available offline.
The updater keeps RomFS mounted for its certificate and offline notes until
its worker has finished during shutdown.

Opening Update checks the public releases of `EstebanPdN/mario-kart-64-3ds`.
Stable and Experimental channels are stored in `sdmc:/3ds/MK64/update/channel.txt`.
Use the D-pad and A, or touch. B/Start returns or cancels a transfer. Installation
requires explicit confirmation with No selected initially, and closes the game.
Save progress first. Cancellation, HOME and sleep are disabled while installing.

The updater validates HTTPS certificates, exact repository/tag/asset URLs, size
and SHA-256. CIA packages also require title ID `0004000005a27000` before AM
installation. AM overwrites the installed title without deleting it first.
Complete 3DSX packages replace the actual launch path through a backup and rename.
An unknown launch path or existing backup stops replacement. ROM/O2R data, saves,
settings and diagnostic files are outside the update package and remain intact.
Temporary downloads are removed; errors are written to `sdmc:/3ds/MK64/update.log`.

Supported tags include `v1.6`, `v1.6.1` and `v1.6-E1`. Versions compare numerically;
a stable version sorts after experiments with the same base version. The highest
version in the selected channel must provide exactly one matching
`mk64-3ds-vVERSION.cia` or `mk64-3ds-vVERSION.3dsx` asset with GitHub's SHA-256
digest. Equal/older versions cannot be installed. Drafts are ignored. Metadata
is bounded to 100 releases / 2 MiB, downloads to 128 MiB. An invalid newest release
is an error instead of silently selecting an older one.

The updater is adapted from EstebanPdN/legend-of-doom-3ds at `6d73990`, including
its SSL service initialization, worker priority and buffered SD transfer fixes.
These updater sources are GPL-3.0-or-later; see COPYING. Dependency source hashes,
3DS patches and license notices are in `../update-dependencies/`. No Doom artwork,
game data or font is included.

Host verification on macOS:

```sh
python3 platform/3ds/tests/run-update-tests.py --jansson-prefix /path/to/native/jansson --live-check
```

Tests use ASan/UBSan and real host download/hash operations with mocked AM calls.
They do not establish physical-console installation, networking or visual results.
