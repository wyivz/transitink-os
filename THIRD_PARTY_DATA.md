# Third-party transport data

The files under `data/catalog/ttc/` and the byte-for-byte generated arrays in
`src/generated/TransitTtcCatalogAssets.cpp` are factual transport-directory
data, not project source code. The project software licence does not replace or
relicense the terms that apply to these data sources.

## TTC Surface catalog

- Catalog path: `data/catalog/ttc/`
- Generator: `scripts/generate_ttc_route_catalog.py`
- Generated files: `index.json.gz` and `stops-ttc.json.gz`
- Integrity metadata: `data/catalog/ttc/catalog-manifest.json`
- Static catalog source: TTC Surface GTFS ZIP from Open Toronto / TTC open data
  (`SurfaceGTFS.zip`)
- Live ETA source: `https://bustime.ttc.ca/gtfsrt/trips` (GTFS-Realtime),
  filtered on-device by configured `stop_id` and `route_id`
- Licence: [Open Government Licence - Toronto](https://open.toronto.ca/open-data-licence/)
- Maintainer refresh: `python3 scripts/generate_ttc_route_catalog.py --refresh`
  (also scheduled via `.github/workflows/ttc-catalog.yml`)

When redistributing a TTC catalog release, attribute the City of Toronto and the
Toronto Transit Commission under the Open Government Licence - Toronto. Do not
imply endorsement of TransitInk OS.

## Refresh and review policy

Catalog refresh is explicit. `scripts/generate_ttc_route_catalog.py --refresh`
downloads the current TTC Surface GTFS ZIP, extracts routes, trips, stops, and
stop times, reduces them to the route/direction/stop fields needed by the
settings portal, writes deterministic gzip assets, updates the manifest, and
regenerates the embedded PROGMEM arrays.

After generating a new baseline, review the manifest counts, generated date,
source changes, and licence notice before committing and publishing new
firmware.
