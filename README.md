<h1 align="center">Mario Kart 64 3DS</h1>

<p align="center">
  <strong>Pick your kart. Bring the race with you.</strong><br>
  Mario Kart 64, adapted for Nintendo 3DS.
</p>

<p align="center">
  <a href="#download-and-installation">Download</a> ·
  <a href="#features">Features</a> ·
  <a href="#documentation">Documentation</a> ·
  <a href="CHANGELOG.md">Changelog</a> ·
  <a href="CONTRIBUTING.md">Contribute</a> ·
  <a href="#community-and-bug-reports">Community</a>
</p>

<p align="center">
  <a href="#download-and-installation"><img src="docs/badges/platform.svg" alt="Nintendo 3DS" height="22"></a>
  <a href="platform/3ds/update/README.md"><img src="docs/badges/updater.svg" alt="Updates: built in" height="22"></a>
  <a href="#license-and-legal-notice"><img src="docs/badges/license.svg" alt="Component-specific licenses" height="22"></a>
</p>

<p align="center">
  <img width="1200" alt="Mario Kart 64 running on Nintendo 3DS" src="docs/images/showcase-rounded.webp" />
</p>

<p align="center">
  <a href="https://estebanpdn.github.io/"><img src="docs/images/website.svg" alt="Visit Esteban’s Forge" width="142" height="46"></a>
  <a href="https://discord.gg/BjbzfRYpV"><img src="docs/images/discord-official.svg" alt="Join our Discord community" width="166" height="46"></a>
</p>

Native Nintendo 3DS port of **Mario Kart 64**, based on
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart), with a
dual-screen interface, touch menus and customizable controls and display options.

This project was developed with the help of [OpenAI Codex](https://openai.com/codex/) and [Claude Code](https://claude.com/product/claude-code). Learn more about the development process [here](AI_DEVELOPMENT.md).

A legally obtained **USA Mario Kart 64 ROM** is required. No ROM or extracted
game data is included.

## Features

<table>
  <tr>
    <td width="50%" valign="top"><strong>Race your way</strong><br>Single-player Grand Prix, Time Trial, VS and Battle, with CPU opponents, random cups and AUTO spectator mode.</td>
    <td width="50%" valign="top"><strong>Made for two screens</strong><br>Top-screen racing, five HUD layouts, course maps and bottom-screen touch menus.</td>
  </tr>
  <tr>
    <td width="50%" valign="top"><strong>Tune the presentation</strong><br>Adjust render scale, draw distance, filters, UI textures and display profiles.</td>
    <td width="50%" valign="top"><strong>Keep it current</strong><br>Built-in Stable and Experimental updates that preserve your game data, saves and settings.</td>
  </tr>
</table>

<a id="installation"></a>

## Download and installation

1. Download the **CIA** or **3DSX** from the
   [releases page](https://github.com/EstebanPdN/mario-kart-64-3ds/releases).
2. Install the CIA with FBI, or use the 3DSX with the Homebrew Launcher.
3. Place your USA ROM at `sd:/3ds/MK64/mk64.z64`.
4. Launch the port. It validates the ROM and creates `mk64.o2r` on the SD card.
   Later launches reuse this archive.

The ROM must use `.z64` byte order. Use the
[Hack64 ROM Swapper](https://hack64.net/tools/swapper.php) if conversion is needed.
First-time extraction can take a while; keep the console charged.

For faster setup, create `mk64.o2r` with
[SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) on a computer,
then copy it alongside your ROM:

```text
sd:/3ds/MK64/mk64.z64
sd:/3ds/MK64/mk64.o2r
```

Use **Options > Game > Update** for built-in updates. See the
[updater guide](platform/3ds/update/README.md) for details.

## Controls

| Input | Action |
| --- | --- |
| Circle Pad | Steer |
| A | Accelerate |
| B | Brake / Reverse |
| R | Hop / Drift |
| L | Use item; open Options from Game Select |
| X | Rear view |
| Y | Cycle HUD layouts |
| C-Stick | Hold for Turbo |
| Touch screen | Navigate menus |
| SELECT | Camera distance by default; configurable in Options |
| START | Start / Pause / Continue |

## Documentation

- [Configuration and controls](platform/3ds/docs/CONFIGURATION.md)
- [Building from source](platform/3ds/docs/DEVELOPMENT.md)
- [Troubleshooting](platform/3ds/docs/TROUBLESHOOTING.md)
- [Developer documentation](platform/3ds/docs/README.md)
- [Changelog](CHANGELOG.md) · [Contributing](CONTRIBUTING.md)

## Community and bug reports

[Join the project Discord](https://discord.gg/BjbzfRYpV) for updates, support
and suggestions. Report reproducible problems through
[GitHub issues](https://github.com/EstebanPdN/mario-kart-64-3ds/issues), including
your version, console model and steps to reproduce the issue.

## Credits

- [SpaghettiKart](https://github.com/HarbourMasters/SpaghettiKart) and the N64
  reverse-engineering and decompilation communities — the foundation of this port.
- Epic0522's [MarioKart64-3DSPort-Complete](https://github.com/Epic0522/MarioKart64-3DSPort-Complete)
  — solo modes, AUTO, random cups, Modern Logic and runtime corrections.
- Epic0522's [Super Mario 64 3DS Port Ultimate](https://github.com/Epic0522/Super-Mario-64-3ds-port---Ultimate)
  — 3DS implementation and optimization references.
- The Nintendo 3DS homebrew community — tools and libraries.
- EstebanPdN — Nintendo 3DS adaptation, HUD, branding and updater, with help from Codex.

## License and legal notice

Licenses are component-specific. The updater includes
[GNU GPL v3.0](platform/3ds/update/COPYING); its dependencies retain their
[curl](platform/3ds/update-dependencies/LICENSE-curl.txt),
[mbedTLS](platform/3ds/update-dependencies/LICENSE-mbedTLS.txt) and
[Jansson](platform/3ds/update-dependencies/LICENSE-Jansson.txt) licenses.
SpaghettiKart and its libraries retain their upstream notices. No blanket
license applies to the whole repository or to Nintendo game content.

This is an unofficial fan project, unaffiliated with Nintendo or Harbour Masters.
Mario Kart 64 and its game content belong to Nintendo.

---

<p align="center">
  <a href="https://estebanpdn.github.io/">Esteban’s Forge</a> ·
  <a href="https://discord.gg/BjbzfRYpV">Discord</a> ·
  <a href="#documentation">Documentation</a> ·
  <a href="#license-and-legal-notice">License and notices</a><br>
  <sub>An independent fan project. Game names and content belong to their respective owners.</sub>
</p>
