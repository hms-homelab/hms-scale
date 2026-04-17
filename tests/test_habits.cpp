#include <gtest/gtest.h>

#include "analytics/HabitAnalyzer.h"

using namespace hms_colada;

// ---------------------------------------------------------------------------
// Helper: build a ScaleMeasurement with minimal fields
// ---------------------------------------------------------------------------
static ScaleMeasurement makeMeasurement(double weight_kg,
                                        const std::string& measured_at,
                                        double body_fat = 0,
                                        double muscle_kg = 0) {
    ScaleMeasurement m;
    m.weight_kg   = weight_kg;
    m.measured_at = measured_at;
    m.composition.body_fat_percentage = body_fat;
    m.composition.muscle_mass_kg      = muscle_kg;
    return m;
}

// Helper: generate N daily measurements starting from a base date
static std::vector<ScaleMeasurement> dailySeries(int count, double start_weight,
                                                  double delta_per_day = 0,
                                                  const std::string& base = "2026-03-01") {
    std::vector<ScaleMeasurement> out;
    // Parse base date
    int y, mon, d;
    std::sscanf(base.c_str(), "%d-%d-%d", &y, &mon, &d);

    for (int i = 0; i < count; ++i) {
        char ts[32];
        // Simple day increment (good enough for tests within a single month)
        std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT08:00:00", y, mon, d + i);
        out.push_back(makeMeasurement(start_weight + i * delta_per_day, ts));
    }
    return out;
}

// =============================================================================
// Linear Regression Tests
// =============================================================================

TEST(HabitAnalyzer, LinearRegression_PositiveSlope) {
    // y = 2x + 1  ->  slope = 2
    std::vector<double> x = {0, 1, 2, 3, 4};
    std::vector<double> y = {1, 3, 5, 7, 9};
    double slope = HabitAnalyzer::linearRegressionSlope(x, y);
    EXPECT_NEAR(slope, 2.0, 1e-6);
}

TEST(HabitAnalyzer, LinearRegression_Flat) {
    std::vector<double> x = {0, 1, 2, 3, 4};
    std::vector<double> y = {5, 5, 5, 5, 5};
    double slope = HabitAnalyzer::linearRegressionSlope(x, y);
    EXPECT_NEAR(slope, 0.0, 1e-6);
}

// =============================================================================
// Consistency Tests
// =============================================================================

TEST(HabitAnalyzer, Consistency_Daily) {
    // 5 measurements, 1 day apart each -> score = 100
    auto ms = dailySeries(5, 80.0);
    HabitAnalyzer analyzer(nullptr);
    auto c = analyzer.analyzeConsistency(ms);

    EXPECT_EQ(c.total_measurements, 5);
    EXPECT_NEAR(c.avg_days_between, 1.0, 0.01);
    EXPECT_DOUBLE_EQ(c.consistency_score, 100.0);
}

TEST(HabitAnalyzer, Consistency_Weekly) {
    // Measurements 7 days apart -> score should be low
    std::vector<ScaleMeasurement> ms;
    ms.push_back(makeMeasurement(80, "2026-03-01T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-08T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-15T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-22T08:00:00"));

    HabitAnalyzer analyzer(nullptr);
    auto c = analyzer.analyzeConsistency(ms);

    EXPECT_NEAR(c.avg_days_between, 7.0, 0.01);
    // score = 100 - (7-1)*20 = 100 - 120 = clamped to 0
    EXPECT_DOUBLE_EQ(c.consistency_score, 0.0);
}

// =============================================================================
// Streak Tests
// =============================================================================

TEST(HabitAnalyzer, Streak_Consecutive) {
    // 5 daily measurements -> streak = 5
    auto ms = dailySeries(5, 80.0);
    HabitAnalyzer analyzer(nullptr);
    auto c = analyzer.analyzeConsistency(ms);

    EXPECT_EQ(c.max_streak_days, 5);
    EXPECT_EQ(c.current_streak_days, 5);
}

TEST(HabitAnalyzer, Streak_WithGap) {
    // 3 daily, then 5-day gap, then 2 daily -> current streak = 2
    std::vector<ScaleMeasurement> ms;
    ms.push_back(makeMeasurement(80, "2026-03-01T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-02T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-03T08:00:00"));
    // 5-day gap
    ms.push_back(makeMeasurement(80, "2026-03-08T08:00:00"));
    ms.push_back(makeMeasurement(80, "2026-03-09T08:00:00"));

    HabitAnalyzer analyzer(nullptr);
    auto c = analyzer.analyzeConsistency(ms);

    EXPECT_EQ(c.max_streak_days, 3);
    EXPECT_EQ(c.current_streak_days, 2);
}

// =============================================================================
// Weight Trend Tests
// =============================================================================

TEST(HabitAnalyzer, WeightTrend_Gaining) {
    // Increasing weights: 80, 80.5, 81, 81.5, 82
    auto ms = dailySeries(5, 80.0, 0.5);
    HabitAnalyzer analyzer(nullptr);
    auto w = analyzer.analyzeWeightTrend(ms);

    EXPECT_EQ(w.trend_direction, "gaining");
    EXPECT_GT(w.slope_kg_per_measurement, 0.01);
    EXPECT_NEAR(w.current_weight, 82.0, 0.01);
    EXPECT_NEAR(w.starting_weight, 80.0, 0.01);
    EXPECT_NEAR(w.total_change_kg, 2.0, 0.01);
}

TEST(HabitAnalyzer, WeightTrend_Stable) {
    // Constant weight
    auto ms = dailySeries(5, 80.0, 0.0);
    HabitAnalyzer analyzer(nullptr);
    auto w = analyzer.analyzeWeightTrend(ms);

    EXPECT_EQ(w.trend_direction, "stable");
    EXPECT_NEAR(w.slope_kg_per_measurement, 0.0, 0.01);
}

// =============================================================================
// Weekly Pattern Tests
// =============================================================================

TEST(HabitAnalyzer, WeeklyPattern_HasData) {
    // 14 daily measurements -> covers multiple days of week
    auto ms = dailySeries(14, 80.0, 0.0);
    HabitAnalyzer analyzer(nullptr);
    auto wp = analyzer.analyzeWeeklyPattern(ms);

    EXPECT_FALSE(wp.heaviest_day.empty());
    EXPECT_FALSE(wp.lightest_day.empty());
    EXPECT_FALSE(wp.day_averages.empty());
}

// =============================================================================
// Prediction Tests
// =============================================================================

TEST(HabitAnalyzer, Predictions_Reasonable) {
    // 30 daily measurements gaining 0.1 kg/day
    auto ms = dailySeries(30, 80.0, 0.1);
    HabitAnalyzer analyzer(nullptr);
    auto p = analyzer.makePredictions(ms);

    // Current weight ~82.9, gaining ~0.7 kg/week
    // predicted_7d should be roughly current + rate
    EXPECT_GT(p.predicted_weight_7d, 80.0);
    EXPECT_LT(p.predicted_weight_7d, 100.0);
    EXPECT_GT(p.predicted_weight_30d, p.predicted_weight_7d);
    EXPECT_GT(p.predicted_weight_90d, p.predicted_weight_30d);
    EXPECT_GT(p.rate_kg_per_week, 0);
    EXPECT_EQ(p.trend_confidence, "medium");  // 30 measurements -> "medium" (>14 but not >30)
}

// =============================================================================
// Recommendations Tests
// =============================================================================

TEST(HabitAnalyzer, Recommendations_LowConsistency) {
    // Build insights with low consistency
    HabitInsights insights;
    insights.consistency.consistency_score = 40;
    insights.consistency.avg_days_between = 4.0;

    HabitAnalyzer analyzer(nullptr);
    auto recs = analyzer.generateRecommendations(insights);

    ASSERT_FALSE(recs.empty());
    bool found = false;
    for (auto& r : recs) {
        if (r.category == "consistency" && r.priority == "high") {
            found = true;
            EXPECT_EQ(r.title, "Weigh More Consistently");
        }
    }
    EXPECT_TRUE(found) << "Expected a consistency recommendation";
}
