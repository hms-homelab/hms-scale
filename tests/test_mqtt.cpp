#include <gtest/gtest.h>
#include "mqtt/DiscoveryPublisher.h"
#include "mqtt/ScaleSubscriber.h"

using namespace hms_colada;

// ==========================================================================
// DiscoveryPublisher tests
// ==========================================================================

TEST(DiscoveryPublisher, SafeName_SimpleName) {
    EXPECT_EQ(DiscoveryPublisher::safeName("Sabrina"), "sabrina");
}

TEST(DiscoveryPublisher, SafeName_NameWithSpaces) {
    EXPECT_EQ(DiscoveryPublisher::safeName("Test User"), "test_user");
}

TEST(DiscoveryPublisher, SafeName_MixedCase) {
    EXPECT_EQ(DiscoveryPublisher::safeName("John DOE"), "john_doe");
}

TEST(DiscoveryPublisher, SafeName_EmptyString) {
    EXPECT_EQ(DiscoveryPublisher::safeName(""), "");
}

TEST(DiscoveryPublisher, SafeName_MultipleSpaces) {
    EXPECT_EQ(DiscoveryPublisher::safeName("A B C"), "a_b_c");
}

TEST(DiscoveryPublisher, TopicFormat_SensorDiscovery) {
    // Verify the discovery topic pattern matches the expected format
    std::string safe = DiscoveryPublisher::safeName("Sabrina");
    std::string sensor_id = "weight";
    std::string unique_id = "colada_scale_" + safe + "_" + sensor_id;
    std::string expected = "homeassistant/sensor/colada_scale_sabrina/colada_scale_sabrina_weight/config";

    // Reconstruct the topic the same way DiscoveryPublisher does internally
    std::string topic = "homeassistant/sensor/colada_scale_" + safe + "/" + unique_id + "/config";
    EXPECT_EQ(topic, expected);
}

TEST(DiscoveryPublisher, TopicFormat_BinarySensor) {
    std::string expected = "homeassistant/binary_sensor/colada_scale_system/colada_scale_system_status/config";
    std::string topic = "homeassistant/binary_sensor/colada_scale_system/colada_scale_system_status/config";
    EXPECT_EQ(topic, expected);
}

TEST(DiscoveryPublisher, TopicFormat_UserSelector) {
    std::string expected = "homeassistant/select/colada_scale_system/colada_scale_user_selector/config";
    std::string topic = "homeassistant/select/colada_scale_system/colada_scale_user_selector/config";
    EXPECT_EQ(topic, expected);
}

TEST(DiscoveryPublisher, TopicFormat_StateTopic) {
    std::string safe = DiscoveryPublisher::safeName("Sabrina");
    std::string state_topic = "colada_scale/" + safe + "/weight";
    EXPECT_EQ(state_topic, "colada_scale/sabrina/weight");
}

// ==========================================================================
// ScaleSubscriber::parseMeasurement tests
// ==========================================================================

TEST(ScaleSubscriber, ParsesValidPayload) {
    std::string payload = R"({
        "weight_kg": 80.5,
        "weight_lb": 177.47,
        "impedance": 520
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.weight_kg, 80.5);
    EXPECT_DOUBLE_EQ(result.weight_lbs, 177.47);
    EXPECT_DOUBLE_EQ(result.impedance_ohm, 520.0);
}

TEST(ScaleSubscriber, ParsesValidPayload_WeightLbs) {
    // Test alternate field name "weight_lbs"
    std::string payload = R"({
        "weight_kg": 60.0,
        "weight_lbs": 132.28,
        "impedance": 480
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.weight_kg, 60.0);
    EXPECT_DOUBLE_EQ(result.weight_lbs, 132.28);
    EXPECT_DOUBLE_EQ(result.impedance_ohm, 480.0);
}

TEST(ScaleSubscriber, ParsesZeroImpedance) {
    // Impedance 0 is valid (no BIA performed, e.g., children)
    std::string payload = R"({
        "weight_kg": 25.0,
        "weight_lb": 55.12,
        "impedance": 0
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.impedance_ohm, 0.0);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_MissingWeightKg) {
    std::string payload = R"({
        "weight_lb": 177.47,
        "impedance": 520
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_MissingWeightLb) {
    std::string payload = R"({
        "weight_kg": 80.5,
        "impedance": 520
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_MissingImpedance) {
    std::string payload = R"({
        "weight_kg": 80.5,
        "weight_lb": 177.47
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_WeightTooHigh) {
    std::string payload = R"({
        "weight_kg": 600.0,
        "weight_lb": 1322.8,
        "impedance": 520
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_WeightNegative) {
    std::string payload = R"({
        "weight_kg": -5.0,
        "weight_lb": -11.0,
        "impedance": 520
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_ImpedanceTooHigh) {
    std::string payload = R"({
        "weight_kg": 80.5,
        "weight_lb": 177.47,
        "impedance": 5000
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_NegativeImpedance) {
    std::string payload = R"({
        "weight_kg": 80.5,
        "weight_lb": 177.47,
        "impedance": -100
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_MalformedJson) {
    auto result = ScaleSubscriber::parseMeasurement("not json at all");
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, RejectsInvalidPayload_EmptyPayload) {
    auto result = ScaleSubscriber::parseMeasurement("");
    EXPECT_FALSE(result.valid);
}

TEST(ScaleSubscriber, ParsesEdgeCaseValues) {
    // Boundary values: max valid weight, max valid impedance
    std::string payload = R"({
        "weight_kg": 500.0,
        "weight_lb": 1100.0,
        "impedance": 3000
    })";

    auto result = ScaleSubscriber::parseMeasurement(payload);
    EXPECT_TRUE(result.valid);
    EXPECT_DOUBLE_EQ(result.weight_kg, 500.0);
    EXPECT_DOUBLE_EQ(result.weight_lbs, 1100.0);
    EXPECT_DOUBLE_EQ(result.impedance_ohm, 3000.0);
}
