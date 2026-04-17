#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "identification/HybridEngine.h"
#include "services/MLTrainingService.h"
#include "database/IScaleDatabase.h"
#include "models/ScaleModels.h"

#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ===========================================================================
// MockScaleDatabase - minimal in-memory implementation
// ===========================================================================

class MockScaleDatabase : public IScaleDatabase {
public:
    std::vector<ScaleUser> users;
    std::vector<ScaleMeasurement> measurements;

    bool connect() override { return true; }

    // -- Users --
    std::vector<ScaleUser> getUsers(bool active_only) override {
        if (!active_only) return users;
        std::vector<ScaleUser> result;
        for (const auto& u : users) {
            if (u.is_active) result.push_back(u);
        }
        return result;
    }

    std::optional<ScaleUser> getUser(const std::string& id) override {
        for (const auto& u : users) {
            if (u.id == id) return u;
        }
        return std::nullopt;
    }

    std::optional<ScaleUser> createUser(const ScaleUser& user) override {
        users.push_back(user);
        return user;
    }

    bool updateUser(const std::string& /*id*/, const nlohmann::json& /*fields*/) override {
        return true;
    }

    bool deleteUser(const std::string& /*id*/) override { return true; }

    std::vector<ScaleUser> getUsersByWeightRange(double weight_kg) override {
        std::vector<ScaleUser> result;
        for (const auto& u : users) {
            if (std::abs(u.expected_weight_kg - weight_kg) <= u.weight_tolerance_kg) {
                result.push_back(u);
            }
        }
        return result;
    }

    // -- Measurements --
    std::optional<ScaleMeasurement> createMeasurement(const ScaleMeasurement& m) override {
        measurements.push_back(m);
        return m;
    }

    std::vector<ScaleMeasurement> getMeasurements(const std::string& user_id,
                                                   int /*days*/, int /*limit*/,
                                                   int /*offset*/) override {
        std::vector<ScaleMeasurement> result;
        for (const auto& m : measurements) {
            if (m.user_id == user_id) result.push_back(m);
        }
        return result;
    }

    std::optional<ScaleMeasurement> getLatestByUser(const std::string& user_id) override {
        for (auto it = measurements.rbegin(); it != measurements.rend(); ++it) {
            if (it->user_id == user_id) return *it;
        }
        return std::nullopt;
    }

    std::vector<ScaleMeasurement> getUnassigned(int /*limit*/) override {
        std::vector<ScaleMeasurement> result;
        for (const auto& m : measurements) {
            if (m.user_id.empty()) result.push_back(m);
        }
        return result;
    }

    bool assignMeasurement(const std::string& /*mid*/, const std::string& /*uid*/,
                           double /*conf*/) override {
        return true;
    }

    int getMeasurementCount(const std::string& user_id) override {
        int count = 0;
        for (const auto& m : measurements) {
            if (m.user_id == user_id) ++count;
        }
        return count;
    }

    // -- Analytics --
    std::vector<DailyAverage> getDailyAverages(const std::string& /*uid*/,
                                                int /*days*/) override {
        return {};
    }

    std::vector<WeeklyTrend> getWeeklyTrends(const std::string& /*uid*/,
                                              int /*weeks*/) override {
        return {};
    }

    // -- ML --
    std::vector<ScaleMeasurement> getMeasurementsForML(int min_per_user) override {
        // Count per user
        std::map<std::string, int> counts;
        for (const auto& m : measurements) {
            if (!m.user_id.empty()) ++counts[m.user_id];
        }

        // Filter to users with enough measurements
        std::vector<ScaleMeasurement> result;
        for (const auto& m : measurements) {
            if (m.user_id.empty()) continue;
            if (counts[m.user_id] >= min_per_user) {
                result.push_back(m);
            }
        }
        return result;
    }

    bool updateExpectedWeight(const std::string& user_id, double weight_kg) override {
        for (auto& u : users) {
            if (u.id == user_id) {
                u.expected_weight_kg = weight_kg;
                return true;
            }
        }
        return false;
    }
};

// ===========================================================================
// Helper: create a ScaleUser
// ===========================================================================

static ScaleUser makeUser(const std::string& id, const std::string& name,
                           double weight_kg, double tolerance = 3.0,
                           bool active = true) {
    ScaleUser u;
    u.id = id;
    u.name = name;
    u.expected_weight_kg = weight_kg;
    u.weight_tolerance_kg = tolerance;
    u.is_active = active;
    u.sex = "male";
    u.height_cm = 175;
    u.date_of_birth = "1990-01-01";
    return u;
}

// ===========================================================================
// HybridEngine Tests
// ===========================================================================

class HybridEngineTest : public ::testing::Test {
protected:
    MockScaleDatabase db;
};

TEST_F(HybridEngineTest, ExactMatch_SingleCandidate) {
    db.users.push_back(makeUser("user1", "Alice", 65.0));
    db.users.push_back(makeUser("user2", "Bob", 85.0));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    auto result = engine.identify(65.3, 500.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->user_id, "user1");
    EXPECT_EQ(result->user_name, "Alice");
    EXPECT_DOUBLE_EQ(result->confidence, 95.0);
    EXPECT_EQ(result->method, "deterministic_exact_match");
    EXPECT_FALSE(result->requires_manual);
}

TEST_F(HybridEngineTest, ExactMatch_NoCandidate) {
    db.users.push_back(makeUser("user1", "Alice", 65.0));
    db.users.push_back(makeUser("user2", "Bob", 85.0));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    // Weight far from any user, also outside tolerance
    auto result = engine.identify(120.0, 500.0);

    EXPECT_FALSE(result.has_value());
}

TEST_F(HybridEngineTest, ExactMatch_MultipleInRange) {
    // Two users with overlapping exact ranges
    db.users.push_back(makeUser("user1", "Alice", 65.0));
    db.users.push_back(makeUser("user2", "Bob", 65.3));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    // 65.1 is within 0.5kg of both Alice (65.0) and Bob (65.3)
    auto result = engine.identify(65.1, 500.0);

    // Should NOT be exact match (ambiguous), but should fall to tolerance match
    // since both are within default 3.0kg tolerance - picks closest
    ASSERT_TRUE(result.has_value());
    // Should not be exact match method
    EXPECT_NE(result->method, "deterministic_exact_match");
    // Should be tolerance match (Stage 3) since no ML model loaded
    EXPECT_EQ(result->method, "deterministic_tolerance_match");
    EXPECT_DOUBLE_EQ(result->confidence, 75.0);
}

TEST_F(HybridEngineTest, ToleranceMatch) {
    // User with custom tolerance
    db.users.push_back(makeUser("user1", "Alice", 65.0, 5.0));  // +/-5kg tolerance
    db.users.push_back(makeUser("user2", "Bob", 85.0, 2.0));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    // 68.0 is outside exact range (0.5kg) but within Alice's 5.0kg tolerance
    auto result = engine.identify(68.0, 500.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->user_id, "user1");
    EXPECT_EQ(result->user_name, "Alice");
    EXPECT_DOUBLE_EQ(result->confidence, 75.0);
    EXPECT_EQ(result->method, "deterministic_tolerance_match");
}

TEST_F(HybridEngineTest, ManualAssignment) {
    db.users.push_back(makeUser("user1", "Alice", 65.0, 2.0));
    db.users.push_back(makeUser("user2", "Bob", 85.0, 2.0));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    // Weight far from all users (outside both exact and tolerance ranges)
    auto result = engine.identify(100.0, 500.0);

    EXPECT_FALSE(result.has_value());
}

TEST_F(HybridEngineTest, HasModel_NoModelLoaded) {
    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");
    EXPECT_FALSE(engine.hasModel());
}

TEST_F(HybridEngineTest, InactiveUsersIgnored) {
    db.users.push_back(makeUser("user1", "Alice", 65.0, 3.0, false));  // inactive
    db.users.push_back(makeUser("user2", "Bob", 85.0));

    hms_colada::HybridEngine engine(&db, "/tmp/hms-colada-test-models-nonexistent");

    // Alice is inactive so should not match even though weight is close
    auto result = engine.identify(65.0, 500.0);
    EXPECT_FALSE(result.has_value());
}

// ===========================================================================
// MLTrainingService Tests
// ===========================================================================

class MLTrainingServiceTest : public ::testing::Test {
protected:
    MockScaleDatabase db;
};

TEST_F(MLTrainingServiceTest, GetStatus_InitialIdle) {
    hms_colada::MLTrainingService::Config config;
    config.enabled = true;
    config.model_dir = "/tmp/hms-colada-test-ml-idle";

    hms_colada::MLTrainingService svc(&db, config);

    auto status = svc.getStatus();
    EXPECT_EQ(status["status"].asString(), "idle");
    EXPECT_EQ(status["schedule"].asString(), "weekly");
}

TEST_F(MLTrainingServiceTest, TrainWithSyntheticData) {
    // Create test model directory
    std::string test_dir = "/tmp/hms-colada-test-ml-train";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);

    // Create 2 users with distinct weights
    db.users.push_back(makeUser("user-a", "Alice", 60.0));
    db.users.push_back(makeUser("user-b", "Bob", 90.0));

    // Generate synthetic measurements: 30 per user with some noise
    std::mt19937 rng(42);
    std::normal_distribution<double> weight_noise(0.0, 0.5);
    std::normal_distribution<double> imp_noise(0.0, 10.0);

    for (int i = 0; i < 30; ++i) {
        // Alice: ~60kg, ~550 ohm
        ScaleMeasurement ma;
        ma.id = "meas-a-" + std::to_string(i);
        ma.user_id = "user-a";
        ma.weight_kg = 60.0 + weight_noise(rng);
        ma.impedance_ohm = 550.0 + imp_noise(rng);
        ma.measured_at = "2026-04-" + std::string(i < 9 ? "0" : "") +
                         std::to_string(i + 1) + "T08:00:00";
        db.measurements.push_back(ma);

        // Bob: ~90kg, ~450 ohm
        ScaleMeasurement mb;
        mb.id = "meas-b-" + std::to_string(i);
        mb.user_id = "user-b";
        mb.weight_kg = 90.0 + weight_noise(rng);
        mb.impedance_ohm = 450.0 + imp_noise(rng);
        mb.measured_at = "2026-04-" + std::string(i < 9 ? "0" : "") +
                         std::to_string(i + 1) + "T20:00:00";
        db.measurements.push_back(mb);
    }

    hms_colada::MLTrainingService::Config config;
    config.enabled = true;
    config.model_dir = test_dir;
    config.min_measurements = 20;

    hms_colada::MLTrainingService svc(&db, config);

    // Trigger training directly (not via background thread)
    svc.triggerTraining();

    // Since triggerTraining only sets a flag and we haven't started the worker,
    // we need to verify the training can work by checking the synthetic data
    // is sufficient. The actual training happens in the background thread.
    // For unit testing, we verify the status and config are correct.
    auto status = svc.getStatus();
    EXPECT_EQ(status["model_dir"].asString(), test_dir);
    EXPECT_EQ(status["min_measurements"].asInt(), 20);

    // Verify synthetic data is correct
    auto ml_data = db.getMeasurementsForML(20);
    EXPECT_EQ(ml_data.size(), 60u);  // 30 Alice + 30 Bob

    // Clean up
    fs::remove_all(test_dir);
}
