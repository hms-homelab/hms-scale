#include "database/PostgresScaleDatabase.h"
#include <spdlog/spdlog.h>
#include <sstream>

PostgresScaleDatabase::PostgresScaleDatabase(const std::string& connection_string)
    : conn_string_(connection_string) {}

bool PostgresScaleDatabase::connect() {
    try {
        conn_ = std::make_unique<pqxx::connection>(conn_string_);
        if (conn_->is_open()) {
            spdlog::info("Connected to PostgreSQL: {}", conn_->dbname());
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("PostgreSQL connection failed: {}", e.what());
        return false;
    }
}

std::string PostgresScaleDatabase::fieldOrEmpty(const pqxx::field& f) const {
    return f.is_null() ? "" : std::string(f.c_str());
}

ScaleUser PostgresScaleDatabase::rowToUser(const pqxx::row& r) const {
    ScaleUser u;
    u.id = fieldOrEmpty(r["id"]);
    u.name = fieldOrEmpty(r["name"]);
    u.date_of_birth = fieldOrEmpty(r["date_of_birth"]);
    u.sex = fieldOrEmpty(r["sex"]);
    u.height_cm = r["height_cm"].is_null() ? 0 : r["height_cm"].as<double>();
    u.expected_weight_kg = r["expected_weight_kg"].is_null() ? 0 : r["expected_weight_kg"].as<double>();
    u.weight_tolerance_kg = r["weight_tolerance_kg"].is_null() ? 3.0 : r["weight_tolerance_kg"].as<double>();
    u.is_active = r["is_active"].is_null() ? true : r["is_active"].as<bool>();
    u.created_at = fieldOrEmpty(r["created_at"]);
    u.updated_at = fieldOrEmpty(r["updated_at"]);
    u.last_measurement_at = fieldOrEmpty(r["last_measurement_at"]);
    return u;
}

ScaleMeasurement PostgresScaleDatabase::rowToMeasurement(const pqxx::row& r) const {
    ScaleMeasurement m;
    m.id = fieldOrEmpty(r["id"]);
    m.user_id = fieldOrEmpty(r["user_id"]);
    m.weight_kg = r["weight_kg"].is_null() ? 0 : r["weight_kg"].as<double>();
    m.weight_lbs = r["weight_lbs"].is_null() ? 0 : r["weight_lbs"].as<double>();
    m.impedance_ohm = r["impedance_ohm"].is_null() ? 0 : r["impedance_ohm"].as<double>();
    m.composition.body_fat_percentage = r["body_fat_percentage"].is_null() ? 0 : r["body_fat_percentage"].as<double>();
    m.composition.lean_mass_kg = r["lean_mass_kg"].is_null() ? 0 : r["lean_mass_kg"].as<double>();
    m.composition.muscle_mass_kg = r["muscle_mass_kg"].is_null() ? 0 : r["muscle_mass_kg"].as<double>();
    m.composition.bone_mass_kg = r["bone_mass_kg"].is_null() ? 0 : r["bone_mass_kg"].as<double>();
    m.composition.body_water_percentage = r["body_water_percentage"].is_null() ? 0 : r["body_water_percentage"].as<double>();
    m.composition.visceral_fat_rating = r["visceral_fat_rating"].is_null() ? 0 : r["visceral_fat_rating"].as<int>();
    m.composition.bmi = r["bmi"].is_null() ? 0 : r["bmi"].as<double>();
    m.composition.bmr_kcal = r["bmr_kcal"].is_null() ? 0 : r["bmr_kcal"].as<int>();
    m.composition.metabolic_age = r["metabolic_age"].is_null() ? 0 : r["metabolic_age"].as<int>();
    m.composition.protein_percentage = r["protein_percentage"].is_null() ? 0 : r["protein_percentage"].as<double>();
    m.identification_confidence = r["identification_confidence"].is_null() ? 0 : r["identification_confidence"].as<double>();
    m.identification_method = fieldOrEmpty(r["identification_method"]);
    m.measured_at = fieldOrEmpty(r["measured_at"]);
    m.created_at = fieldOrEmpty(r["created_at"]);
    return m;
}

// ── Users ──────────────────────────────────────────────────────────────────────

std::vector<ScaleUser> PostgresScaleDatabase::getUsers(bool active_only) {
    std::vector<ScaleUser> users;
    try {
        pqxx::work txn(*conn_);
        std::string sql = "SELECT * FROM scale_users";
        if (active_only) sql += " WHERE is_active = true";
        sql += " ORDER BY name";
        auto result = txn.exec(sql);
        for (const auto& r : result) {
            users.push_back(rowToUser(r));
        }
        txn.commit();
    } catch (const std::exception& e) {
        spdlog::error("getUsers failed: {}", e.what());
    }
    return users;
}

std::optional<ScaleUser> PostgresScaleDatabase::getUser(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT * FROM scale_users WHERE id = $1", id);
        txn.commit();
        if (result.empty()) return std::nullopt;
        return rowToUser(result[0]);
    } catch (const std::exception& e) {
        spdlog::error("getUser failed: {}", e.what());
        return std::nullopt;
    }
}

std::optional<ScaleUser> PostgresScaleDatabase::createUser(const ScaleUser& user) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "INSERT INTO scale_users (id, name, date_of_birth, sex, height_cm, "
            "expected_weight_kg, weight_tolerance_kg, is_active) "
            "VALUES (gen_random_uuid(), $1, $2, $3, $4, $5, $6, $7) RETURNING *",
            user.name, user.date_of_birth, user.sex, user.height_cm,
            user.expected_weight_kg, user.weight_tolerance_kg, user.is_active);
        txn.commit();
        if (result.empty()) return std::nullopt;
        return rowToUser(result[0]);
    } catch (const std::exception& e) {
        spdlog::error("createUser failed: {}", e.what());
        return std::nullopt;
    }
}

bool PostgresScaleDatabase::updateUser(const std::string& id, const nlohmann::json& fields) {
    try {
        // Build SET clause dynamically from JSON fields
        std::vector<std::string> sets;
        int param_idx = 1;
        // We'll build the query with string concatenation for field names
        // but use parameterized values via a manual approach
        pqxx::work txn(*conn_);

        // Simple approach: update each field individually
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            std::string col = it.key();
            // Whitelist allowed columns
            if (col != "name" && col != "date_of_birth" && col != "sex" &&
                col != "height_cm" && col != "expected_weight_kg" &&
                col != "weight_tolerance_kg" && col != "is_active") {
                continue;
            }

            std::string sql = "UPDATE scale_users SET " + col + " = $1, updated_at = NOW() WHERE id = $2";
            if (it.value().is_string()) {
                txn.exec_params(sql, it.value().get<std::string>(), id);
            } else if (it.value().is_number_float()) {
                txn.exec_params(sql, it.value().get<double>(), id);
            } else if (it.value().is_boolean()) {
                txn.exec_params(sql, it.value().get<bool>(), id);
            } else if (it.value().is_number_integer()) {
                txn.exec_params(sql, it.value().get<int>(), id);
            }
        }
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("updateUser failed: {}", e.what());
        return false;
    }
}

bool PostgresScaleDatabase::deleteUser(const std::string& id) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE scale_users SET is_active = false, updated_at = NOW() WHERE id = $1", id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("deleteUser (soft) failed: {}", e.what());
        return false;
    }
}

std::vector<ScaleUser> PostgresScaleDatabase::getUsersByWeightRange(double weight_kg) {
    std::vector<ScaleUser> users;
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT * FROM scale_users "
            "WHERE is_active = true AND ABS(expected_weight_kg - $1) <= weight_tolerance_kg "
            "ORDER BY ABS(expected_weight_kg - $1)",
            weight_kg);
        txn.commit();
        for (const auto& r : result) {
            users.push_back(rowToUser(r));
        }
    } catch (const std::exception& e) {
        spdlog::error("getUsersByWeightRange failed: {}", e.what());
    }
    return users;
}

// ── Measurements ───────────────────────────────────────────────────────────────

std::optional<ScaleMeasurement> PostgresScaleDatabase::createMeasurement(const ScaleMeasurement& m) {
    try {
        pqxx::work txn(*conn_);

        // Use nullptr for empty user_id (unassigned measurement)
        std::string sql =
            "INSERT INTO scale_measurements ("
            "id, user_id, weight_kg, weight_lbs, impedance_ohm, "
            "body_fat_percentage, lean_mass_kg, muscle_mass_kg, bone_mass_kg, "
            "body_water_percentage, visceral_fat_rating, bmi, bmr_kcal, "
            "metabolic_age, protein_percentage, identification_confidence, "
            "identification_method, measured_at"
            ") VALUES ("
            "gen_random_uuid(), $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17"
            ") RETURNING *";

        pqxx::result result;
        if (m.user_id.empty()) {
            result = txn.exec_params(sql,
                nullptr, m.weight_kg, m.weight_lbs, m.impedance_ohm,
                m.composition.body_fat_percentage, m.composition.lean_mass_kg,
                m.composition.muscle_mass_kg, m.composition.bone_mass_kg,
                m.composition.body_water_percentage, m.composition.visceral_fat_rating,
                m.composition.bmi, m.composition.bmr_kcal,
                m.composition.metabolic_age, m.composition.protein_percentage,
                m.identification_confidence, m.identification_method,
                m.measured_at.empty() ? "NOW()" : m.measured_at);
        } else {
            result = txn.exec_params(sql,
                m.user_id, m.weight_kg, m.weight_lbs, m.impedance_ohm,
                m.composition.body_fat_percentage, m.composition.lean_mass_kg,
                m.composition.muscle_mass_kg, m.composition.bone_mass_kg,
                m.composition.body_water_percentage, m.composition.visceral_fat_rating,
                m.composition.bmi, m.composition.bmr_kcal,
                m.composition.metabolic_age, m.composition.protein_percentage,
                m.identification_confidence, m.identification_method,
                m.measured_at.empty() ? "NOW()" : m.measured_at);
        }

        // Update user's last_measurement_at if assigned
        if (!m.user_id.empty()) {
            txn.exec_params(
                "UPDATE scale_users SET last_measurement_at = NOW() WHERE id = $1",
                m.user_id);
        }

        txn.commit();
        if (result.empty()) return std::nullopt;
        return rowToMeasurement(result[0]);
    } catch (const std::exception& e) {
        spdlog::error("createMeasurement failed: {}", e.what());
        return std::nullopt;
    }
}

std::vector<ScaleMeasurement> PostgresScaleDatabase::getMeasurements(
    const std::string& user_id, int days, int limit, int offset) {
    std::vector<ScaleMeasurement> measurements;
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT * FROM scale_measurements "
            "WHERE user_id = $1 AND measured_at >= NOW() - ($2 || ' days')::interval "
            "ORDER BY measured_at DESC LIMIT $3 OFFSET $4",
            user_id, days, limit, offset);
        txn.commit();
        for (const auto& r : result) {
            measurements.push_back(rowToMeasurement(r));
        }
    } catch (const std::exception& e) {
        spdlog::error("getMeasurements failed: {}", e.what());
    }
    return measurements;
}

std::optional<ScaleMeasurement> PostgresScaleDatabase::getLatestByUser(const std::string& user_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT * FROM scale_measurements "
            "WHERE user_id = $1 ORDER BY measured_at DESC LIMIT 1",
            user_id);
        txn.commit();
        if (result.empty()) return std::nullopt;
        return rowToMeasurement(result[0]);
    } catch (const std::exception& e) {
        spdlog::error("getLatestByUser failed: {}", e.what());
        return std::nullopt;
    }
}

std::vector<ScaleMeasurement> PostgresScaleDatabase::getUnassigned(int limit) {
    std::vector<ScaleMeasurement> measurements;
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT * FROM scale_measurements "
            "WHERE user_id IS NULL ORDER BY measured_at DESC LIMIT $1",
            limit);
        txn.commit();
        for (const auto& r : result) {
            measurements.push_back(rowToMeasurement(r));
        }
    } catch (const std::exception& e) {
        spdlog::error("getUnassigned failed: {}", e.what());
    }
    return measurements;
}

bool PostgresScaleDatabase::assignMeasurement(
    const std::string& measurement_id, const std::string& user_id, double confidence) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE scale_measurements SET user_id = $1, identification_confidence = $2 "
            "WHERE id = $3",
            user_id, confidence, measurement_id);
        txn.exec_params(
            "UPDATE scale_users SET last_measurement_at = NOW() WHERE id = $1",
            user_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("assignMeasurement failed: {}", e.what());
        return false;
    }
}

int PostgresScaleDatabase::getMeasurementCount(const std::string& user_id) {
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT COUNT(*) FROM scale_measurements WHERE user_id = $1",
            user_id);
        txn.commit();
        return result[0][0].as<int>();
    } catch (const std::exception& e) {
        spdlog::error("getMeasurementCount failed: {}", e.what());
        return 0;
    }
}

// ── Analytics ──────────────────────────────────────────────────────────────────

std::vector<DailyAverage> PostgresScaleDatabase::getDailyAverages(
    const std::string& user_id, int days) {
    std::vector<DailyAverage> averages;
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT DATE(measured_at) as date, "
            "AVG(weight_kg) as avg_weight_kg, "
            "MIN(weight_kg) as min_weight_kg, "
            "MAX(weight_kg) as max_weight_kg, "
            "AVG(body_fat_percentage) as avg_body_fat, "
            "COUNT(*) as measurement_count "
            "FROM scale_measurements "
            "WHERE user_id = $1 AND measured_at >= NOW() - ($2 || ' days')::interval "
            "GROUP BY DATE(measured_at) ORDER BY date DESC",
            user_id, days);
        txn.commit();
        for (const auto& r : result) {
            DailyAverage da;
            da.date = fieldOrEmpty(r["date"]);
            da.user_id = user_id;
            da.avg_weight_kg = r["avg_weight_kg"].as<double>();
            da.min_weight_kg = r["min_weight_kg"].as<double>();
            da.max_weight_kg = r["max_weight_kg"].as<double>();
            da.avg_body_fat = r["avg_body_fat"].is_null() ? 0 : r["avg_body_fat"].as<double>();
            da.measurement_count = r["measurement_count"].as<int>();
            averages.push_back(da);
        }
    } catch (const std::exception& e) {
        spdlog::error("getDailyAverages failed: {}", e.what());
    }
    return averages;
}

std::vector<WeeklyTrend> PostgresScaleDatabase::getWeeklyTrends(
    const std::string& user_id, int weeks) {
    std::vector<WeeklyTrend> trends;
    try {
        pqxx::work txn(*conn_);
        auto result = txn.exec_params(
            "SELECT DATE_TRUNC('week', measured_at)::date as week_start, "
            "AVG(weight_kg) as avg_weight_kg, "
            "AVG(body_fat_percentage) as avg_body_fat, "
            "COUNT(*) as measurement_count "
            "FROM scale_measurements "
            "WHERE user_id = $1 AND measured_at >= NOW() - ($2 || ' weeks')::interval "
            "GROUP BY DATE_TRUNC('week', measured_at) ORDER BY week_start DESC",
            user_id, weeks);
        txn.commit();

        double prev_weight = 0;
        // Process in reverse (oldest first) for weight_change calculation
        std::vector<pqxx::row> rows(result.begin(), result.end());
        for (int i = static_cast<int>(rows.size()) - 1; i >= 0; --i) {
            const auto& r = rows[i];
            WeeklyTrend wt;
            wt.week_start = fieldOrEmpty(r["week_start"]);
            wt.user_id = user_id;
            wt.avg_weight_kg = r["avg_weight_kg"].as<double>();
            wt.avg_body_fat = r["avg_body_fat"].is_null() ? 0 : r["avg_body_fat"].as<double>();
            wt.measurement_count = r["measurement_count"].as<int>();
            wt.weight_change_kg = (prev_weight > 0) ? wt.avg_weight_kg - prev_weight : 0;
            prev_weight = wt.avg_weight_kg;
            trends.push_back(wt);
        }
        // Reverse back to newest-first order
        std::reverse(trends.begin(), trends.end());
    } catch (const std::exception& e) {
        spdlog::error("getWeeklyTrends failed: {}", e.what());
    }
    return trends;
}

// ── ML ─────────────────────────────────────────────────────────────────────────

std::vector<ScaleMeasurement> PostgresScaleDatabase::getMeasurementsForML(int min_per_user) {
    std::vector<ScaleMeasurement> measurements;
    try {
        pqxx::work txn(*conn_);
        // Only include measurements from users with enough data points
        auto result = txn.exec_params(
            "SELECT m.* FROM scale_measurements m "
            "INNER JOIN ("
            "  SELECT user_id FROM scale_measurements "
            "  WHERE user_id IS NOT NULL "
            "  GROUP BY user_id HAVING COUNT(*) >= $1"
            ") q ON m.user_id = q.user_id "
            "ORDER BY m.user_id, m.measured_at",
            min_per_user);
        txn.commit();
        for (const auto& r : result) {
            measurements.push_back(rowToMeasurement(r));
        }
    } catch (const std::exception& e) {
        spdlog::error("getMeasurementsForML failed: {}", e.what());
    }
    return measurements;
}

bool PostgresScaleDatabase::updateExpectedWeight(const std::string& user_id, double weight_kg) {
    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "UPDATE scale_users SET expected_weight_kg = $1, updated_at = NOW() WHERE id = $2",
            weight_kg, user_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("updateExpectedWeight failed: {}", e.what());
        return false;
    }
}
