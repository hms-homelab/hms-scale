# hms-scale

![experimental](https://img.shields.io/badge/status-experimental-orange)

C++ smart scale service with ML user identification, BIA body composition analysis, and Angular dashboard.

Receives measurements from an ESPHome scale via MQTT, identifies which household member stepped on using a 4-stage hybrid engine (deterministic + Random Forest ML), calculates body composition metrics, and publishes results to Home Assistant.

## Features

- **4-stage user identification** -- exact weight match, ML Random Forest classifier, tolerance match, manual assignment
- **BIA body composition** -- body fat % (Kyle), muscle mass (Janssen), body water (Watson), BMR (Mifflin-St Jeor), bone mass, visceral fat, metabolic age
- **ML training** -- background Random Forest training with GridSearch hyperparameter optimization, JSON model persistence
- **Habit analytics** -- consistency scoring, streaks, weight trends, predictions, weekly patterns, recommendations
- **MQTT integration** -- subscribes to scale measurements, publishes Home Assistant discovery sensors per user
- **Web dashboard** -- Angular 21 + Chart.js with metric cards, weight trend charts, body composition doughnut, user management, ML training panel

## Stack

- **Backend:** C++17, Drogon, pqxx (PostgreSQL), Paho MQTT, spdlog, nlohmann_json
- **Frontend:** Angular 21, Angular Material, Chart.js 4.5 (zoom + annotation plugins)
- **Shared:** [hms-shared](https://github.com/hms-homelab/hms-shared) v1.6.6 (MqttClient, DbPool)
- **Database:** PostgreSQL

## Build

```bash
# Backend
mkdir build && cd build
cmake ..
make -j$(nproc)

# Tests
./tests/run_tests

# Frontend
cd frontend
npm install
ng build --configuration production
```

## Configuration

Copy `config.json.example` to `~/.hms-colada/config.json` and edit, or use environment variables:

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
| `MQTT_PASS` | | MQTT password |
| `WEB_PORT` | 8889 | Web server port |

## Database Setup

Create the PostgreSQL database and tables:

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

**Subscriptions:**
- `giraffe_scale/measurement` -- scale measurement payload `{"weight_kg", "weight_lb", "impedance"}`
- `colada_scale/user_selector/set` -- manual user selection from Home Assistant

**Publications (per user):**
- `colada_scale/{user}/weight`, `body_fat`, `muscle_mass`, `bmi`, `bmr`, `body_water`, `bone_mass`, `visceral_fat`, `metabolic_age`, `protein`, `lean_mass`, `last_measurement`
- `homeassistant/sensor/colada_scale_{user}/*/config` -- HA MQTT discovery

## API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Service health check |
| GET | `/api/dashboard` | Dashboard summary with latest per-user measurements |
| GET/POST | `/api/users` | List or create users |
| GET/PUT/DELETE | `/api/users/{id}` | Get, update, or delete a user |
| GET | `/api/measurements` | Query measurements (`?user_id=&days=`) |
| POST | `/api/measurements/{id}/assign` | Manually assign measurement to user |
| POST | `/api/ml/train` | Trigger ML model training |
| GET | `/api/ml/status` | Training status and model metrics |
| GET | `/api/analytics/daily` | Daily weight averages |
| GET | `/api/analytics/summary` | User summary with stats |
| GET | `/api/habits/insights` | Habit analysis with recommendations |

## License

MIT
