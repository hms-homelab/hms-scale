#pragma once

#include "mqtt_client.h"
#include "database/IScaleDatabase.h"
#include "mqtt/DiscoveryPublisher.h"
#include <mutex>
#include <string>

namespace hms_colada {

// Forward declarations (built by other agents)
class HybridEngine;
class BodyCompositionCalculator;

/**
 * ScaleSubscriber - MQTT subscriber for incoming scale measurements
 *
 * Subscribes to:
 *   giraffe_scale/measurement         — weight + impedance JSON from ESP32
 *   colada_scale/user_selector/set    — user selector commands from HA
 *
 * Processing pipeline (onMeasurement):
 *   1. Parse JSON (weight_kg, weight_lb/weight_lbs, impedance)
 *   2. Validate ranges (weight 0-500, impedance 0-3000)
 *   3. Identify user via HybridEngine
 *   4. Calculate body composition if user + impedance > 0
 *   5. Store measurement in database
 *   6. Update expected weight (7-day rolling avg)
 *   7. Publish state to HA via DiscoveryPublisher
 */
class ScaleSubscriber {
public:
    ScaleSubscriber(hms::MqttClient* mqtt, IScaleDatabase* db,
                    DiscoveryPublisher* discovery,
                    HybridEngine* identifier = nullptr,
                    BodyCompositionCalculator* calculator = nullptr);

    /// Subscribe to scale topics and start processing
    void start();

    /// Set optional services (can be wired after construction)
    void setIdentifier(HybridEngine* engine) { identifier_ = engine; }
    void setCalculator(BodyCompositionCalculator* calc) { calculator_ = calc; }

    /// Result from processing a measurement (returned by webhook path)
    struct ProcessResult {
        std::string user_name;
        double confidence = 0;
        std::string method;
        bool identified = false;
    };

    /// Process a measurement and return identification info (thread-safe)
    ProcessResult processMeasurementWithResult(double weight_kg, double weight_lbs, double impedance_ohm);

    // Exposed for testing
    struct ParsedMeasurement {
        double weight_kg = 0;
        double weight_lbs = 0;
        double impedance_ohm = 0;  // 0 = no BIA
        bool valid = false;
    };

    /// Parse and validate a measurement JSON payload. Static for testability.
    static ParsedMeasurement parseMeasurement(const std::string& payload);

private:
    /// MQTT callback for giraffe_scale/measurement
    void onMeasurement(const std::string& topic, const std::string& payload);

    /// MQTT callback for colada_scale/user_selector/set
    void onUserSelector(const std::string& topic, const std::string& payload);

    /// Core processing pipeline after parsing
    void processMeasurement(double weight_kg, double weight_lbs, double impedance_ohm);

    hms::MqttClient* mqtt_;
    IScaleDatabase* db_;
    DiscoveryPublisher* discovery_;
    HybridEngine* identifier_;
    BodyCompositionCalculator* calculator_;
    std::mutex process_mutex_;
};

}  // namespace hms_colada
