#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "database/IScaleDatabase.h"
#include "models/ScaleModels.h"

namespace hms_colada {

struct ConsistencyMetrics {
    int total_measurements = 0;
    double avg_days_between = 0;
    int longest_gap_days = 0;
    double consistency_score = 0;  // 0-100
    int max_streak_days = 0;
    int current_streak_days = 0;
};

struct WeightTrend {
    double current_weight = 0;
    double starting_weight = 0;
    double total_change_kg = 0;
    double min_weight = 0;
    double max_weight = 0;
    double weight_range = 0;
    std::string trend_direction;  // "losing", "gaining", "stable"
    double slope_kg_per_measurement = 0;
    double volatility = 0;  // std dev
    bool is_volatile = false;  // > 1.5kg
    double avg_7d = 0;
    double avg_30d = 0;
};

struct BodyCompTrend {
    double body_fat_change = 0;
    double muscle_change_kg = 0;
    std::string body_fat_trend;   // "decreasing", "increasing", "stable"
    std::string muscle_trend;     // "gaining", "losing", "stable"
    bool is_recomposing = false;  // losing fat AND gaining muscle
};

struct WeeklyPattern {
    std::map<std::string, double> day_averages;  // "Monday" -> avg_weight
    std::string heaviest_day;
    std::string lightest_day;
    double weekly_variation_kg = 0;
    bool has_weekend_effect = false;  // variation > 1.0kg
};

struct Prediction {
    double predicted_weight_7d = 0;
    double predicted_weight_30d = 0;
    double predicted_weight_90d = 0;
    double rate_kg_per_week = 0;
    double rate_lbs_per_week = 0;
    std::string trend_confidence;  // "high" (>30), "medium" (>14), "low"
};

struct Recommendation {
    std::string category;      // "consistency", "stability", "achievement", "pattern", "warning"
    std::string priority;      // "high", "medium", "positive"
    std::string title;
    std::string message;
    std::string actionable_step;
};

struct Alert {
    std::string type;      // "gap", "volatility"
    std::string severity;  // "warning", "caution"
    std::string message;
};

struct HabitInsights {
    ConsistencyMetrics consistency;
    WeightTrend weight;
    BodyCompTrend body_comp;
    WeeklyPattern weekly;
    Prediction predictions;
    std::vector<Recommendation> recommendations;
    std::vector<Alert> alerts;
};

// Publishable metrics for HA MQTT
struct PublishableHabitMetrics {
    double consistency_score = 0;
    int current_streak_days = 0;
    std::string weight_trend_direction;
    double weight_change_kg = 0;
    double predicted_weight_7d = 0;
    double predicted_weight_30d = 0;
    double rate_kg_per_week = 0;
    std::string body_fat_trend;
    std::string muscle_trend;
    bool is_recomposing = false;
    std::string heaviest_day;
    std::string lightest_day;
};

class HabitAnalyzer {
public:
    explicit HabitAnalyzer(IScaleDatabase* db);

    // Full insights for a user
    HabitInsights getInsights(const std::string& user_id, int days = 90);

    // Just the publishable metrics for MQTT
    PublishableHabitMetrics getPublishableMetrics(const std::string& user_id, int days = 90);

    // Individual analysis components
    ConsistencyMetrics analyzeConsistency(const std::vector<ScaleMeasurement>& measurements);
    WeightTrend analyzeWeightTrend(const std::vector<ScaleMeasurement>& measurements);
    BodyCompTrend analyzeBodyCompTrend(const std::vector<ScaleMeasurement>& measurements);
    WeeklyPattern analyzeWeeklyPattern(const std::vector<ScaleMeasurement>& measurements);
    Prediction makePredictions(const std::vector<ScaleMeasurement>& measurements);
    std::vector<Recommendation> generateRecommendations(const HabitInsights& insights);
    std::vector<Alert> generateAlerts(const HabitInsights& insights);

    // Exposed for testing
    static double linearRegressionSlope(const std::vector<double>& x, const std::vector<double>& y);
    static double stddev(const std::vector<double>& values);

private:
    // Parse ISO timestamp to day of week (0=Monday, 6=Sunday)
    static int dayOfWeek(const std::string& timestamp);
    static std::string dayName(int dow);

    // Days between two ISO timestamps
    static double daysBetween(const std::string& ts1, const std::string& ts2);

    IScaleDatabase* db_;
};

}  // namespace hms_colada
