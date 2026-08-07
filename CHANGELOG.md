# Changelog

## [Unreleased]

## v1.1.2 — 2026-08-07

### Fixed
- **Every unidentified weigh-in was rejected and lost.** `createMeasurement` bound `nullptr` for `user_id` when no user matched, but still passed `identification_confidence` as the struct's default `0.0`. The table's `valid_user_or_unassigned` constraint requires the two to be null together, so the insert failed and the reading was discarded — the service logged "No user identified - storing as unassigned" immediately followed by "Failed to store measurement". This also starved `GET /api/measurements/unassigned` and `POST /api/measurements/{id}/assign`, which exist precisely so an unmatched reading can be claimed afterwards: they have had nothing new to work with since the C++ service took over in April 2026.
- **The release job published before the Windows build finished.** It declared `needs: [build-and-test]` but downloads the artifact `windows-build` uploads, so it started as soon as the Linux job ended — six minutes early on the v1.1.1 run — and the download always missed. Because that step is `continue-on-error` and the zip step is gated on its outcome, the failure was silent and every release from v1.0.0 through v1.1.1 published with zero assets.

## v1.1.1 — 2026-08-07

### Security
- **Removed live credentials from the tracked `hms-colada.service`.** The unit file carried a real `DB_PASSWORD`, `MQTT_USER` and `MQTT_PASSWORD`, plus internal host addresses, and this repository is public — the values were fetchable without authentication. They are now placeholders, with a pointer to `EnvironmentFile=` for keeping real secrets in an untracked, mode-600 file. Note that removing them from HEAD does not remove them from history; anything that was in there needs rotating.

### Added
- **mDNS publishing, so `hms-scale.local:8889` actually resolves.** The ESP gateway firmware ships with that as its default server, but nothing ever published the name — a fresh install of both halves could not talk to each other until the user found the server's IP by hand. `MdnsPublisher` now announces two records through Avahi on startup: a `_hms-scale._tcp` service carrying the port and a TXT `path=/api/webhook/measurement`, and a `hms-scale.local` CNAME onto the machine's existing name. The service record is the one clients should discover by, since browsing a service type cannot collide with anything; the CNAME exists for firmware that resolves a fixed name, and is a CNAME rather than an A record so it follows the host across DHCP changes. New `mdns` config section plus `MDNS_ENABLED` / `MDNS_HOSTNAME`, defaulting to on.
- Publishing is strictly best-effort and never blocks startup: missing Avahi, a stopped daemon, or no D-Bus at all logs one line and moves on. `BUILD_WITH_MDNS` defaults ON but disables itself on non-Linux platforms and when `libavahi-client-dev` is absent, so it cannot break a Windows build or an existing Debian one. Name collisions between two installs on one network are resolved by Avahi's usual rename and logged.

### Fixed
- **PostgreSQL connection never recovered after the server dropped it.** `PostgresScaleDatabase` created a single `pqxx::connection` in `connect()` and all 16 query methods used it directly, with no health check and no reconnect. A Postgres restart, idle timeout, or network blip therefore broke *every* query permanently — the live service logged `getMeasurementsForML failed: Lost connection to the database server` once a minute for weeks and reported "No measurements available for training" while the database sat healthy with 857 measurements. Queries now go through a `db()` accessor that transparently reconnects, and `createMeasurement` retries once on a broken connection so a weigh-in — which the scale reports exactly once — is not silently lost. Adds regression tests that drop the connection server-side and assert recovery, including across repeated drops.

## v1.1.0 — 2026-05-08

### Added
- Direct BLE connectivity to Etekcity scale via BlueZ D-Bus (sdbus-c++)
- `BleScaleClient`: persistent reconnect loop, MAC-based scan, notify-only on characteristic 0xFFF1 (service 0xFFF0)
- 22-byte Etekcity packet parser with lock-on-stability logic (mirrors ESP-IDF firmware)
- `BUILD_WITH_BLE=ON` CMake option; sdbus-c++ dependency
- `ble` config section in `~/.hms-colada/config.json` with `enabled`, `scale_mac`, `reconnect_delay_s`
- `BLE_ENABLED` and `BLE_SCALE_MAC` environment variable fallbacks

## v1.0.1 — 2026-04-17

### Fixed
- Unit toggle (lbs/kg) in nav bar, persisted in localStorage, all pages respect it
- Height displays as ft'in" in imperial mode (table and edit dialog)
- User edit dialog: feet + inches split fields instead of raw inches
- ML panel: read metrics from nested response (fixes null% display)
- ML panel: feature importance chart renders correctly
- Settings page: MQTT username/password show runtime values (env vars)
- Timestamp parsing for PostgreSQL format (space separator, not ISO T)
- Habits: longest gap, avg gap, consistency score now calculated correctly
- Weight, tolerance, muscle change, predictions all use active unit system

## v1.0.0 — 2026-04-17

### Added
- C++ Drogon backend replacing Python colada-scale-native
- 4-stage hybrid user identification (exact, ML Random Forest, tolerance, manual)
- BIA body composition (Kyle, Janssen, Watson, Mifflin-St Jeor, Hologic)
- ML training service with GridSearch + 5-fold CV, JSON model persistence
- Habit analytics (consistency, streaks, trends, predictions, recommendations)
- MQTT subscriber + Home Assistant discovery publisher (12 sensors per user)
- HTTP webhook endpoint for ESP32 direct delivery
- Device log webhook endpoint with ring buffer
- Angular 21 dashboard with Chart.js (dark theme)
- 5 pages: Dashboard, Users, ML, Habits, Settings
- Web-based configuration management (GET/PUT /api/config)
- Dockerfile (3-stage: node UI, debian C++ builder, slim runtime)
- GitHub Actions CI (build, test, Docker push to GHCR)
- 108 unit tests
