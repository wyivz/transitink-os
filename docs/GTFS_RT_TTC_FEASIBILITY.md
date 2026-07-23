# TransitInk OS TTC GTFS-Realtime notes

> **Current product status:** this branch is TTC-only. Firmware widget types are
> `Disabled` and `TtcEta` only. Route/stop selection uses the embedded
> `data/catalog/ttc/` catalog. Live arrivals come from
> `https://bustime.ttc.ca/gtfsrt/trips` (GTFS-Realtime Trip Updates), filtered
> on-device by configured `route_id` and `stop_id`. The display keeps the
> original four-slot e-ink layout; Hong Kong operators, catalogs, weather, and
> Traditional Chinese product copy are removed.

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

## Historical feasibility notes

Earlier investigation compared Hong Kong stop-scoped JSON clients with TTC
GTFS-RT. That comparison motivated the TTC-only cut-over on this branch. The
implementation above is the landed design; do not reintroduce Hong Kong
operator clients into this product line.
