#include "mqtt/DiscoveryPublisher.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>

namespace hms_colada {

// ---------------------------------------------------------------------------
// Sensor table: the 12 per-user measurement sensors
// ---------------------------------------------------------------------------
struct SensorDef {
    const char* id;
    const char* name_suffix;   // appended to user display name
    const char* unit;
    const char* device_class;
    const char* icon;
    const char* state_class;   // "" means omit
};

static const SensorDef kMeasurementSensors[] = {
    {"weight",           "Weight",           "kg",    "weight",    "mdi:scale-bathroom",   "measurement"},
    {"body_fat",         "Body Fat",         "%",     "",          "mdi:percent",          "measurement"},
    {"muscle_mass",      "Muscle Mass",      "kg",    "weight",    "mdi:arm-flex",         "measurement"},
    {"bmi",              "BMI",              "",      "",          "mdi:human",            "measurement"},
    {"bmr",              "BMR",              "kcal",  "",          "mdi:fire",             "measurement"},
    {"body_water",       "Body Water",       "%",     "",          "mdi:water-percent",    "measurement"},
    {"bone_mass",        "Bone Mass",        "kg",    "weight",    "mdi:bone",             "measurement"},
    {"visceral_fat",     "Visceral Fat",     "",      "",          "mdi:stomach",          "measurement"},
    {"metabolic_age",    "Metabolic Age",    "years", "",          "mdi:calendar-clock",   "measurement"},
    {"protein",          "Protein",          "%",     "",          "mdi:food-steak",       "measurement"},
    {"lean_mass",        "Lean Mass",        "kg",    "weight",    "mdi:arm-flex-outline",  "measurement"},
    {"last_measurement", "Last Measurement", "",      "timestamp", "mdi:clock-outline",    ""},
};

static constexpr int kSensorCount = sizeof(kMeasurementSensors) / sizeof(kMeasurementSensors[0]);

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
DiscoveryPublisher::DiscoveryPublisher(hms::MqttClient* mqtt)
    : mqtt_(mqtt) {}

// ---------------------------------------------------------------------------
// safeName: "Sabrina" -> "sabrina", "Test User" -> "test_user"
// ---------------------------------------------------------------------------
std::string DiscoveryPublisher::safeName(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (c == ' ') {
            result += '_';
        } else {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Topic helpers
// ---------------------------------------------------------------------------
std::string DiscoveryPublisher::discoveryTopic(const std::string& component,
                                                const std::string& node_id,
                                                const std::string& object_id) {
    // homeassistant/{component}/{node_id}/{object_id}/config
    return "homeassistant/" + component + "/" + node_id + "/" + object_id + "/config";
}

// ---------------------------------------------------------------------------
// Device info builders
// ---------------------------------------------------------------------------
Json::Value DiscoveryPublisher::buildUserDeviceInfo(const std::string& safe,
                                                     const std::string& display) const {
    Json::Value dev;
    dev["identifiers"].append("colada_scale_" + safe);
    dev["name"] = "Colada Scale - " + display;
    dev["manufacturer"] = "Colada";
    dev["model"] = "Multi-User Smart Scale";
    dev["sw_version"] = "1.0.0";
    return dev;
}

Json::Value DiscoveryPublisher::buildSystemDeviceInfo() const {
    Json::Value dev;
    dev["identifiers"].append("colada_scale_system");
    dev["name"] = "Colada Scale Service";
    dev["manufacturer"] = "Colada";
    dev["model"] = "Multi-User Smart Scale System";
    dev["sw_version"] = "1.0.0";
    return dev;
}

// ---------------------------------------------------------------------------
// Publish single sensor discovery config
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishSensorDiscovery(
        const std::string& safe,
        const std::string& user_display_name,
        const std::string& sensor_id,
        const std::string& sensor_name,
        const std::string& unit,
        const std::string& device_class,
        const std::string& icon,
        const std::string& state_class) {

    const std::string unique_id = "colada_scale_" + safe + "_" + sensor_id;
    const std::string state_topic = "colada_scale/" + safe + "/" + sensor_id;
    const std::string topic = discoveryTopic("sensor",
                                              "colada_scale_" + safe,
                                              unique_id);

    Json::Value config;
    config["name"] = user_display_name + " " + sensor_name;
    config["state_topic"] = state_topic;
    config["unique_id"] = unique_id;

    if (!unit.empty())         config["unit_of_measurement"] = unit;
    if (!device_class.empty()) config["device_class"] = device_class;
    if (!state_class.empty())  config["state_class"] = state_class;
    if (!icon.empty())         config["icon"] = icon;

    config["device"] = buildUserDeviceInfo(safe, user_display_name);
    config["availability_topic"] = "colada_scale/status";
    config["payload_available"] = "online";
    config["payload_not_available"] = "offline";

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string payload = Json::writeString(writer, config);

    mqtt_->publish(topic, payload, 1, true);
}

// ---------------------------------------------------------------------------
// publishUserDiscovery — 12 sensors per user
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishUserDiscovery(const ScaleUser& user) {
    const std::string safe = safeName(user.name);

    spdlog::info("Discovery: publishing {} sensor configs for {}", kSensorCount, user.name);

    for (int i = 0; i < kSensorCount; ++i) {
        const auto& s = kMeasurementSensors[i];
        publishSensorDiscovery(safe, user.name,
                               s.id, s.name_suffix,
                               s.unit, s.device_class,
                               s.icon, s.state_class);
    }
}

// ---------------------------------------------------------------------------
// publishSystemDiscovery — binary sensor + user selector
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishSystemDiscovery(const std::vector<ScaleUser>& users) {
    // 1. System status binary sensor
    {
        Json::Value config;
        config["name"] = "Colada Scale Status";
        config["unique_id"] = "colada_scale_system_status";
        config["state_topic"] = "colada_scale/status";
        config["payload_on"] = "online";
        config["payload_off"] = "offline";
        config["device_class"] = "connectivity";
        config["icon"] = "mdi:scale-bathroom";
        config["device"] = buildSystemDeviceInfo();

        const std::string topic = discoveryTopic("binary_sensor",
                                                  "colada_scale_system",
                                                  "colada_scale_system_status");
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string payload = Json::writeString(writer, config);

        mqtt_->publish(topic, payload, 1, true);
        spdlog::info("Discovery: published system status binary sensor");
    }

    // 2. User selector (select entity)
    {
        Json::Value options(Json::arrayValue);
        for (const auto& u : users) {
            if (u.is_active) {
                options.append(u.name);
            }
        }

        if (options.empty()) {
            spdlog::warn("Discovery: no active users for user selector");
            return;
        }

        Json::Value config;
        config["name"] = "Colada Scale User";
        config["unique_id"] = "colada_scale_user_selector";
        config["command_topic"] = "colada_scale/user_selector/set";
        config["state_topic"] = "colada_scale/user_selector/state";
        config["options"] = options;
        config["icon"] = "mdi:account-circle";
        config["device"] = buildSystemDeviceInfo();

        const std::string topic = discoveryTopic("select",
                                                  "colada_scale_system",
                                                  "colada_scale_user_selector");
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string payload = Json::writeString(writer, config);

        mqtt_->publish(topic, payload, 1, true);

        // Publish initial state (first active user)
        if (!options.empty()) {
            mqtt_->publish("colada_scale/user_selector/state",
                           options[0].asString(), 1, true);
        }

        spdlog::info("Discovery: published user selector with {} users", options.size());
    }
}

// ---------------------------------------------------------------------------
// publishMeasurementState — publish numeric values to state topics
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishMeasurementState(const ScaleUser& user,
                                                  const ScaleMeasurement& measurement) {
    const std::string safe = safeName(user.name);
    const std::string prefix = "colada_scale/" + safe + "/";
    const auto& c = measurement.composition;

    auto pub = [&](const std::string& sensor_id, const std::string& value) {
        mqtt_->publish(prefix + sensor_id, value, 0, true);
    };

    // Helper to format doubles — omit trailing zeros
    auto fmt = [](double v) -> std::string {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        // Trim trailing zeros after decimal
        std::string s(buf);
        auto dot = s.find('.');
        if (dot != std::string::npos) {
            auto last = s.find_last_not_of('0');
            if (last == dot) last++; // keep at least one decimal
            s.erase(last + 1);
        }
        return s;
    };

    pub("weight",           fmt(measurement.weight_kg));
    pub("body_fat",         fmt(c.body_fat_percentage));
    pub("muscle_mass",      fmt(c.muscle_mass_kg));
    pub("bmi",              fmt(c.bmi));
    pub("bmr",              std::to_string(c.bmr_kcal));
    pub("body_water",       fmt(c.body_water_percentage));
    pub("bone_mass",        fmt(c.bone_mass_kg));
    pub("visceral_fat",     std::to_string(c.visceral_fat_rating));
    pub("metabolic_age",    std::to_string(c.metabolic_age));
    pub("protein",          fmt(c.protein_percentage));
    pub("lean_mass",        fmt(c.lean_mass_kg));
    pub("last_measurement", measurement.measured_at);

    spdlog::debug("Published measurement state for {}", user.name);
}

// ---------------------------------------------------------------------------
// publishHabitState — single habit metric
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishHabitState(const std::string& safe,
                                            const std::string& metric_name,
                                            const std::string& value) {
    mqtt_->publish("colada_scale/" + safe + "/" + metric_name, value, 0, true);
}

// ---------------------------------------------------------------------------
// publishAvailability
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishAvailability(bool online) {
    const std::string status = online ? "online" : "offline";
    mqtt_->publish("colada_scale/status", status, 1, true);
    spdlog::info("Published availability: {}", status);
}

// ---------------------------------------------------------------------------
// publishUserSelector — echo selected user back to state topic
// ---------------------------------------------------------------------------
void DiscoveryPublisher::publishUserSelector(const std::string& selected_user) {
    mqtt_->publish("colada_scale/user_selector/state", selected_user, 1, true);
    spdlog::info("Published user selector state: {}", selected_user);
}

// ---------------------------------------------------------------------------
// republishAll — full discovery refresh (e.g., after reconnect)
// ---------------------------------------------------------------------------
void DiscoveryPublisher::republishAll(const std::vector<ScaleUser>& users) {
    spdlog::info("Discovery: republishing all configs for {} users", users.size());

    for (const auto& user : users) {
        if (user.is_active) {
            publishUserDiscovery(user);
        }
    }
    publishSystemDiscovery(users);
    publishAvailability(true);
}

}  // namespace hms_colada
