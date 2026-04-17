#include "analytics/HabitAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace hms_colada {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Parse "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS" into std::tm
static bool parseTimestamp(const std::string& ts, std::tm& out) {
    std::memset(&out, 0, sizeof(out));
    int y, M, d, h, m, s;
    // Try space separator first (PostgreSQL format), then T (ISO 8601)
    if (std::sscanf(ts.c_str(), "%d-%d-%d %d:%d:%d", &y, &M, &d, &h, &m, &s) < 6 &&
        std::sscanf(ts.c_str(), "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s) < 6)
        return false;
    out.tm_year = y - 1900;
    out.tm_mon  = M - 1;
    out.tm_mday = d;
    out.tm_hour = h;
    out.tm_min  = m;
    out.tm_sec  = s;
    out.tm_isdst = 0;
    return true;
}

static std::time_t toEpoch(const std::tm& t) {
    std::tm copy = t;
    return timegm(&copy);
}

// ---------------------------------------------------------------------------
// Static math utilities
// ---------------------------------------------------------------------------

double HabitAnalyzer::linearRegressionSlope(const std::vector<double>& x,
                                             const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    int n = static_cast<int>(x.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for (int i = 0; i < n; ++i) {
        sum_x  += x[i];
        sum_y  += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }
    double denom = n * sum_xx - sum_x * sum_x;
    if (std::abs(denom) < 1e-10) return 0.0;
    return (n * sum_xy - sum_x * sum_y) / denom;
}

double HabitAnalyzer::stddev(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double sq_sum = 0;
    for (auto v : values) {
        double d = v - mean;
        sq_sum += d * d;
    }
    return std::sqrt(sq_sum / values.size());  // population stddev (matches numpy default)
}

int HabitAnalyzer::dayOfWeek(const std::string& timestamp) {
    std::tm t{};
    if (!parseTimestamp(timestamp, t)) return 0;
    std::time_t epoch = toEpoch(t);
    std::tm utc{};
    gmtime_r(&epoch, &utc);
    // tm_wday: 0=Sunday. Convert to 0=Monday.
    return (utc.tm_wday + 6) % 7;
}

std::string HabitAnalyzer::dayName(int dow) {
    static const char* names[] = {
        "Monday", "Tuesday", "Wednesday", "Thursday",
        "Friday", "Saturday", "Sunday"
    };
    if (dow < 0 || dow > 6) return "Unknown";
    return names[dow];
}

double HabitAnalyzer::daysBetween(const std::string& ts1, const std::string& ts2) {
    std::tm t1{}, t2{};
    if (!parseTimestamp(ts1, t1) || !parseTimestamp(ts2, t2)) return 0.0;
    double diff = std::difftime(toEpoch(t2), toEpoch(t1));
    return diff / 86400.0;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

HabitAnalyzer::HabitAnalyzer(IScaleDatabase* db) : db_(db) {}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

HabitInsights HabitAnalyzer::getInsights(const std::string& user_id, int days) {
    HabitInsights insights;

    auto measurements = db_->getMeasurements(user_id, days, 10000, 0);
    if (measurements.empty()) {
        spdlog::warn("HabitAnalyzer: no measurements for user {} in {} days", user_id, days);
        return insights;
    }

    // Sort by measured_at ascending
    std::sort(measurements.begin(), measurements.end(),
              [](const ScaleMeasurement& a, const ScaleMeasurement& b) {
                  return a.measured_at < b.measured_at;
              });

    insights.consistency = analyzeConsistency(measurements);
    insights.weight      = analyzeWeightTrend(measurements);
    insights.body_comp   = analyzeBodyCompTrend(measurements);
    insights.weekly      = analyzeWeeklyPattern(measurements);
    insights.predictions = makePredictions(measurements);
    insights.recommendations = generateRecommendations(insights);
    insights.alerts          = generateAlerts(insights);

    return insights;
}

PublishableHabitMetrics HabitAnalyzer::getPublishableMetrics(const std::string& user_id,
                                                             int days) {
    auto insights = getInsights(user_id, days);
    PublishableHabitMetrics m;
    m.consistency_score      = insights.consistency.consistency_score;
    m.current_streak_days    = insights.consistency.current_streak_days;
    m.weight_trend_direction = insights.weight.trend_direction;
    m.weight_change_kg       = insights.weight.total_change_kg;
    m.predicted_weight_7d    = insights.predictions.predicted_weight_7d;
    m.predicted_weight_30d   = insights.predictions.predicted_weight_30d;
    m.rate_kg_per_week       = insights.predictions.rate_kg_per_week;
    m.body_fat_trend         = insights.body_comp.body_fat_trend;
    m.muscle_trend           = insights.body_comp.muscle_trend;
    m.is_recomposing         = insights.body_comp.is_recomposing;
    m.heaviest_day           = insights.weekly.heaviest_day;
    m.lightest_day           = insights.weekly.lightest_day;
    return m;
}

// ---------------------------------------------------------------------------
// analyzeConsistency
// ---------------------------------------------------------------------------

ConsistencyMetrics HabitAnalyzer::analyzeConsistency(
    const std::vector<ScaleMeasurement>& measurements) {
    ConsistencyMetrics c;
    c.total_measurements = static_cast<int>(measurements.size());
    if (measurements.size() < 2) {
        c.consistency_score = 100;
        c.max_streak_days = c.total_measurements;
        c.current_streak_days = c.total_measurements;
        return c;
    }

    // Compute gaps
    std::vector<double> gaps;
    for (size_t i = 1; i < measurements.size(); ++i) {
        double gap = daysBetween(measurements[i - 1].measured_at,
                                 measurements[i].measured_at);
        gaps.push_back(gap);
    }

    double sum_gaps = std::accumulate(gaps.begin(), gaps.end(), 0.0);
    c.avg_days_between = sum_gaps / gaps.size();

    double max_gap = *std::max_element(gaps.begin(), gaps.end());
    c.longest_gap_days = static_cast<int>(std::ceil(max_gap));

    // Consistency score: daily=100, every 6 days=0
    c.consistency_score = std::min(100.0, std::max(0.0,
        100.0 - (c.avg_days_between - 1.0) * 20.0));

    // Streak calculation (gap <= 2 days continues streak)
    int max_streak = 1;
    int current_streak = 1;
    for (size_t i = 0; i < gaps.size(); ++i) {
        if (gaps[i] <= 2.0) {
            current_streak++;
        } else {
            current_streak = 1;
        }
        max_streak = std::max(max_streak, current_streak);
    }
    c.max_streak_days = max_streak;
    c.current_streak_days = current_streak;

    return c;
}

// ---------------------------------------------------------------------------
// analyzeWeightTrend
// ---------------------------------------------------------------------------

WeightTrend HabitAnalyzer::analyzeWeightTrend(
    const std::vector<ScaleMeasurement>& measurements) {
    WeightTrend w;
    if (measurements.empty()) return w;

    std::vector<double> weights;
    for (auto& m : measurements) {
        if (m.weight_kg > 0) weights.push_back(m.weight_kg);
    }
    if (weights.size() < 2) {
        if (!weights.empty()) {
            w.current_weight = weights.back();
            w.starting_weight = weights.front();
            w.min_weight = weights.front();
            w.max_weight = weights.front();
            w.trend_direction = "stable";
        }
        return w;
    }

    w.current_weight  = weights.back();
    w.starting_weight = weights.front();
    w.total_change_kg = w.current_weight - w.starting_weight;
    w.min_weight = *std::min_element(weights.begin(), weights.end());
    w.max_weight = *std::max_element(weights.begin(), weights.end());
    w.weight_range = w.max_weight - w.min_weight;

    // Linear regression on index vs weight
    std::vector<double> x(weights.size());
    std::iota(x.begin(), x.end(), 0.0);
    double slope = linearRegressionSlope(x, weights);
    w.slope_kg_per_measurement = slope;

    if (slope > 0.01)       w.trend_direction = "gaining";
    else if (slope < -0.01) w.trend_direction = "losing";
    else                    w.trend_direction = "stable";

    // Volatility
    w.volatility = stddev(weights);
    w.is_volatile = w.volatility > 1.5;

    // Rolling averages (last N measurements, not calendar days)
    auto rollingAvg = [&](size_t window) -> double {
        if (weights.size() < window) window = weights.size();
        double sum = 0;
        for (size_t i = weights.size() - window; i < weights.size(); ++i)
            sum += weights[i];
        return sum / window;
    };
    w.avg_7d  = rollingAvg(7);
    w.avg_30d = rollingAvg(30);

    return w;
}

// ---------------------------------------------------------------------------
// analyzeBodyCompTrend
// ---------------------------------------------------------------------------

BodyCompTrend HabitAnalyzer::analyzeBodyCompTrend(
    const std::vector<ScaleMeasurement>& measurements) {
    BodyCompTrend bc;
    bc.body_fat_trend = "stable";
    bc.muscle_trend   = "stable";

    std::vector<double> bf_vals, muscle_vals;
    for (auto& m : measurements) {
        if (m.composition.body_fat_percentage > 0)
            bf_vals.push_back(m.composition.body_fat_percentage);
        if (m.composition.muscle_mass_kg > 0)
            muscle_vals.push_back(m.composition.muscle_mass_kg);
    }

    if (bf_vals.size() >= 2) {
        bc.body_fat_change = bf_vals.back() - bf_vals.front();
        std::vector<double> x(bf_vals.size());
        std::iota(x.begin(), x.end(), 0.0);
        double bf_slope = linearRegressionSlope(x, bf_vals);
        if (bf_slope < -0.01)      bc.body_fat_trend = "decreasing";
        else if (bf_slope > 0.01)  bc.body_fat_trend = "increasing";
        else                       bc.body_fat_trend = "stable";
    }

    if (muscle_vals.size() >= 2) {
        bc.muscle_change_kg = muscle_vals.back() - muscle_vals.front();
        std::vector<double> x(muscle_vals.size());
        std::iota(x.begin(), x.end(), 0.0);
        double mu_slope = linearRegressionSlope(x, muscle_vals);
        if (mu_slope > 0.01)       bc.muscle_trend = "gaining";
        else if (mu_slope < -0.01) bc.muscle_trend = "losing";
        else                       bc.muscle_trend = "stable";
    }

    bc.is_recomposing = (bc.body_fat_trend == "decreasing" &&
                         bc.muscle_trend == "gaining");
    return bc;
}

// ---------------------------------------------------------------------------
// analyzeWeeklyPattern
// ---------------------------------------------------------------------------

WeeklyPattern HabitAnalyzer::analyzeWeeklyPattern(
    const std::vector<ScaleMeasurement>& measurements) {
    WeeklyPattern wp;
    if (measurements.empty()) return wp;

    // Accumulate weights per day-of-week
    std::map<int, std::vector<double>> dow_weights;  // 0=Mon .. 6=Sun
    for (auto& m : measurements) {
        if (m.weight_kg <= 0) continue;
        int dow = dayOfWeek(m.measured_at);
        dow_weights[dow].push_back(m.weight_kg);
    }

    if (dow_weights.empty()) return wp;

    double best_avg = -1e9, worst_avg = 1e9;
    int heaviest_dow = 0, lightest_dow = 0;

    for (auto& [dow, wts] : dow_weights) {
        double avg = std::accumulate(wts.begin(), wts.end(), 0.0) / wts.size();
        std::string name = dayName(dow);
        wp.day_averages[name] = avg;
        if (avg > best_avg)  { best_avg = avg;  heaviest_dow = dow; }
        if (avg < worst_avg) { worst_avg = avg; lightest_dow = dow; }
    }

    wp.heaviest_day = dayName(heaviest_dow);
    wp.lightest_day = dayName(lightest_dow);
    wp.weekly_variation_kg = best_avg - worst_avg;
    wp.has_weekend_effect = wp.weekly_variation_kg > 1.0;

    return wp;
}

// ---------------------------------------------------------------------------
// makePredictions
// ---------------------------------------------------------------------------

Prediction HabitAnalyzer::makePredictions(
    const std::vector<ScaleMeasurement>& measurements) {
    Prediction p;
    p.trend_confidence = "low";

    std::vector<double> weights;
    for (auto& m : measurements)
        if (m.weight_kg > 0) weights.push_back(m.weight_kg);

    if (weights.size() < 7) return p;

    std::vector<double> x(weights.size());
    std::iota(x.begin(), x.end(), 0.0);
    double slope = linearRegressionSlope(x, weights);

    // Total span in days
    double total_days = daysBetween(measurements.front().measured_at,
                                    measurements.back().measured_at);
    if (total_days < 1.0) total_days = 1.0;

    double measurements_per_day = static_cast<double>(weights.size()) / total_days;
    p.rate_kg_per_week = slope * measurements_per_day * 7.0;
    p.rate_lbs_per_week = p.rate_kg_per_week * 2.20462;

    double current = weights.back();
    p.predicted_weight_7d  = current + p.rate_kg_per_week;
    p.predicted_weight_30d = current + p.rate_kg_per_week * (30.0 / 7.0);
    p.predicted_weight_90d = current + p.rate_kg_per_week * (90.0 / 7.0);

    if (weights.size() > 30)      p.trend_confidence = "high";
    else if (weights.size() > 14) p.trend_confidence = "medium";
    else                          p.trend_confidence = "low";

    return p;
}

// ---------------------------------------------------------------------------
// generateRecommendations
// ---------------------------------------------------------------------------

std::vector<Recommendation> HabitAnalyzer::generateRecommendations(
    const HabitInsights& insights) {
    std::vector<Recommendation> recs;

    // Low consistency
    if (insights.consistency.consistency_score < 70) {
        std::ostringstream msg;
        msg << "You measure every " << std::fixed;
        msg.precision(1);
        msg << insights.consistency.avg_days_between
            << " days on average. Try measuring daily for better trend tracking.";
        recs.push_back({"consistency", "high", "Weigh More Consistently",
                        msg.str(),
                        "Set a daily reminder at your preferred time"});
    }

    // High volatility
    if (insights.weight.is_volatile) {
        std::ostringstream msg;
        msg << "Your weight varies by " << std::fixed;
        msg.precision(1);
        msg << insights.weight.weight_range
            << "kg. This could indicate inconsistent measurement timing or hydration.";
        recs.push_back({"stability", "medium", "Reduce Weight Fluctuations",
                        msg.str(),
                        "Measure at the same time daily (preferably morning, after bathroom, before eating)"});
    }

    // Recomposition
    if (insights.body_comp.is_recomposing) {
        std::ostringstream msg;
        msg << "You're losing fat (" << std::fixed;
        msg.precision(1);
        msg << insights.body_comp.body_fat_change
            << "%) while gaining muscle ("
            << insights.body_comp.muscle_change_kg << "kg). Great work!";
        recs.push_back({"achievement", "positive", "Body Recomposition Detected",
                        msg.str(),
                        "Keep doing what you are doing - your current routine is working"});
    }

    // Weekend effect
    if (insights.weekly.has_weekend_effect) {
        std::ostringstream msg;
        msg << "You're heaviest on " << insights.weekly.heaviest_day
            << " and lightest on " << insights.weekly.lightest_day << ".";
        recs.push_back({"pattern", "medium", "Weekend Pattern Detected",
                        msg.str(),
                        "Focus on portion control during your heavier days"});
    }

    // Rapid gain (>0.5 kg/week)
    if (insights.predictions.rate_kg_per_week > 0.5) {
        std::ostringstream msg;
        msg << "You're gaining " << std::fixed;
        msg.precision(2);
        msg << insights.predictions.rate_kg_per_week
            << "kg/week. Healthy rate is 0.25-0.5kg/week.";
        recs.push_back({"warning", "high", "Rapid Weight Gain",
                        msg.str(),
                        "Review calorie intake and increase physical activity"});
    }

    // Rapid loss (< -1.0 kg/week)
    if (insights.predictions.rate_kg_per_week < -1.0) {
        std::ostringstream msg;
        msg << "You're losing " << std::fixed;
        msg.precision(2);
        msg << std::abs(insights.predictions.rate_kg_per_week)
            << "kg/week. Healthy rate is 0.5-1kg/week.";
        recs.push_back({"warning", "high", "Rapid Weight Loss",
                        msg.str(),
                        "Ensure adequate nutrition - rapid loss can lead to muscle loss"});
    }

    return recs;
}

// ---------------------------------------------------------------------------
// generateAlerts
// ---------------------------------------------------------------------------

std::vector<Alert> HabitAnalyzer::generateAlerts(const HabitInsights& insights) {
    std::vector<Alert> alerts;

    if (insights.consistency.longest_gap_days > 14) {
        std::ostringstream msg;
        msg << "You went " << insights.consistency.longest_gap_days
            << " days without measuring. Regular tracking helps maintain accountability.";
        alerts.push_back({"gap", "warning", msg.str()});
    }

    if (insights.weight.weight_range > 5.0) {
        std::ostringstream msg;
        msg << "Weight fluctuates by " << std::fixed;
        msg.precision(1);
        msg << insights.weight.weight_range
            << "kg. Consider tracking hydration and meal timing.";
        alerts.push_back({"volatility", "caution", msg.str()});
    }

    return alerts;
}

}  // namespace hms_colada
