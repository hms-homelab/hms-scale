#pragma once
#include "database/IScaleDatabase.h"
#include <string>
#include <memory>
#include <pqxx/pqxx>

class PostgresScaleDatabase : public IScaleDatabase {
public:
    explicit PostgresScaleDatabase(const std::string& connection_string);
    ~PostgresScaleDatabase() override = default;

    bool connect() override;

    // Users
    std::vector<ScaleUser> getUsers(bool active_only = true) override;
    std::optional<ScaleUser> getUser(const std::string& id) override;
    std::optional<ScaleUser> createUser(const ScaleUser& user) override;
    bool updateUser(const std::string& id, const nlohmann::json& fields) override;
    bool deleteUser(const std::string& id) override;
    std::vector<ScaleUser> getUsersByWeightRange(double weight_kg) override;

    // Measurements
    std::optional<ScaleMeasurement> createMeasurement(const ScaleMeasurement& m) override;
    std::vector<ScaleMeasurement> getMeasurements(const std::string& user_id, int days = 30, int limit = 100, int offset = 0) override;
    std::optional<ScaleMeasurement> getLatestByUser(const std::string& user_id) override;
    std::vector<ScaleMeasurement> getUnassigned(int limit = 50) override;
    bool assignMeasurement(const std::string& measurement_id, const std::string& user_id, double confidence = 100.0) override;
    int getMeasurementCount(const std::string& user_id) override;

    // Analytics
    std::vector<DailyAverage> getDailyAverages(const std::string& user_id, int days = 30) override;
    std::vector<WeeklyTrend> getWeeklyTrends(const std::string& user_id, int weeks = 12) override;

    // ML
    std::vector<ScaleMeasurement> getMeasurementsForML(int min_per_user = 20) override;
    bool updateExpectedWeight(const std::string& user_id, double weight_kg) override;

private:
    std::string conn_string_;
    std::unique_ptr<pqxx::connection> conn_;

    // Templates accept both pqxx 7.x (field/row) and pqxx 8.x (field_ref/row_ref)
    template<typename F>
    std::string fieldOrEmpty(const F& f) const {
        return f.is_null() ? "" : f.template as<std::string>();
    }

    template<typename R>
    ScaleUser rowToUser(const R& r) const {
        ScaleUser u;
        u.id = fieldOrEmpty(r["id"]);
        u.name = fieldOrEmpty(r["name"]);
        u.date_of_birth = fieldOrEmpty(r["date_of_birth"]);
        u.sex = fieldOrEmpty(r["sex"]);
        u.height_cm = r["height_cm"].is_null() ? 0 : r["height_cm"].template as<double>();
        u.expected_weight_kg = r["expected_weight_kg"].is_null() ? 0 : r["expected_weight_kg"].template as<double>();
        u.weight_tolerance_kg = r["weight_tolerance_kg"].is_null() ? 3.0 : r["weight_tolerance_kg"].template as<double>();
        u.is_active = r["is_active"].is_null() ? true : r["is_active"].template as<bool>();
        u.created_at = fieldOrEmpty(r["created_at"]);
        u.updated_at = fieldOrEmpty(r["updated_at"]);
        u.last_measurement_at = fieldOrEmpty(r["last_measurement_at"]);
        return u;
    }

    template<typename R>
    ScaleMeasurement rowToMeasurement(const R& r) const {
        ScaleMeasurement m;
        m.id = fieldOrEmpty(r["id"]);
        m.user_id = fieldOrEmpty(r["user_id"]);
        m.weight_kg = r["weight_kg"].is_null() ? 0 : r["weight_kg"].template as<double>();
        m.weight_lbs = r["weight_lbs"].is_null() ? 0 : r["weight_lbs"].template as<double>();
        m.impedance_ohm = r["impedance_ohm"].is_null() ? 0 : r["impedance_ohm"].template as<double>();
        m.composition.body_fat_percentage = r["body_fat_percentage"].is_null() ? 0 : r["body_fat_percentage"].template as<double>();
        m.composition.lean_mass_kg = r["lean_mass_kg"].is_null() ? 0 : r["lean_mass_kg"].template as<double>();
        m.composition.muscle_mass_kg = r["muscle_mass_kg"].is_null() ? 0 : r["muscle_mass_kg"].template as<double>();
        m.composition.bone_mass_kg = r["bone_mass_kg"].is_null() ? 0 : r["bone_mass_kg"].template as<double>();
        m.composition.body_water_percentage = r["body_water_percentage"].is_null() ? 0 : r["body_water_percentage"].template as<double>();
        m.composition.visceral_fat_rating = r["visceral_fat_rating"].is_null() ? 0 : r["visceral_fat_rating"].template as<int>();
        m.composition.bmi = r["bmi"].is_null() ? 0 : r["bmi"].template as<double>();
        m.composition.bmr_kcal = r["bmr_kcal"].is_null() ? 0 : r["bmr_kcal"].template as<int>();
        m.composition.metabolic_age = r["metabolic_age"].is_null() ? 0 : r["metabolic_age"].template as<int>();
        m.composition.protein_percentage = r["protein_percentage"].is_null() ? 0 : r["protein_percentage"].template as<double>();
        m.identification_confidence = r["identification_confidence"].is_null() ? 0 : r["identification_confidence"].template as<double>();
        m.identification_method = fieldOrEmpty(r["identification_method"]);
        m.measured_at = fieldOrEmpty(r["measured_at"]);
        m.created_at = fieldOrEmpty(r["created_at"]);
        return m;
    }
};
