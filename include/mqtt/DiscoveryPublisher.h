#pragma once

#include "mqtt_client.h"
#include "models/ScaleModels.h"
#include <json/json.h>
#include <string>
#include <vector>

namespace hms_colada {

/**
 * DiscoveryPublisher - Home Assistant MQTT Discovery for Colada Scale
 *
 * Publishes per-user body composition sensors and system-level sensors
 * (status binary sensor, user selector) via HA MQTT Discovery protocol.
 *
 * Topic patterns:
 *   Discovery: homeassistant/sensor/colada_scale_{safe_name}/colada_scale_{safe_name}_{sensor_id}/config
 *   State:     colada_scale/{safe_name}/{sensor_id}
 *   Avail:     colada_scale/status  ("online" / "offline")
 */
class DiscoveryPublisher {
public:
    explicit DiscoveryPublisher(hms::MqttClient* mqtt);

    /// Publish HA MQTT discovery configs for a user's 12 measurement sensors
    void publishUserDiscovery(const ScaleUser& user);

    /// Publish system-level discovery (status binary sensor, user selector)
    void publishSystemDiscovery(const std::vector<ScaleUser>& users);

    /// Publish measurement state values for a user
    void publishMeasurementState(const ScaleUser& user, const ScaleMeasurement& measurement);

    /// Publish a single habit metric state value
    void publishHabitState(const std::string& safe_name,
                           const std::string& metric_name,
                           const std::string& value);

    /// Publish system availability ("online" / "offline")
    void publishAvailability(bool online);

    /// Publish user selector state (echo back selected user)
    void publishUserSelector(const std::string& selected_user);

    /// Republish all discovery configs (after MQTT reconnect)
    void republishAll(const std::vector<ScaleUser>& users);

    /// Convert display name to safe MQTT identifier (lowercase, spaces -> underscores)
    static std::string safeName(const std::string& name);

private:
    /// Publish a single sensor discovery config
    void publishSensorDiscovery(const std::string& safe_name,
                                const std::string& user_display_name,
                                const std::string& sensor_id,
                                const std::string& sensor_name,
                                const std::string& unit,
                                const std::string& device_class,
                                const std::string& icon,
                                const std::string& state_class = "measurement");

    /// Build device info JSON for a per-user device
    Json::Value buildUserDeviceInfo(const std::string& safe_name,
                                    const std::string& user_display_name) const;

    /// Build device info JSON for the system device
    Json::Value buildSystemDeviceInfo() const;

    /// Build discovery topic string
    static std::string discoveryTopic(const std::string& component,
                                       const std::string& node_id,
                                       const std::string& object_id);

    hms::MqttClient* mqtt_;
};

}  // namespace hms_colada
