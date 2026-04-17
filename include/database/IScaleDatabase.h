#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "models/ScaleModels.h"

class IScaleDatabase {
public:
    virtual ~IScaleDatabase() = default;
    virtual bool connect() = 0;

    // Users
    virtual std::vector<ScaleUser> getUsers(bool active_only = true) = 0;
    virtual std::optional<ScaleUser> getUser(const std::string& id) = 0;
    virtual std::optional<ScaleUser> createUser(const ScaleUser& user) = 0;
    virtual bool updateUser(const std::string& id, const nlohmann::json& fields) = 0;
    virtual bool deleteUser(const std::string& id) = 0; // soft delete
    virtual std::vector<ScaleUser> getUsersByWeightRange(double weight_kg) = 0;

    // Measurements
    virtual std::optional<ScaleMeasurement> createMeasurement(const ScaleMeasurement& m) = 0;
    virtual std::vector<ScaleMeasurement> getMeasurements(const std::string& user_id, int days = 30, int limit = 100, int offset = 0) = 0;
    virtual std::optional<ScaleMeasurement> getLatestByUser(const std::string& user_id) = 0;
    virtual std::vector<ScaleMeasurement> getUnassigned(int limit = 50) = 0;
    virtual bool assignMeasurement(const std::string& measurement_id, const std::string& user_id, double confidence = 100.0) = 0;
    virtual int getMeasurementCount(const std::string& user_id) = 0;

    // Analytics
    virtual std::vector<DailyAverage> getDailyAverages(const std::string& user_id, int days = 30) = 0;
    virtual std::vector<WeeklyTrend> getWeeklyTrends(const std::string& user_id, int weeks = 12) = 0;

    // ML
    virtual std::vector<ScaleMeasurement> getMeasurementsForML(int min_per_user = 20) = 0;
    virtual bool updateExpectedWeight(const std::string& user_id, double weight_kg) = 0;
};
