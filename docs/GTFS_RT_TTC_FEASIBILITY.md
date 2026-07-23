# TransitInk OS TTC GTFS-Realtime notes

> **Product status:** this repository is a TTC-only edition of TransitInk OS.
> It keeps the original four-slot e-ink dashboard shell and setup/sleep UX, and
> replaces the Hong Kong transit stack with on-device GTFS-Realtime filtering for
> Toronto surface routes.

## Live data path

```text
Surface GTFS (Open Toronto) → scripts/generate_ttc_route_catalog.py
  → data/catalog/ttc/ + embedded TransitTtcCatalogAssets
Config portal: pick route + stop for each of 4 slots
Device: HTTPS GET https://bustime.ttc.ca/gtfsrt/trips (~130KB protobuf)
  → shared feed cache (~55s TTL)
  → GtfsRealtimeTripFilter by stop_id / route_id
  → TtcProvider → WidgetScheduler → e-ink lanes ("N min")
```

## Endpoints and limits

| Item | Detail |
| --- | --- |
| Trip Updates | `https://bustime.ttc.ca/gtfsrt/trips` |
| Scope | Surface (bus / streetcar). Full subway ETA is not this feed. |
| Auth | None |
| TLS | GlobalSign Root CA - R3 (`TransitTlsTrust.h`) |
| Static GTFS | Open Toronto `SurfaceGTFS.zip` (~80MB); reduced pack under `data/catalog/ttc/` |
| Licence | [Open Government Licence – Toronto](https://open.toronto.ca/open-data-licence/) |

## Why on-device filter (no standing proxy)

Official TTC live ETA is a city-wide protobuf feed, not a stop-scoped JSON API.
ESP32-S3 with PSRAM can download ~130KB and filter locally if:

1. Catalog is a reduced stop/route pack (not full GTFS).
2. All four slots share one TripUpdates download per refresh window.
3. TLS trusts GlobalSign for `*.ttc.ca`.

Maintainer refresh: `python3 scripts/generate_ttc_route_catalog.py --refresh`
(also `.github/workflows/ttc-catalog.yml`).

## Provenance

The dashboard shell and device lifecycle come from upstream
[TransitInk OS](https://github.com/Zerie55699/transitink-os). Earlier design
notes compared Hong Kong stop-scoped JSON clients with TTC GTFS-RT; that
comparison motivated the TTC-only cut-over in this edition. Do not reintroduce
Hong Kong operator clients unless the product scope is intentionally widened
and documented.
