#include "ml/ScaleFeatureEngine.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace hms_colada {
namespace ml {

void ScaleFeatureEngine::parseTimestamp(const std::string& ts, int& hour, int& day_of_week) {
    // Parse "YYYY-MM-DDTHH:MM:SS" with sscanf + mktime for day_of_week
    int year = 0, month = 0, day = 0, h = 0, min = 0, sec = 0;
    std::sscanf(ts.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &h, &min, &sec);

    hour = h;

    // Use mktime to compute day_of_week
    std::tm tm_val;
    std::memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = h;
    tm_val.tm_min = min;
    tm_val.tm_sec = sec;
    tm_val.tm_isdst = -1;

    std::mktime(&tm_val);
    // tm_wday: 0=Sunday, 1=Monday, ..., 6=Saturday
    // Convert to 0=Monday, 6=Sunday
    day_of_week = (tm_val.tm_wday + 6) % 7;
}

std::vector<std::string> ScaleFeatureEngine::featureNames() {
    return {
        "weight_kg",
        "impedance_ohm",
        "has_impedance",
        "hour_of_day",
        "day_of_week",
        "is_morning",
        "is_evening",
        "is_weekend",
        "hour_sin",
        "hour_cos",
        "day_sin",
        "day_cos",
        "weight_delta_1d",
        "weight_delta_7d",
        "impedance_weight_ratio"
    };
}

std::vector<double> ScaleFeatureEngine::extractSingle(double weight_kg, double impedance_ohm,
                                                       const std::string& timestamp,
                                                       double prev_weight_1d,
                                                       double prev_weight_7d) {
    int hour = 0, day_of_week = 0;
    parseTimestamp(timestamp, hour, day_of_week);

    std::vector<double> features(15);

    // 0: weight_kg
    features[0] = weight_kg;
    // 1: impedance_ohm (0 if missing)
    features[1] = impedance_ohm;
    // 2: has_impedance
    features[2] = (impedance_ohm > 0) ? 1.0 : 0.0;
    // 3: hour_of_day
    features[3] = static_cast<double>(hour);
    // 4: day_of_week (0=Monday, 6=Sunday)
    features[4] = static_cast<double>(day_of_week);
    // 5: is_morning (hour 6-11)
    features[5] = (hour >= 6 && hour < 12) ? 1.0 : 0.0;
    // 6: is_evening (hour 18-23)
    features[6] = (hour >= 18 && hour < 24) ? 1.0 : 0.0;
    // 7: is_weekend — Python uses day_of_week.isin([5, 6]) which is Saturday=5, Sunday=6
    features[7] = (day_of_week == 5 || day_of_week == 6) ? 1.0 : 0.0;
    // 8: hour_sin
    features[8] = std::sin(2.0 * M_PI * hour / 24.0);
    // 9: hour_cos
    features[9] = std::cos(2.0 * M_PI * hour / 24.0);
    // 10: day_sin
    features[10] = std::sin(2.0 * M_PI * day_of_week / 7.0);
    // 11: day_cos
    features[11] = std::cos(2.0 * M_PI * day_of_week / 7.0);
    // 12: weight_delta_1d
    features[12] = (prev_weight_1d > 0) ? (weight_kg - prev_weight_1d) : 0.0;
    // 13: weight_delta_7d
    features[13] = (prev_weight_7d > 0) ? (weight_kg - prev_weight_7d) : 0.0;
    // 14: impedance_weight_ratio
    features[14] = (weight_kg > 0) ? (impedance_ohm / weight_kg) : 0.0;

    return features;
}

ScaleFeatureEngine::Result ScaleFeatureEngine::build(
        const std::vector<ScaleMeasurementRecord>& records, int min_per_user) {

    Result result;
    result.feature_names = featureNames();

    // Group records by user_id
    std::map<std::string, std::vector<const ScaleMeasurementRecord*>> user_records;
    for (const auto& r : records) {
        user_records[r.user_id].push_back(&r);
    }

    // Sort each user's records by measured_at and assign class indices
    std::map<std::string, int> user_to_class;
    for (auto& [uid, recs] : user_records) {
        if (static_cast<int>(recs.size()) < min_per_user) continue;

        // Sort by timestamp
        std::sort(recs.begin(), recs.end(),
                  [](const ScaleMeasurementRecord* a, const ScaleMeasurementRecord* b) {
                      return a->measured_at < b->measured_at;
                  });

        int class_idx = static_cast<int>(result.class_names.size());
        user_to_class[uid] = class_idx;
        result.class_names.push_back(uid);
    }

    // Build feature matrix
    for (auto& [uid, recs] : user_records) {
        auto it = user_to_class.find(uid);
        if (it == user_to_class.end()) continue; // skipped (too few)

        int class_idx = it->second;

        for (size_t i = 0; i < recs.size(); ++i) {
            const auto* rec = recs[i];

            // Find previous weight for delta features
            double prev_1d = 0.0;
            double prev_7d = 0.0;
            if (i > 0) {
                prev_1d = recs[i - 1]->weight_kg;
            }
            if (i >= 7) {
                prev_7d = recs[i - 7]->weight_kg;
            }

            auto features = extractSingle(rec->weight_kg, rec->impedance_ohm,
                                           rec->measured_at, prev_1d, prev_7d);
            result.X.push_back(std::move(features));
            result.y.push_back(class_idx);
        }
    }

    return result;
}

}  // namespace ml
}  // namespace hms_colada
