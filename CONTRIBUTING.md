# Contributing to TransitInk OS (TTC edition)

Thanks for helping improve this TTC-focused edition of TransitInk OS. Start with
[README](README.md), [Project structure](docs/PROJECT_STRUCTURE.md), and
[Development](docs/DEVELOPMENT.md) before changing production code.

This repository is derived from
[Zerie55699/transitink-os](https://github.com/Zerie55699/transitink-os). Keep
Hong Kong operator clients and catalogs out of this product line unless a change
explicitly reintroduces multi-city support and documents that scope change.

## Local checks

Set up the repository-local tools, run the complete test suite, and build the
firmware:

```bash
scripts/install_tools.sh
python3 -m unittest discover -s tests -p "test_*.py" -q
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
```

Changes that affect board pins, power, sleep/wake behaviour, flash layout,
e-paper refresh policy, GTFS-Realtime parsing, or the TTC catalog generator
should also be verified on hardware when possible. Include the board revision
and verification performed in the pull request.

## Pull requests

- Keep each change focused and explain its user-visible or hardware impact.
- Add tests for new behaviour and preserve the existing module boundaries.
- Do not include generated build output, device backups, credentials, local
  access-point tokens, or private serial logs.
- Regenerate generated source with its checked-in script; do not edit generated
  tables by hand.
- Update public documentation when commands, configuration, supported hardware,
  or the TTC data path change.
- Prefer English for user-facing strings and project documentation in this
  edition.

Unless explicitly stated otherwise, contributions intentionally submitted for
inclusion in TransitInk OS must be licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE). By submitting a contribution,
you confirm that you have the right to provide it under those terms.
Follow the release procedure in [Development](docs/DEVELOPMENT.md) before
publishing a release.
