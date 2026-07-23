# TransitInk OS (TTC edition)

TransitInk OS is an ESP32-S3 firmware for a **400×300 e-paper arrival dashboard**.
This repository is a **Toronto TTC-only edition**: it shows live bus and
streetcar arrival countdowns on up to four widget slots, with an on-device
**English** settings portal.

It runs on the same **Zectrix Note 4** hardware profile as the original
project.

## Relationship to the original project

This firmware is based on the original open-source
[TransitInk OS](https://github.com/Zerie55699/transitink-os) by
[@Zerie55699](https://github.com/Zerie55699).

| | Original project | This edition |
| --- | --- | --- |
| City / operators | Hong Kong multi-operator transit | Toronto **TTC surface** (bus / streetcar) |
| Live data | Operator-specific stop APIs (JSON/XML) | TTC BusTime **GTFS-Realtime** Trip Updates |
| Settings UI | Traditional Chinese portal | English portal |
| Dashboard shell | Four e-ink lanes, Wi-Fi setup, sleep/wake | Same interaction model and layout style |
| Weather footer | Hong Kong Observatory fetch | Layout reserved; **no remote weather fetch** |

Credit and thanks belong to the original author for the hardware integration,
e-ink dashboard shell, configuration portal security model, sleep/wake
behaviour, installer tooling, and overall product architecture. This fork
replaces the Hong Kong transit stack with a TTC-only data path while keeping
that shell.

This edition is **not** a drop-in replacement for Hong Kong users. If you need
KMB, Citybus, Green Minibus, MTR, Light Rail, or journey-time widgets, use the
[upstream project](https://github.com/Zerie55699/transitink-os).

## Current status

- **Product scope:** TTC surface arrivals only (`Disabled` / `TtcEta` widgets).
- **Config schema:** v3.
- **Hardware target:** Zectrix Note 4 (`zectrix_note4` PlatformIO environment).
- **Language:** English UI on device and in the settings portal.
- **Network model:** no standing proxy required; the device downloads the public
  Trip Updates feed and filters it locally by configured `route_id` / `stop_id`.
- **Not included yet:** full subway ETA from this feed, remote weather, and
  Hong Kong operators.

## Features

- Four independently configurable arrival slots on a 400×300 e-paper panel
- Live TTC bus / streetcar ETAs from
  [`https://bustime.ttc.ca/gtfsrt/trips`](https://bustime.ttc.ca/gtfsrt/trips)
- Shared on-device feed cache so active slots reuse one download window
- Embedded TTC route/stop catalog for portal selection (`data/catalog/ttc/`)
- First-boot Wi-Fi setup access point and later LAN settings via Volume Up + QR
- Optional sleep, wake duration, maintenance wake, and daily awake window
- Browser installer packaging path inherited from the original project

## Demo hardware

![Zectrix Note 4 running TransitInk OS](installer/assets/zectrix-note4-product.png)

TransitInk OS is an independent source-available project and is not affiliated
with or endorsed by Zectrix, the Toronto Transit Commission, or the City of
Toronto. Zectrix, TTC, and related names remain the property of their
respective owners.

## End-user quick start

1. Flash this firmware to a compatible Zectrix Note 4 (see Build / flash below).
   Always create a full flash backup first.
2. On first boot with no valid settings, connect to the on-device Wi-Fi network
   `TransitInk-xxxx` using the password shown on the screen.
3. Open `http://192.168.4.1/` and configure:
   - **Wi-Fi** — your home/office network
   - **Widgets** — up to four TTC route / direction / stop slots (or Disabled)
   - **Power** — optional sleep and daily wake window
4. Tap **Save and restart**.
5. After the device joins your network, press **Volume Up** any time to reopen
   settings on the device LAN IP (QR code on screen). If Wi-Fi is down, it falls
   back to the setup access point.

During normal use the dashboard refreshes active TTC slots about every
60 seconds, downloads the Trip Updates feed, filters arrivals for each
configured stop, and shows the next one or two countdowns per slot (for example
`5 min`).

Timezone defaults follow Eastern Time with daylight-saving rules suitable for
Toronto.

## What this edition does not do

- Full **subway** arrival parity (the configured live feed is surface GTFS-RT)
- Hong Kong operators, catalogs, or Traditional Chinese product copy
- Live weather in the footer
- Continuous cloud proxy / backend hosting for stop filtering

## Build, test, and flash (developers)

Helper scripts target macOS or Linux. Python 3 and a C/C++ toolchain are
required.

```bash
scripts/install_tools.sh
scripts/audit_python_tools.sh
.venv/bin/python scripts/generate_hk_glyph_font.py --check
.venv/bin/python scripts/generate_ttc_route_catalog.py --check
python3 -m unittest discover -s tests -p "test_*.py" -q
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio check -e zectrix_note4 --fail-on-defect high
```

Before replacing firmware on a device:

```bash
export ESP32_PORT=/dev/cu.usbmodemXXXX   # or your serial device
scripts/backup_flash.sh "$ESP32_PORT"
scripts/flash_firmware.sh "$ESP32_PORT"
```

Restore if needed:

```bash
scripts/restore_flash.sh backups/<backup>.bin "$ESP32_PORT"
```

Flashing or restoring can make a device temporarily unusable. Confirm the
board, flash size, and serial port first.

### Maintainer catalog refresh

The settings portal uses the versioned TTC catalog embedded in firmware under
`data/catalog/ttc/`. Normal device operation does **not** download the full
static GTFS zip. Maintainers refresh the reduced catalog with:

```bash
.venv/bin/python scripts/generate_ttc_route_catalog.py --refresh
```

There is also a scheduled workflow at `.github/workflows/ttc-catalog.yml`.

## Power behaviour

Power settings include an optional daily awake window (default example
08:00–09:00 when first enabled). The device can schedule a low-power timer wake
at the start of the window, update normally during it, and return to sleep
afterward. Outside the window, the wake button still uses the configured awake
duration. Periodic sleeping maintenance wakes are disabled while the daily
window is enabled. Scheduling requires the clock to have been synchronized over
Wi-Fi at least once since power-on.

## Repository layout

```text
include/          C++ headers and board configuration
src/              firmware, TTC client, display, portal, application entry
src/core/         hardware-independent domain logic (including GTFS-RT filter)
src/generated/    generated glyph data and embedded TTC catalog arrays
data/catalog/ttc/ TTC catalog assets and integrity metadata
src/hardware/     ESP32 board support and display drivers
src/providers/    TTC widget provider and scheduler router
include/hardware/ compile-time board profiles
installer/        GitHub Pages installer source
lib/              vendored third-party source
third_party/      redistributable assets with pinned provenance
scripts/          setup, catalog/font generation, backup, flash, restore
test_host/        native C++ behaviour tests
test_native/      PlatformIO Unity tests
tests/            Python structure tests and native-test orchestration
docs/             architecture, development, and hardware guides
```

More detail:

- [Project structure](docs/PROJECT_STRUCTURE.md)
- [Development](docs/DEVELOPMENT.md)
- [TTC GTFS-Realtime notes](docs/GTFS_RT_TTC_FEASIBILITY.md)
- [Adding hardware](docs/ADDING_HARDWARE.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
- [Third-party data](THIRD_PARTY_DATA.md)
- [Security policy](SECURITY.md)

## Hardware profiles

`zectrix_note4` is the default PlatformIO environment. Its profile is
`include/hardware/boards/ZectrixNote4.h` (SSD1683-style 400×300 panel). Product
and portal code do not embed that board’s GPIO map. See
[Adding hardware](docs/ADDING_HARDWARE.md) before targeting another board.

## Web installer and releases

Build a local installer package after the firmware build:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

Generated Pages content lands in `dist/installer/` and is not committed. Pushing
a `vX.Y.Z` tag that matches `FIRMWARE_VERSION` publishes release assets and can
deploy Pages for **this** repository.

The upstream author’s public installer for the original Hong Kong product is at
[https://zerie55699.github.io/transitink-os/](https://zerie55699.github.io/transitink-os/).
Do not assume that site serves this TTC edition unless you intentionally publish
there.

## Licence and data terms

Original TransitInk OS source code is licensed for noncommercial purposes under
the [PolyForm Noncommercial License 1.0.0](LICENSE). Commercial use requires a
separate written licence from the applicable copyright holder.

Redistributable fonts, generated glyphs, vendored components, and transport data
retain the separate terms in
[Third-party notices](THIRD_PARTY_NOTICES.md) and
[Third-party data](THIRD_PARTY_DATA.md). TTC catalog/ETA data is subject to the
[Open Government Licence – Toronto](https://open.toronto.ca/open-data-licence/).

Binary-release recipients can use
[Corresponding source and rebuild information](CORRESPONDING_SOURCE.md) to
reproduce the firmware with the pinned Arduino core and libraries.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a change. Keep
credentials, flash backups, generated build output, and local device state out
of commits. Prefer changes that preserve the TTC-only product boundary unless a
contribution explicitly aims to restore multi-city support.
