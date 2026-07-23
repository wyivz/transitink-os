# Project structure

TransitInk OS keeps the standard PlatformIO source layout while separating
hardware-independent behaviour from device and network integration.

This repository is a **TTC edition** derived from the original Hong Kong
TransitInk OS project. The dashboard shell, board profile, portal security
model, and sleep/wake lifecycle are intentionally close to upstream. The live
transit path is TTC-only:

`TtcClient` -> `TtcProvider` -> `WidgetProviderRouter` -> `WidgetScheduler` ->
`EInkDisplay`.

## Directory map

```text
.
├── .github/workflows/           Continuous integration and release workflows
├── data/catalog/ttc/            Checked-in TTC Surface catalog baseline
├── include/                     Firmware headers
│   ├── core/                    Hardware-independent domain interfaces
│   ├── hardware/                Board profiles and display-driver selection
│   └── providers/               Provider adapter interfaces
├── src/                         Production implementation
│   ├── core/                    GTFS-RT filtering, validation, scheduling, snapshots
│   ├── hardware/                ESP32 buttons, wake support, and panel drivers
│   ├── providers/               TTC provider adapter
│   └── main.cpp                 Device lifecycle and application composition
├── scripts/                     Developer and device-operation tools
├── installer/                   Static GitHub Pages installer source
├── test_host/                   Native C++ tests and test shims
├── test_native/                 PlatformIO Unity tests for production paths
├── tests/                       Python structural/integration tests
├── docs/                        Public technical documentation
├── platformio.ini               ESP32-S3 firmware environments
├── platformio.native.ini        Native PlatformIO test environment
├── partitions.csv               16 MiB flash partition layout
└── requirements-dev.txt         Pinned local Python tooling
```

## Production layers

### Application composition

`src/main.cpp` owns boot, Wi-Fi connection, wake/sleep behaviour, the scheduler,
display updates, configuration mode, and the top-level device lifecycle. It
constructs:

- `TtcClient`
- `TtcProvider`
- `WidgetProviderRouter`
- `transitink::WidgetScheduler`
- `ConfigPortal(deviceConfig, configStore)`

`main.cpp` should not contain GTFS-Realtime parsing rules. It also keeps a
`WeatherSnapshot` so the dashboard footer layout remains stable, but the
TTC-only product does not fetch remote weather.

### Hardware profiles and adapters

`include/hardware/boards/` contains compile-time board profiles. A profile owns
display pins and controller type, battery measurement, button mapping and
polarity, and wake capabilities. `BoardSupport` exposes button and wake actions
to the application without leaking GPIO numbers into `main.cpp`.

`SelectedDisplayDriver` maps a profile's controller type to a driver under
`src/hardware/displays/`. `EInkDisplay` owns only the product renderer and frame
refresh policy. A board using the same controller only needs a new profile;
another controller also needs a driver implementing the selected driver's
`begin`, `show`, and `showPartialRegion` surface.

The current dashboard renderer requires an exact 400x300 canvas. Supporting a
different screen size requires a renderer/layout addition, not board-specific
conditionals in the existing renderer.

### Device and service integration

The top level of `src/` contains the Arduino-facing implementation:

- `EInkDisplay` and `HkGlyphFont` render the 400x300 user interface.
- `BatteryMonitor` consumes the selected board's battery profile.
- `ConfigStore`, `AppConfig`, `ConfigPortal`, `PortalConfigCodec`, and
  `TransitInkPortalPage` own persistent settings and the local English web
  portal.
- `TtcClient` performs the only live transit HTTPS request.
- `ConfigPortal` streams the versioned gzip TTC catalog directly from generated
  PROGMEM arrays without decompressing them in ESP32 RAM.

### Frontend TTC catalog

`scripts/generate_ttc_route_catalog.py` is the maintainer path for the
low-frequency TTC Surface GTFS source. It validates and deterministically
generates:

- `data/catalog/ttc/index.json.gz`
- `data/catalog/ttc/stops-ttc.json.gz`
- `data/catalog/ttc/catalog-manifest.json`
- `include/generated/TransitTtcCatalogAssets.h`
- `src/generated/TransitTtcCatalogAssets.cpp`

The settings portal loads `/assets/catalog/current/ttc/index.json` alongside
`/api/config`, then loads `/assets/catalog/current/ttc/stops-ttc.json` when a TTC
stop list is needed. Route, direction, and stop selections resolve in the
browser. Normal selection never calls an official route API and there is no
runtime catalog service.

### Domain core

`include/core/` and `src/core/` contain bounded parsing and business rules that
do not depend on display or Wi-Fi hardware. This includes widget configuration,
normalised snapshots, scheduling, portal request authentication, battery status,
and `GtfsRealtimeTripFilter`.

New rules should go here when they can be expressed without Arduino APIs. This
keeps them fast to compile and easy to exercise with strict host warnings.

### Provider adapter

`src/providers/TtcProvider.cpp` converts TTC records into common widget
snapshots by calling `normalizeTtcSnapshot`. `WidgetProviderRouter` is the
dispatch boundary used by `WidgetScheduler`; display code consumes snapshots
instead of calling the TTC client directly.

## Test layers

The three test directories have distinct roles and should not be merged merely
for naming consistency:

- `test_host/` compiles pure C and C++ modules with the system compiler using
  `-Wall -Wextra -Werror`.
- `test_native/` uses PlatformIO's native Unity runner for production code that
  needs small Arduino or Preferences compatibility shims.
- `tests/` contains Python tests that assert source boundaries, portal
  behaviour, and the firmware's structural contracts. `tests/test_core.py`
  also orchestrates the native C/C++ executables.

The single local verification entry point is:

```bash
python3 -m unittest discover -s tests -p "test_*.py" -q
```

The detailed board extension procedure is documented in
[Adding hardware](ADDING_HARDWARE.md).

## Generated and local-only content

These paths are deliberately excluded from Git:

- `.venv/`, `.platformio/`, and `.pio/` - local tools, packages, and builds
- `.catalog-cache/` - resumable official-source downloads used by the generator
- `.test-build/` - native test executables and objects
- `backups/` - full device flash images, which may contain credentials
- `dist/` - generated release assets and deployable installer package
- `.superpowers/` - local agent workspace

Tracked generated glyph and TTC catalog C++ sources are required by the firmware
build. Their generators, deterministic inputs, gzip assets, manifests and
licensing boundaries live under `scripts/`, `data/catalog/ttc/`,
`THIRD_PARTY_NOTICES.md`, and `THIRD_PARTY_DATA.md`.

## Release and installer boundary

`installer/` contains only reviewed static source. `scripts/package_installer.py`
reads the single firmware version from `include/ProductConfig.h`, merges the
Zectrix Note 4 bootloader, partition table, Arduino boot application and
firmware at their verified offsets, then creates a relative ESP Web Tools
manifest, SHA-256 checksum and release metadata under `dist/installer/`.

The release workflow rejects a tag that does not match `FIRMWARE_VERSION`. The
same generated binary is uploaded to the GitHub Release and deployed through
GitHub Pages, so the installer cannot silently point at a different firmware
than the release asset.
