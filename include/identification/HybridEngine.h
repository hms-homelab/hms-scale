#pragma once

#include "database/IScaleDatabase.h"
#include "models/ScaleModels.h"
#include "ml/RandomForest.h"
#include "ml/StandardScaler.h"
#include "ml/ScaleFeatureEngine.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace hms_colada {

struct IdentificationResult {
    std::string user_id;
    std::string user_name;
    double confidence = 0;          // 0-100
    std::string method;             // "deterministic_exact_match", "ml_random_forest",
                                    // "deterministic_tolerance_match", "manual"
    bool requires_manual = false;
    std::map<std::string, double> ml_probabilities;  // user_name -> probability
};

class HybridEngine {
public:
    HybridEngine(IScaleDatabase* db, const std::string& model_dir = "");

    // Main identification method
    std::optional<IdentificationResult> identify(double weight_kg,
                                                  double impedance_ohm,
                                                  const std::string& timestamp = "");

    // Load/reload ML model from disk
    bool loadModel();
    bool hasModel() const;

private:
    // Stage 1: Exact match (+/-0.5kg, 95% confidence)
    std::optional<IdentificationResult> exactMatch(double weight_kg,
                                                    const std::vector<ScaleUser>& users);

    // Stage 2: ML prediction (>=80% confidence)
    std::optional<IdentificationResult> mlPredict(double weight_kg,
                                                   double impedance_ohm,
                                                   const std::string& timestamp,
                                                   const std::vector<ScaleUser>& users);

    // Stage 3: Tolerance match (user.weight_tolerance_kg, 75% confidence)
    std::optional<IdentificationResult> toleranceMatch(double weight_kg,
                                                        const std::vector<ScaleUser>& users);

    IScaleDatabase* db_;
    std::string model_dir_;
    ml::RandomForest model_;
    ml::StandardScaler scaler_;
    std::vector<std::string> class_names_;           // user_id per class index
    std::map<std::string, std::string> user_name_map_; // user_id -> name
    bool model_loaded_ = false;
};

}  // namespace hms_colada
