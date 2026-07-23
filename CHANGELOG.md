# Changelog

All notable user-visible changes to TransitInk OS will be documented here.

## Unreleased — TTC edition

- Converted the web installer, devices catalog, and no-port dialog to English for TTC users.
- Slimmed the embedded glyph table to Latin UI characters (about 100 glyphs).
- Removed Traditional Chinese installer/dialog copy and Hong Kong widget marketing text.
- Converted the product to a Toronto TTC-only arrival dashboard while retaining
  the original four-slot e-ink shell, Wi-Fi setup flow, and sleep/wake model.
- Replaced Hong Kong operator clients and catalogs with on-device GTFS-Realtime
  Trip Updates filtering against `https://bustime.ttc.ca/gtfsrt/trips`.
- Switched the settings portal and dashboard chrome to English.
- Embedded a reduced TTC Surface GTFS catalog under `data/catalog/ttc/` for
  route/stop selection.
- Removed remote weather fetching; the footer layout remains for compatibility.
- Documented provenance relative to the upstream Hong Kong project.

## 1.0.2 - 2026-07-23

- Added an optional daily automatic wake window, including schedules which
  cross midnight.
- Kept normal button wake behaviour outside the configured automatic window.
- Avoided periodic sleeping-maintenance wake-ups while the daily schedule is
  enabled, reducing unnecessary battery use.

## 1.0.1 - 2026-07-22

- Fixed the browser installer image so it preserves the Zectrix Note 4
  bootloader flash mode and boots after installation.
- Added a release-time check that rejects installer firmware which differs from
  PlatformIO's canonical factory image.
- Changed future TransitInk OS releases to the PolyForm Noncommercial License
  1.0.0; separately licensed third-party components retain their own terms.
- Added an on-device prompt directing users to press Volume Up when no dashboard
  widgets have been configured.
