# Changelog

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
