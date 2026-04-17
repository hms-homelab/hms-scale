# hms-scale

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-support-%23FFDD00.svg?logo=buy-me-a-coffee)](https://www.buymeacoffee.com/aamat09)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![experimental](https://img.shields.io/badge/status-experimental-orange)

C++ smart scale service with ML user identification, BIA body composition analysis, and Angular dashboard. 5 MB memory.

Receives measurements from an ESPHome scale via MQTT, identifies which household member stepped on using a 4-stage hybrid engine (deterministic + Random Forest ML), calculates body composition metrics, and publishes results to Home Assistant.

## Features

- 4-stage user identification (exact match, ML Random Forest, tolerance, manual)
- BIA body composition (Kyle, Janssen, Watson, Mifflin-St Jeor, Hologic)
- Background ML training with GridSearch hyperparameter optimization
- Habit analytics (consistency, streaks, trends, predictions, recommendations)
- Home Assistant MQTT auto-discovery (12 sensors per user)
- Angular 21 web dashboard with Chart.js charts
- Web-based configuration management
- 108 unit tests

## Quick Start

### 1. Build

```bash
# Dependencies (Debian/Ubuntu)
sudo apt install build-essential cmake libcurl4-openssl-dev libpq-dev \
    libpqxx-dev libssl-dev libjsoncpp-dev libpaho-mqtt-dev \
    libpaho-mqttpp-dev nlohmann-json3-dev libspdlog-dev libdrogon-dev \
    uuid-dev libbrotli-dev zlib1g-dev

# Backend
mkdir build && cd build
cmake ..
make -j$(nproc)

# Tests
./tests/run_tests

# Frontend
cd ../frontend
npm install
npx ng build --configuration production
```

### 2. Configure

```bash
cp config.json.example ~/.hms-colada/config.json
# Edit with your PostgreSQL and MQTT credentials
```

### 3. Run

```bash
./build/hms_colada
```

### 4. Install as Service

```bash
sudo cp build/hms_colada /usr/local/bin/
sudo mkdir -p /usr/local/share/hms-colada/static/browser
sudo cp -r frontend/dist/browser/* /usr/local/share/hms-colada/static/browser/
sudo cp hms-colada.service.example /etc/systemd/system/hms-colada.service
# Edit service file with your credentials
sudo systemctl daemon-reload
sudo systemctl enable --now hms-colada
```

## Configuration

Copy `config.json.example` to `~/.hms-colada/config.json`, or use environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `DB_HOST` | localhost | PostgreSQL host |
| `DB_PORT` | 5432 | PostgreSQL port |
| `DB_NAME` | colada_scale | Database name |
| `DB_USER` | colada_user | Database user |
| `DB_PASSWORD` | | Database password |
| `MQTT_BROKER` | localhost | MQTT broker host |
| `MQTT_PORT` | 1883 | MQTT broker port |
| `MQTT_USER` | | MQTT username |
| `MQTT_PASSWORD` | | MQTT password |
| `WEB_PORT` | 8889 | Web server port |
| `STATIC_DIR` | ./static/browser | Frontend files path |
| `ML_ENABLED` | false | Enable background ML training |
| `ML_SCHEDULE` | weekly | Training schedule (daily/weekly/monthly) |

Configuration can also be managed from the web UI at `/settings`.

## Database Setup

```sql
CREATE DATABASE colada_scale;

CREATE TABLE scale_users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(100) NOT NULL,
    date_of_birth DATE,
    sex VARCHAR(10),
    height_cm DECIMAL(5,2),
    expected_weight_kg DECIMAL(5,2),
    weight_tolerance_kg DECIMAL(4,2) DEFAULT 3.0,
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    last_measurement_at TIMESTAMPTZ
);

CREATE TABLE scale_measurements (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID REFERENCES scale_users(id),
    weight_kg DECIMAL(5,2),
    weight_lbs DECIMAL(6,3),
    impedance_ohm DECIMAL(6,2),
    body_fat_percentage DECIMAL(4,2),
    lean_mass_kg DECIMAL(5,2),
    muscle_mass_kg DECIMAL(5,2),
    bone_mass_kg DECIMAL(4,2),
    body_water_percentage DECIMAL(4,2),
    visceral_fat_rating INTEGER,
    bmi DECIMAL(4,2),
    bmr_kcal INTEGER,
    metabolic_age INTEGER,
    protein_percentage DECIMAL(4,2),
    identification_confidence DECIMAL(5,2),
    identification_method VARCHAR(50),
    measured_at TIMESTAMPTZ DEFAULT NOW(),
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

## MQTT Topics

```
giraffe_scale/measurement              # subscribe: {"weight_kg", "weight_lb", "impedance"}
colada_scale/user_selector/set         # subscribe: manual user selection from HA

colada_scale/{user}/weight             # publish: state per metric (retained)
colada_scale/{user}/body_fat
colada_scale/{user}/muscle_mass
colada_scale/{user}/bmi
colada_scale/{user}/bmr
colada_scale/{user}/body_water
colada_scale/{user}/bone_mass
colada_scale/{user}/visceral_fat
colada_scale/{user}/metabolic_age
colada_scale/{user}/protein
colada_scale/{user}/lean_mass
colada_scale/{user}/last_measurement
colada_scale/status                    # "online"/"offline" (LWT)
```

HA auto-discovery publishes sensor configs to `homeassistant/sensor/colada_scale_{user}/*/config`.

## Architecture

```
ESPHome Scale (ESP32)
    |
    | MQTT (giraffe_scale/measurement)
    v
hms-scale
    |
    +-- Hybrid Identification Engine
    |       Stage 1: Exact match (+/-0.5 kg, 95% confidence)
    |       Stage 2: ML Random Forest (>= 80% confidence)
    |       Stage 3: Tolerance match (user-defined, 75% confidence)
    |       Stage 4: Manual assignment
    |
    +-- BIA Body Composition Calculator
    |       Kyle (body fat), Janssen (muscle), Watson (water)
    |       Mifflin-St Jeor (BMR), Hologic (bone), visceral fat
    |
    +-- ML Training Service (background thread)
    |       GridSearch: 12 hyperparameter combos, 5-fold CV
    |       Schedule: daily / weekly / monthly
    |
    +-- Habit Analyzer
    |       Consistency, streaks, trends, predictions
    |
    +-- MQTT Discovery Publisher
    |       12 sensors per user, HA auto-discovery
    |
    v
PostgreSQL          Home Assistant          Angular Dashboard
(measurements)      (MQTT sensors)          (port 8889)
```

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |
| GET | `/api/dashboard` | Dashboard with latest per-user measurements |
| GET/POST | `/api/users` | List or create users |
| GET/PUT/DELETE | `/api/users/{id}` | Get, update, or delete user |
| GET | `/api/measurements` | Query measurements (`?user_id=&days=`) |
| GET | `/api/measurements/unassigned` | Unassigned measurements |
| POST | `/api/measurements/{id}/assign` | Manual user assignment |
| POST | `/api/ml/train` | Trigger ML model training |
| GET | `/api/ml/status` | Training status and metrics |
| POST | `/api/ml/predict` | Predict user from weight/impedance |
| GET | `/api/analytics/daily` | Daily weight averages |
| GET | `/api/analytics/weekly` | Weekly trends |
| GET | `/api/analytics/summary` | User summary with stats |
| GET | `/api/analytics/progress` | Time-series for charts |
| GET | `/api/habits/insights` | Habit analysis with recommendations |
| GET | `/api/habits/predictions` | Weight predictions |
| GET/PUT | `/api/config` | Read or update service configuration |

## Docker

```bash
# Build
docker build -t hms-scale .

# Run
docker run -d \
    -e DB_HOST=192.168.1.100 \
    -e DB_PASSWORD=your_password \
    -e MQTT_BROKER=192.168.1.100 \
    -e MQTT_USER=user \
    -e MQTT_PASSWORD=pass \
    -p 8889:8889 \
    hms-scale
```

## Related Projects

- [hms-shared](https://github.com/hms-homelab/hms-shared) -- shared C++ libraries (MqttClient, DbPool)
- [hms-cpap](https://github.com/hms-homelab/hms-cpap) -- CPAP therapy data collection and ML analysis
- [hms-tuya](https://github.com/hms-homelab/hms-tuya) -- Tuya WiFi MQTT bridge
- [hms-portal](https://github.com/hms-homelab/hms-portal) -- homelab dashboard

## License

MIT License -- see [LICENSE](LICENSE) for details.
