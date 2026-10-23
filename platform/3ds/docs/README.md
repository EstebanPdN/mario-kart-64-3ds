# Documentation — 2.0

Use the document that answers the current task. This documentation describes
the native port, including implemented behavior and named validation limits.
It does not certify sustained performance or compatibility on untested hardware.

| Task | Document |
| --- | --- |
| Install, supply game data, learn controls | [Project README](../../../README.md) |
| Build a recursive clone and make a first change | [Development](DEVELOPMENT.md) |
| Find the subsystem that owns a behavior | [Source map](SOURCE_MAP.md) |
| Understand execution, ownership and teardown | [Architecture](ARCHITECTURE.md) |
| Configure Display/Advanced, controls and saved keys | [Configuration](CONFIGURATION.md) |
| Regenerate hints, option icons, HUD labels and maps | [Native sprites](NATIVE_SPRITES.md) |
| Preserve identity, saves, formats and update behavior | [Compatibility](COMPATIBILITY.md) |
| Run host checks, emulator routes and hardware acceptance | [Validation](VALIDATION.md) |
| Recover from startup, SD, update or diagnostic errors | [Troubleshooting](TROUBLESHOOTING.md) |
| Prepare a reviewable contribution | [Contributing](../../../CONTRIBUTING.md) |

The source implementations and generators are the authority for behavior.
Update the relevant reference when a setting, control, path, resource lifetime
or build input changes. Keep historical release notes in the changelog rather
than documenting old behavior as though it were still current.
