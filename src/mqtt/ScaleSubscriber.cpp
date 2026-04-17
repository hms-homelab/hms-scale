#include "mqtt/ScaleSubscriber.h"
#include "identification/HybridEngine.h"
#include "calculation/BodyCompositionCalculator.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace hms_colada {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
ScaleSubscriber::ScaleSubscriber(hms::MqttClient* mqtt, IScaleDatabase* db,
                                 DiscoveryPublisher* discovery,
                                 HybridEngine* identifier,
                                 BodyCompositionCalculator* calculator)
    : mqtt_(mqtt), db_(db), discovery_(discovery),
      identifier_(identifier), calculator_(calculator) {}

// ---------------------------------------------------------------------------
// start — subscribe to MQTT topics
// ---------------------------------------------------------------------------
void ScaleSubscriber::start() {
    spdlog::info("ScaleSubscriber: subscribing to giraffe_scale/measurement");
    mqtt_->subscribe("giraffe_scale/measurement",
        [this](const std::string& topic, const std::string& payload) {
            onMeasurement(topic, payload);
        });

    spdlog::info("ScaleSubscriber: subscribing to colada_scale/user_selector/set");
    mqtt_->subscribe("colada_scale/user_selector/set",
        [this](const std::string& topic, const std::string& payload) {
            onUserSelector(topic, payload);
        });
}

// ---------------------------------------------------------------------------
// parseMeasurement — static, testable JSON parsing + validation
// ---------------------------------------------------------------------------
ScaleSubscriber::ParsedMeasurement ScaleSubscriber::parseMeasurement(const std::string& payload) {
    ParsedMeasurement result;

    try {
        auto data = nlohmann::json::parse(payload);

        // Required: weight_kg
        if (!data.contains("weight_kg")) {
            spdlog::error("Missing required field: weight_kg");
            return result;
        }

        // Required: weight_lb or weight_lbs
        if (!data.contains("weight_lb") && !data.contains("weight_lbs")) {
            spdlog::error("Missing required field: weight_lb or weight_lbs");
            return result;
        }

        // Required: impedance
        if (!data.contains("impedance")) {
            spdlog::error("Missing required field: impedance");
            return result;
        }

        double weight_kg = data["weight_kg"].get<double>();
        double weight_lbs = data.contains("weight_lb")
            ? data["weight_lb"].get<double>()
            : data["weight_lbs"].get<double>();
        double impedance = data["impedance"].get<double>();

        // Validate ranges
        if (weight_kg <= 0 || weight_kg > 500) {
            spdlog::error("Invalid weight_kg: {}", weight_kg);
            return result;
        }

        if (weight_lbs <= 0 || weight_lbs > 1100) {
            spdlog::error("Invalid weight_lbs: {}", weight_lbs);
            return result;
        }

        if (impedance < 0 || impedance > 3000) {
            spdlog::error("Invalid impedance: {}", impedance);
            return result;
        }

        result.weight_kg = weight_kg;
        result.weight_lbs = weight_lbs;
        // Treat impedance 0 as "no BIA" (keep as 0)
        result.impedance_ohm = impedance;
        result.valid = true;

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse measurement JSON: {}", e.what());
    }

    return result;
}

// ---------------------------------------------------------------------------
// onMeasurement — MQTT callback
// ---------------------------------------------------------------------------
void ScaleSubscriber::onMeasurement(const std::string& topic, const std::string& payload) {
    spdlog::info("Received scale measurement on {}", topic);

    auto parsed = parseMeasurement(payload);
    if (!parsed.valid) {
        spdlog::warn("Invalid measurement data, skipping");
        return;
    }

    processMeasurement(parsed.weight_kg, parsed.weight_lbs, parsed.impedance_ohm);
}

// ---------------------------------------------------------------------------
// onUserSelector — echo selected user name back to state topic
// ---------------------------------------------------------------------------
void ScaleSubscriber::onUserSelector(const std::string& /*topic*/, const std::string& payload) {
    std::string selected = payload;
    // Trim whitespace
    while (!selected.empty() && std::isspace(static_cast<unsigned char>(selected.front())))
        selected.erase(selected.begin());
    while (!selected.empty() && std::isspace(static_cast<unsigned char>(selected.back())))
        selected.pop_back();

    spdlog::info("User selector changed to: {}", selected);

    if (discovery_) {
        discovery_->publishUserSelector(selected);
    }
}

// ---------------------------------------------------------------------------
// processMeasurement — delegates to processMeasurementWithResult
// ---------------------------------------------------------------------------
void ScaleSubscriber::processMeasurement(double weight_kg, double weight_lbs,
                                          double impedance_ohm) {
    processMeasurementWithResult(weight_kg, weight_lbs, impedance_ohm);
}

// ---------------------------------------------------------------------------
// processMeasurementWithResult — full pipeline, returns identification info
// ---------------------------------------------------------------------------
ScaleSubscriber::ProcessResult ScaleSubscriber::processMeasurementWithResult(
    double weight_kg, double weight_lbs, double impedance_ohm) {
    std::lock_guard<std::mutex> lock(process_mutex_);

    ProcessResult result;

    // Current timestamp in ISO 8601
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
    gmtime_r(&time_t_now, &tm_now);
    std::ostringstream ts;
    ts << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%SZ");
    std::string timestamp = ts.str();

    // Step 1: Identify user
    std::optional<IdentificationResult> ident;
    std::string user_id;
    std::string user_name;
    double confidence = 0;
    std::string method;

    if (identifier_) {
        try {
            ident = identifier_->identify(weight_kg, impedance_ohm, timestamp);
            if (ident) {
                user_id = ident->user_id;
                user_name = ident->user_name;
                confidence = ident->confidence;
                method = ident->method;
                spdlog::info("User identified: {} (method: {}, confidence: {}%)",
                             user_name, method, confidence);

                result.user_name = user_name;
                result.confidence = confidence;
                result.method = method;
                result.identified = true;
            } else {
                spdlog::info("No user identified - storing as unassigned");
            }
        } catch (const std::exception& e) {
            spdlog::error("User identification failed: {}", e.what());
        }
    }

    // Step 2: Calculate body composition (only if user identified AND impedance > 0)
    BodyCompositionResult composition;
    bool has_composition = false;

    if (!user_id.empty() && impedance_ohm > 0 && calculator_) {
        try {
            auto user_opt = db_->getUser(user_id);
            if (user_opt) {
                const auto& user = *user_opt;
                // Calculate age from date_of_birth
                int age = 30; // fallback
                if (!user.date_of_birth.empty()) {
                    std::tm dob{};
                    std::istringstream dob_ss(user.date_of_birth);
                    dob_ss >> std::get_time(&dob, "%Y-%m-%d");
                    if (!dob_ss.fail()) {
                        age = tm_now.tm_year - dob.tm_year;
                        if (tm_now.tm_mon < dob.tm_mon ||
                            (tm_now.tm_mon == dob.tm_mon && tm_now.tm_mday < dob.tm_mday)) {
                            age--;
                        }
                    }
                }

                composition = calculator_->calculate(
                    weight_kg, user.height_cm, impedance_ohm, age, user.sex);
                has_composition = true;

                spdlog::info("Body composition: BF={:.1f}%, Muscle={:.1f}kg, "
                             "BMI={:.1f}, BMR={}kcal",
                             composition.body_fat_percentage,
                             composition.muscle_mass_kg,
                             composition.bmi,
                             composition.bmr_kcal);
            }
        } catch (const std::exception& e) {
            spdlog::error("BIA calculation failed: {}", e.what());
        }
    } else if (!user_id.empty() && impedance_ohm <= 0) {
        spdlog::info("Skipping BIA calculation: no impedance");
    }

    // Step 3: Build and store measurement
    ScaleMeasurement measurement;
    measurement.user_id = user_id;
    measurement.weight_kg = weight_kg;
    measurement.weight_lbs = weight_lbs;
    measurement.impedance_ohm = impedance_ohm;
    measurement.identification_confidence = confidence;
    measurement.identification_method = method;
    measurement.measured_at = timestamp;

    if (has_composition) {
        measurement.composition = composition;
    }

    try {
        auto stored = db_->createMeasurement(measurement);
        if (stored) {
            spdlog::info("Measurement stored with ID: {} (user_id: {})",
                         stored->id, user_id.empty() ? "unassigned" : user_id);
        } else {
            spdlog::error("Failed to store measurement");
        }
    } catch (const std::exception& e) {
        spdlog::error("Database error storing measurement: {}", e.what());
    }

    // Step 4: Update expected weight (7-day rolling average)
    if (!user_id.empty()) {
        try {
            // Get last 7 days of measurements for rolling average
            auto recent = db_->getMeasurements(user_id, 7, 100, 0);
            if (!recent.empty()) {
                double sum = 0;
                for (const auto& m : recent) {
                    sum += m.weight_kg;
                }
                double avg = sum / static_cast<double>(recent.size());
                db_->updateExpectedWeight(user_id, avg);
                spdlog::debug("Updated expected weight for {}: {:.2f}kg ({} measurements)",
                              user_name, avg, recent.size());
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to update expected weight: {}", e.what());
        }
    }

    // Step 5: Publish to Home Assistant
    if (!user_id.empty() && discovery_) {
        try {
            auto user_opt = db_->getUser(user_id);
            if (user_opt) {
                discovery_->publishMeasurementState(*user_opt, measurement);
                spdlog::info("Published measurement to HA for {}", user_name);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to publish to HA: {}", e.what());
        }
    }

    return result;
}

}  // namespace hms_colada
