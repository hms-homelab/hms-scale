#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "utils/AppConfig.h"
#include "models/ScaleModels.h"
#include "database/PostgresScaleDatabase.h"
#include <filesystem>
#include <fstream>

// ── AppConfig Tests ────────────────────────────────────────────────────────────

TEST(AppConfigTest, DefaultValues) {
    AppConfig config;
    EXPECT_EQ(config.web_port, 8889);
    EXPECT_EQ(config.database.host, "localhost");
    EXPECT_EQ(config.database.port, 5432);
    EXPECT_EQ(config.database.name, "colada_scale");
    EXPECT_EQ(config.database.user, "colada_user");
    EXPECT_TRUE(config.database.password.empty());
    EXPECT_TRUE(config.mqtt.enabled);
    EXPECT_EQ(config.mqtt.broker, "localhost");
    EXPECT_EQ(config.mqtt.port, 1883);
    EXPECT_EQ(config.mqtt.client_id, "hms_colada");
    EXPECT_EQ(config.mqtt.scale_topic, "giraffe_scale/measurement");
    EXPECT_FALSE(config.ml_training.enabled);
    EXPECT_EQ(config.ml_training.schedule, "weekly");
    EXPECT_EQ(config.ml_training.min_measurements, 20);
    EXPECT_FALSE(config.setup_complete);
}

TEST(AppConfigTest, ConnectionString) {
    AppConfig config;
    config.database.host = "db.example.com";
    config.database.port = 5433;
    config.database.name = "testdb";
    config.database.user = "testuser";
    config.database.password = "secret";

    std::string cs = config.connectionString();
    EXPECT_NE(cs.find("host=db.example.com"), std::string::npos);
    EXPECT_NE(cs.find("port=5433"), std::string::npos);
    EXPECT_NE(cs.find("dbname=testdb"), std::string::npos);
    EXPECT_NE(cs.find("user=testuser"), std::string::npos);
    EXPECT_NE(cs.find("password=secret"), std::string::npos);
}

TEST(AppConfigTest, JsonRoundTrip) {
    std::string tmp_path = "/tmp/hms_colada_test_config.json";

    // Create and save config
    AppConfig original;
    original.web_port = 9999;
    original.database.host = "testhost";
    original.database.port = 5555;
    original.database.name = "testdb";
    original.database.user = "testuser";
    original.database.password = "testpass";
    original.mqtt.enabled = false;
    original.mqtt.broker = "mqtt.test.com";
    original.mqtt.scale_topic = "test/scale";
    original.ml_training.enabled = true;
    original.ml_training.schedule = "daily";
    original.ml_training.min_measurements = 50;
    original.setup_complete = true;
    ASSERT_TRUE(original.save(tmp_path));

    // Load and verify
    AppConfig loaded;
    ASSERT_TRUE(AppConfig::load(tmp_path, loaded));
    EXPECT_EQ(loaded.web_port, 9999);
    EXPECT_EQ(loaded.database.host, "testhost");
    EXPECT_EQ(loaded.database.port, 5555);
    EXPECT_EQ(loaded.database.name, "testdb");
    EXPECT_EQ(loaded.database.user, "testuser");
    EXPECT_EQ(loaded.database.password, "testpass");
    EXPECT_FALSE(loaded.mqtt.enabled);
    EXPECT_EQ(loaded.mqtt.broker, "mqtt.test.com");
    EXPECT_EQ(loaded.mqtt.scale_topic, "test/scale");
    EXPECT_TRUE(loaded.ml_training.enabled);
    EXPECT_EQ(loaded.ml_training.schedule, "daily");
    EXPECT_EQ(loaded.ml_training.min_measurements, 50);
    EXPECT_TRUE(loaded.setup_complete);

    // Cleanup
    std::filesystem::remove(tmp_path);
}

TEST(AppConfigTest, ToJsonRedactsPasswords) {
    AppConfig config;
    config.database.password = "secret";
    config.mqtt.password = "mqttpass";

    auto j = config.toJson();
    EXPECT_EQ(j["database"]["password"], "********");
    EXPECT_EQ(j["mqtt"]["password"], "********");
}

TEST(AppConfigTest, LoadNonexistentFile) {
    AppConfig config;
    EXPECT_FALSE(AppConfig::load("/tmp/nonexistent_config_12345.json", config));
}

// ── ScaleModels Tests ──────────────────────────────────────────────────────────

TEST(ScaleModelsTest, ScaleUserJsonRoundTrip) {
    ScaleUser user;
    user.id = "test-uuid-1234";
    user.name = "Test User";
    user.date_of_birth = "1990-05-15";
    user.sex = "male";
    user.height_cm = 175.5;
    user.expected_weight_kg = 80.0;
    user.weight_tolerance_kg = 3.0;
    user.is_active = true;
    user.created_at = "2026-01-01T00:00:00Z";
    user.updated_at = "2026-01-02T00:00:00Z";
    user.last_measurement_at = "2026-01-03T00:00:00Z";

    nlohmann::json j = user;
    ScaleUser restored = j.get<ScaleUser>();

    EXPECT_EQ(restored.id, "test-uuid-1234");
    EXPECT_EQ(restored.name, "Test User");
    EXPECT_EQ(restored.date_of_birth, "1990-05-15");
    EXPECT_EQ(restored.sex, "male");
    EXPECT_DOUBLE_EQ(restored.height_cm, 175.5);
    EXPECT_DOUBLE_EQ(restored.expected_weight_kg, 80.0);
    EXPECT_DOUBLE_EQ(restored.weight_tolerance_kg, 3.0);
    EXPECT_TRUE(restored.is_active);
}

TEST(ScaleModelsTest, ScaleMeasurementJsonRoundTrip) {
    ScaleMeasurement m;
    m.id = "meas-uuid-5678";
    m.user_id = "user-uuid-1234";
    m.weight_kg = 82.5;
    m.weight_lbs = 181.88;
    m.impedance_ohm = 485.0;
    m.composition.body_fat_percentage = 18.5;
    m.composition.lean_mass_kg = 67.2;
    m.composition.muscle_mass_kg = 63.8;
    m.composition.bone_mass_kg = 3.4;
    m.composition.body_water_percentage = 55.2;
    m.composition.visceral_fat_rating = 8;
    m.composition.bmi = 26.8;
    m.composition.bmr_kcal = 1850;
    m.composition.metabolic_age = 32;
    m.composition.protein_percentage = 17.5;
    m.identification_confidence = 95.0;
    m.identification_method = "weight_match";
    m.measured_at = "2026-01-03T08:30:00Z";
    m.created_at = "2026-01-03T08:30:01Z";

    nlohmann::json j = m;
    ScaleMeasurement restored = j.get<ScaleMeasurement>();

    EXPECT_EQ(restored.id, "meas-uuid-5678");
    EXPECT_EQ(restored.user_id, "user-uuid-1234");
    EXPECT_DOUBLE_EQ(restored.weight_kg, 82.5);
    EXPECT_DOUBLE_EQ(restored.weight_lbs, 181.88);
    EXPECT_DOUBLE_EQ(restored.impedance_ohm, 485.0);
    EXPECT_DOUBLE_EQ(restored.composition.body_fat_percentage, 18.5);
    EXPECT_EQ(restored.composition.visceral_fat_rating, 8);
    EXPECT_EQ(restored.composition.bmr_kcal, 1850);
    EXPECT_EQ(restored.identification_method, "weight_match");
}

TEST(ScaleModelsTest, DailyAverageJson) {
    DailyAverage da;
    da.date = "2026-01-03";
    da.user_id = "user-1";
    da.avg_weight_kg = 82.3;
    da.min_weight_kg = 81.9;
    da.max_weight_kg = 82.7;
    da.avg_body_fat = 18.2;
    da.measurement_count = 3;

    nlohmann::json j = da;
    DailyAverage restored = j.get<DailyAverage>();

    EXPECT_EQ(restored.date, "2026-01-03");
    EXPECT_DOUBLE_EQ(restored.avg_weight_kg, 82.3);
    EXPECT_EQ(restored.measurement_count, 3);
}

TEST(ScaleModelsTest, WeeklyTrendJson) {
    WeeklyTrend wt;
    wt.week_start = "2026-01-01";
    wt.user_id = "user-1";
    wt.avg_weight_kg = 82.1;
    wt.weight_change_kg = -0.5;
    wt.avg_body_fat = 18.0;
    wt.measurement_count = 14;

    nlohmann::json j = wt;
    WeeklyTrend restored = j.get<WeeklyTrend>();

    EXPECT_EQ(restored.week_start, "2026-01-01");
    EXPECT_DOUBLE_EQ(restored.weight_change_kg, -0.5);
    EXPECT_EQ(restored.measurement_count, 14);
}

// ── PostgresScaleDatabase Tests (integration, skipped if no DB) ────────────────

class PostgresDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Skip if DB_HOST env not set (unit test mode)
        const char* host = std::getenv("DB_HOST");
        if (!host) {
            GTEST_SKIP() << "DB_HOST not set, skipping integration tests";
        }

        AppConfig config;
        config.applyEnvFallbacks();
        db_ = std::make_unique<PostgresScaleDatabase>(config.connectionString());
    }

    std::unique_ptr<PostgresScaleDatabase> db_;
};

TEST_F(PostgresDatabaseTest, ConnectToDatabase) {
    ASSERT_TRUE(db_->connect());
}

TEST_F(PostgresDatabaseTest, GetUsersDoesNotThrow) {
    ASSERT_TRUE(db_->connect());
    auto users = db_->getUsers();
    // Just verify it doesn't throw — may return empty
    SUCCEED();
}

// ── Connection resilience ──────────────────────────────────────────────────────
//
// Regression test for the bug where PostgresScaleDatabase held ONE pqxx
// connection created in connect() and every one of its 16 query paths used it
// directly. When Postgres dropped that connection (restart / idle timeout /
// network blip) every query failed FOREVER — the live service logged
// "getMeasurementsForML failed: Lost connection to the database server" once a
// minute for weeks while the database sat there healthy with 857 measurements.
//
// Credentials come from the environment so nothing secret lands in the repo;
// the test skips when they are absent or the server is unreachable.

namespace {

std::string reconnectTestConnString() {
    auto env = [](const char* k, const char* dflt) {
        const char* v = std::getenv(k);
        return std::string((v && *v) ? v : dflt);
    };
    const char* pw = std::getenv("COLADA_DB_PASSWORD");
    if (!pw || !*pw) return "";
    return "host=" + env("COLADA_DB_HOST", "localhost") +
           " port=" + env("COLADA_DB_PORT", "5432") +
           " dbname=" + env("COLADA_DB_NAME", "colada_scale") +
           " user=" + env("COLADA_DB_USER", "colada_user") +
           " password=" + std::string(pw) +
           " application_name=colada_reconnect_test";
}

} // namespace

TEST(PostgresReconnectTest, RecoversAfterServerDropsConnection) {
    const std::string cs = reconnectTestConnString();
    if (cs.empty()) GTEST_SKIP() << "COLADA_DB_PASSWORD not set - skipping live DB test";

    PostgresScaleDatabase db(cs);
    if (!db.connect()) GTEST_SKIP() << "PostgreSQL not reachable - skipping";

    const auto before = db.getUsers(false);
    if (before.empty()) GTEST_SKIP() << "no users in the database to assert against";

    // Drop THIS object's connection server-side, from a separate session. Scoped
    // by application_name so only our own backend dies, never the live service's.
    try {
        pqxx::connection killer(cs);
        pqxx::work t(killer);
        t.exec("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
               "WHERE application_name = 'colada_reconnect_test' "
               "AND pid <> pg_backend_pid()");
        t.commit();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "could not terminate backend: " << e.what();
    }

    // libpqxx only notices a dead socket on I/O, so the first call is the one
    // that discovers it and is allowed to fail; the next MUST have reconnected.
    (void)db.getUsers(false);
    const auto after = db.getUsers(false);

    EXPECT_EQ(after.size(), before.size())
        << "connection did not recover after the server dropped it - "
           "db() failed to reconnect";
}

TEST(PostgresReconnectTest, SurvivesRepeatedDrops) {
    const std::string cs = reconnectTestConnString();
    if (cs.empty()) GTEST_SKIP() << "COLADA_DB_PASSWORD not set - skipping live DB test";

    PostgresScaleDatabase db(cs);
    if (!db.connect()) GTEST_SKIP() << "PostgreSQL not reachable - skipping";
    const auto before = db.getUsers(false);
    if (before.empty()) GTEST_SKIP() << "no users in the database to assert against";

    // The old code recovered zero times; make sure recovery is repeatable and
    // not a one-shot (e.g. a reconnect that leaves conn_ in a bad state).
    for (int i = 0; i < 3; ++i) {
        try {
            pqxx::connection killer(cs);
            pqxx::work t(killer);
            t.exec("SELECT pg_terminate_backend(pid) FROM pg_stat_activity "
                   "WHERE application_name = 'colada_reconnect_test' "
                   "AND pid <> pg_backend_pid()");
            t.commit();
        } catch (const std::exception&) {
            GTEST_SKIP() << "could not terminate backend";
        }
        (void)db.getUsers(false);
        EXPECT_EQ(db.getUsers(false).size(), before.size())
            << "did not recover on drop #" << (i + 1);
    }
}
