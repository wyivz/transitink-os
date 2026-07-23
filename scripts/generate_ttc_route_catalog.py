#!/usr/bin/env python3
"""Generate a reduced TTC Surface GTFS catalog for on-device stop selection."""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import sys
import urllib.request
import zipfile
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "data" / "catalog" / "ttc"
HEADER = ROOT / "include" / "generated" / "TransitTtcCatalogAssets.h"
SOURCE = ROOT / "src" / "generated" / "TransitTtcCatalogAssets.cpp"
CACHE = ROOT / ".catalog-cache" / "ttc"
SURFACE_GTFS_URL = (
    "https://opendata.toronto.ca/toronto.transit.commission/"
    "ttc-routes-and-schedules/SurfaceGTFS.zip"
)
SCHEMA_VERSION = 1
MAX_INDEX_GZIP_BYTES = 64 * 1024
MAX_STOPS_GZIP_BYTES = 512 * 1024
ASSET_NAMES = ("index.json.gz", "stops-ttc.json.gz")


class CatalogError(RuntimeError):
    pass


def sha256_bytes(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def canonical_json(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode(
        "utf-8"
    )


def gzip_bytes(value: Any) -> bytes:
    return gzip.compress(canonical_json(value), compresslevel=9, mtime=0)


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".partial")
    request = urllib.request.Request(url, headers={"User-Agent": "TransitInkOS-TTC-Catalog/1.0"})
    with urllib.request.urlopen(request, timeout=180) as response, temporary.open("wb") as handle:
        while True:
            chunk = response.read(1024 * 256)
            if not chunk:
                break
            handle.write(chunk)
    temporary.replace(destination)


def extract_surface_gtfs(archive: Path, target: Path) -> Path:
    if target.exists():
        for path in sorted(target.rglob("*"), reverse=True):
            if path.is_file():
                path.unlink()
            elif path.is_dir():
                path.rmdir()
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zipped:
        zipped.extractall(target)
    return target


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def build_catalog(gtfs_dir: Path, revision: str, generated_at: str) -> tuple[dict, dict, dict]:
    routes: dict[str, dict[str, str]] = {}
    for row in read_csv(gtfs_dir / "routes.txt"):
        routes[row["route_id"]] = {
            "short": (row.get("route_short_name") or row["route_id"]).strip(),
            "long": (row.get("route_long_name") or "").strip(),
        }

    stops: dict[str, dict[str, str]] = {}
    for row in read_csv(gtfs_dir / "stops.txt"):
        if row.get("location_type") not in ("", "0", None):
            continue
        stops[row["stop_id"]] = {
            "code": (row.get("stop_code") or "").strip(),
            "name": (row.get("stop_name") or "").strip().strip('"'),
        }

    trip_info: dict[str, tuple[str, str, str]] = {}
    for row in read_csv(gtfs_dir / "trips.txt"):
        trip_info[row["trip_id"]] = (
            row["route_id"],
            row.get("direction_id") or "0",
            (row.get("trip_headsign") or "").strip(),
        )

    best: dict[tuple[str, str], tuple[int, str, list[tuple[int, str]]]] = {}

    def consider(trip_id: str, rows: list[tuple[int, str]]) -> None:
        info = trip_info.get(trip_id)
        if info is None:
            return
        route_id, direction_id, headsign = info
        key = (route_id, direction_id)
        rows = sorted(rows)
        previous = best.get(key)
        if previous is None or len(rows) > previous[0]:
            best[key] = (len(rows), headsign, rows)

    current_trip: str | None = None
    current_rows: list[tuple[int, str]] = []
    with (gtfs_dir / "stop_times.txt").open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            trip_id = row["trip_id"]
            if current_trip is None:
                current_trip = trip_id
            if trip_id != current_trip:
                consider(current_trip, current_rows)
                current_trip = trip_id
                current_rows = []
            current_rows.append((int(row["stop_sequence"]), row["stop_id"]))
    if current_trip is not None:
        consider(current_trip, current_rows)

    index_routes: dict[str, list[dict[str, str]]] = defaultdict(list)
    stops_pack: dict[str, list[dict[str, Any]]] = {}
    stop_count = 0
    for (route_id, direction_id), (_count, headsign, rows) in sorted(best.items()):
        route = routes.get(route_id)
        if route is None or not rows:
            continue
        stop_key = f"{route_id}:{direction_id}"
        stop_rows: list[dict[str, Any]] = []
        for sequence, stop_id in enumerate((sid for _, sid in rows), start=1):
            stop = stops.get(stop_id)
            if stop is None:
                continue
            stop_rows.append(
                {
                    "id": stop_id,
                    "code": stop["code"],
                    "label": stop["name"][:96],
                    "sequence": sequence,
                }
            )
        if not stop_rows:
            continue
        index_routes[route["short"]].append(
            {
                "route_id": route_id,
                "direction_id": direction_id,
                "origin_label": stop_rows[0]["label"][:96],
                "destination_label": (headsign or stop_rows[-1]["label"])[:96],
                "stop_key": stop_key,
            }
        )
        stops_pack[stop_key] = stop_rows
        stop_count += len(stop_rows)

    index = {
        "schema_version": SCHEMA_VERSION,
        "revision": revision,
        "generated_at": generated_at,
        "agency": "ttc",
        "bus": {"ttc": {"routes": dict(sorted(index_routes.items()))}},
    }
    stops_asset = {
        "schema_version": SCHEMA_VERSION,
        "revision": revision,
        "generated_at": generated_at,
        "agency": "ttc",
        "provider": "ttc",
        "routes": stops_pack,
    }
    counts = {
        "ttc_routes": len(index_routes),
        "ttc_directions": len(stops_pack),
        "ttc_stop_refs": stop_count,
        "ttc_stops": len(stops),
    }
    return index, stops_asset, counts


def c_array(name: str, content: bytes) -> str:
    lines = []
    for offset in range(0, len(content), 16):
        chunk = content[offset : offset + 16]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return (
        f"const uint8_t {name}[] PROGMEM = {{\n"
        + "\n".join(lines)
        + f"\n}};\nconst std::size_t {name}Size = sizeof({name});\n"
    )


def generate_cpp(payloads: dict[str, bytes], manifest: dict[str, Any]) -> tuple[bytes, bytes]:
    header = """#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "generated/TransitCatalogAssets.h"

namespace transitink {

extern const char kEmbeddedTtcCatalogRevision[];
extern const char kEmbeddedTtcCatalogGeneratedAt[];
extern const EmbeddedCatalogAsset kEmbeddedTtcCatalogAssets[];
extern const std::size_t kEmbeddedTtcCatalogAssetCount;

}  // namespace transitink
"""
    declarations = []
    entries = []
    mapping = {
        "index.json.gz": ("kTtcCatalogIndex", "ttc/index.json"),
        "stops-ttc.json.gz": ("kTtcCatalogStops", "ttc/stops-ttc.json"),
    }
    for asset_name in ASSET_NAMES:
        symbol, path = mapping[asset_name]
        declarations.append(c_array(symbol, payloads[asset_name]))
        entries.append(
            f'    {{"{path}", {symbol}, {symbol}Size, "{manifest["assets"][asset_name]["sha256"]}"}},'
        )
    source = f"""#include \"generated/TransitTtcCatalogAssets.h\"

namespace transitink {{

const char kEmbeddedTtcCatalogRevision[] = \"{manifest['revision']}\";
const char kEmbeddedTtcCatalogGeneratedAt[] = \"{manifest['generated_at']}\";

namespace {{

{''.join(declarations)}
}}  // namespace

const EmbeddedCatalogAsset kEmbeddedTtcCatalogAssets[] = {{
{chr(10).join(entries)}
}};
const std::size_t kEmbeddedTtcCatalogAssetCount =
    sizeof(kEmbeddedTtcCatalogAssets) / sizeof(kEmbeddedTtcCatalogAssets[0]);

}}  // namespace transitink
"""
    return header.encode("utf-8"), source.encode("utf-8")


def build_outputs(gtfs_dir: Path) -> tuple[dict[str, bytes], dict[str, Any]]:
    generated_at = datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds")
    seed = canonical_json({"source": SURFACE_GTFS_URL, "generated_at": generated_at})
    revision = sha256_bytes(seed)[:16]
    index, stops_asset, counts = build_catalog(gtfs_dir, revision, generated_at)
    payloads = {
        "index.json.gz": gzip_bytes(index),
        "stops-ttc.json.gz": gzip_bytes(stops_asset),
    }
    if len(payloads["index.json.gz"]) > MAX_INDEX_GZIP_BYTES:
        raise CatalogError("TTC index.json.gz exceeds 64 KiB")
    if len(payloads["stops-ttc.json.gz"]) > MAX_STOPS_GZIP_BYTES:
        raise CatalogError("TTC stops-ttc.json.gz exceeds 512 KiB")
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "revision": revision,
        "generated_at": generated_at,
        "source_url": SURFACE_GTFS_URL,
        "counts": counts,
        "assets": {
            name: {"bytes": len(content), "sha256": sha256_bytes(content)}
            for name, content in payloads.items()
        },
    }
    return payloads, manifest


def write_outputs(payloads: dict[str, bytes], manifest: dict[str, Any]) -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, content in payloads.items():
        (OUTPUT / name).write_bytes(content)
    (OUTPUT / "catalog-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    header, source = generate_cpp(payloads, manifest)
    HEADER.parent.mkdir(parents=True, exist_ok=True)
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    HEADER.write_bytes(header)
    SOURCE.write_bytes(source)


def check_committed() -> None:
    if not (OUTPUT / "catalog-manifest.json").exists():
        raise CatalogError("missing data/catalog/ttc/catalog-manifest.json")
    manifest = json.loads((OUTPUT / "catalog-manifest.json").read_text(encoding="utf-8"))
    payloads = {}
    for name in ASSET_NAMES:
        path = OUTPUT / name
        if not path.exists():
            raise CatalogError(f"missing {path}")
        content = path.read_bytes()
        payloads[name] = content
        meta = manifest["assets"][name]
        if meta["bytes"] != len(content) or meta["sha256"] != sha256_bytes(content):
            raise CatalogError(f"{name} does not match manifest")
        decoded = json.loads(gzip.decompress(content).decode("utf-8"))
        if decoded.get("revision") != manifest["revision"]:
            raise CatalogError(f"{name} revision mismatch")
    header, source = generate_cpp(payloads, manifest)
    if HEADER.read_bytes() != header or SOURCE.read_bytes() != source:
        raise CatalogError("generated TTC catalog C++ assets are out of date")


def refresh() -> None:
    CACHE.mkdir(parents=True, exist_ok=True)
    archive = CACHE / "SurfaceGTFS.zip"
    print(f"Downloading {SURFACE_GTFS_URL}", file=sys.stderr)
    download(SURFACE_GTFS_URL, archive)
    gtfs_dir = extract_surface_gtfs(archive, CACHE / "gtfs")
    payloads, manifest = build_outputs(gtfs_dir)
    write_outputs(payloads, manifest)
    print(
        json.dumps(
            {
                "revision": manifest["revision"],
                "counts": manifest["counts"],
                "gzip_bytes": {name: len(payloads[name]) for name in ASSET_NAMES},
            },
            indent=2,
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--refresh", action="store_true")
    group.add_argument("--check", action="store_true")
    group.add_argument(
        "--from-gtfs-dir",
        type=Path,
        help="Build from an already-extracted Surface GTFS directory",
    )
    args = parser.parse_args()
    try:
        if args.check:
            check_committed()
            print("TTC catalog assets OK")
        elif args.refresh:
            refresh()
        else:
            payloads, manifest = build_outputs(args.from_gtfs_dir)
            write_outputs(payloads, manifest)
            print(json.dumps({"revision": manifest["revision"], "counts": manifest["counts"]}))
    except CatalogError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
