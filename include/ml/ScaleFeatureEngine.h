#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace hms_colada {
namespace ml {

struct ScaleMeasurementRecord {
    double weight_kg;
    double impedance_ohm;
    std::string measured_at; // ISO8601 timestamp
    std::string user_id;    // for training labels
};

class ScaleFeatureEngine {
public:
    struct Result {
        std::vector<std::vector<double>> X;  // feature matrix [n_samples x 15]
        std::vector<int> y;                   // user labels (class indices)
        std::vector<std::string> feature_names;
        std::vector<std::string> class_names; // user_id list (index = class label)
    };

    // Build features from historical measurements (for training)
    // Groups by user_id, requires min_per_user measurements per user
    static Result build(const std::vector<ScaleMeasurementRecord>& records, int min_per_user = 20);

    // Extract features for a single measurement (for inference)
    // prev_weight_1d and prev_weight_7d are previous weights for delta features
    // Set to 0 if not available
    static std::vector<double> extractSingle(double weight_kg, double impedance_ohm,
                                              const std::string& timestamp,
                                              double prev_weight_1d = 0,
                                              double prev_weight_7d = 0);

    // Feature names (static, always 15)
    static std::vector<std::string> featureNames();

private:
    // Parse ISO8601 timestamp "YYYY-MM-DDTHH:MM:SS" to hour and day_of_week
    static void parseTimestamp(const std::string& ts, int& hour, int& day_of_week);
};

}  // namespace ml
}  // namespace hms_colada
