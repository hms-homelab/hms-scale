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

    ScaleUser rowToUser(const pqxx::row& r) const;
    ScaleMeasurement rowToMeasurement(const pqxx::row& r) const;
    std::string fieldOrEmpty(const pqxx::field& f) const;
};
