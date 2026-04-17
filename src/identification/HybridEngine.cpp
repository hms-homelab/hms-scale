#include "identification/HybridEngine.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace hms_colada {

// ---------------------------------------------------------------------------
// Default model directory: ~/.hms-colada/models/
// ---------------------------------------------------------------------------
static std::string defaultModelDir() {
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.hms-colada/models";
    return "/tmp/hms-colada/models";
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

HybridEngine::HybridEngine(IScaleDatabase* db, const std::string& model_dir)
    : db_(db),
      model_dir_(model_dir.empty() ? defaultModelDir() : model_dir) {
    // Attempt to load model on construction
    loadModel();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::optional<IdentificationResult> HybridEngine::identify(
        double weight_kg, double impedance_ohm, const std::string& timestamp) {

    spdlog::info("HybridEngine: identify weight={:.2f}kg impedance={:.1f}ohm",
                 weight_kg, impedance_ohm);

    auto users = db_->getUsers(/*active_only=*/true);
    if (users.empty()) {
        spdlog::warn("HybridEngine: no active users in database");
        return std::nullopt;
    }

    // Stage 1: Exact match (+/-0.5kg)
    auto result = exactMatch(weight_kg, users);
    if (result) {
        spdlog::info("HybridEngine: Stage 1 exact match -> {} ({}% confidence)",
                     result->user_name, result->confidence);
        return result;
    }

    // Stage 2: ML prediction (>=80% confidence)
    result = mlPredict(weight_kg, impedance_ohm, timestamp, users);
    if (result) {
        spdlog::info("HybridEngine: Stage 2 ML prediction -> {} ({}% confidence)",
                     result->user_name, result->confidence);
        return result;
    }

    // Stage 3: Tolerance match
    result = toleranceMatch(weight_kg, users);
    if (result) {
        spdlog::info("HybridEngine: Stage 3 tolerance match -> {} ({}% confidence)",
                     result->user_name, result->confidence);
        return result;
    }

    // Stage 4: Manual assignment required
    spdlog::info("HybridEngine: all stages failed - manual assignment required");
    return std::nullopt;
}

bool HybridEngine::loadModel() {
    model_loaded_ = false;

    auto model_path = fs::path(model_dir_) / "user_classifier.json";
    auto scaler_path = fs::path(model_dir_) / "user_classifier_scaler.json";
    auto classes_path = fs::path(model_dir_) / "user_classifier_classes.json";

    if (!fs::exists(model_path) || !fs::exists(scaler_path) || !fs::exists(classes_path)) {
        spdlog::info("HybridEngine: model files not found in {}", model_dir_);
        return false;
    }

    try {
        // Load model
        {
            std::ifstream f(model_path);
            nlohmann::json j = nlohmann::json::parse(f);
            model_ = ml::RandomForest::fromJson(j);
        }

        // Load scaler
        {
            std::ifstream f(scaler_path);
            nlohmann::json j = nlohmann::json::parse(f);
            scaler_ = ml::StandardScaler::fromJson(j);
        }

        // Load class names and user name map
        {
            std::ifstream f(classes_path);
            nlohmann::json j = nlohmann::json::parse(f);
            class_names_ = j.at("class_names").get<std::vector<std::string>>();
            user_name_map_.clear();
            auto& name_map = j.at("user_name_map");
            for (auto it = name_map.begin(); it != name_map.end(); ++it) {
                user_name_map_[it.key()] = it.value().get<std::string>();
            }
        }

        model_loaded_ = true;
        spdlog::info("HybridEngine: loaded ML model ({} classes) from {}",
                     class_names_.size(), model_dir_);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("HybridEngine: failed to load model: {}", e.what());
        model_loaded_ = false;
        return false;
    }
}

bool HybridEngine::hasModel() const {
    return model_loaded_;
}

// ---------------------------------------------------------------------------
// Stage 1: Exact match (+/-0.5kg, 95% confidence)
// ---------------------------------------------------------------------------

std::optional<IdentificationResult> HybridEngine::exactMatch(
        double weight_kg, const std::vector<ScaleUser>& users) {

    constexpr double kExactTolerance = 0.5;  // kg

    std::vector<std::pair<const ScaleUser*, double>> candidates;

    for (const auto& user : users) {
        if (user.expected_weight_kg <= 0) continue;

        double diff = std::abs(weight_kg - user.expected_weight_kg);
        if (diff <= kExactTolerance) {
            candidates.emplace_back(&user, diff);
        }
    }

    // Only return if exactly one candidate (unambiguous)
    if (candidates.size() == 1) {
        const auto& [user, diff] = candidates[0];
        IdentificationResult result;
        result.user_id = user->id;
        result.user_name = user->name;
        result.confidence = 95.0;
        result.method = "deterministic_exact_match";
        return result;
    }

    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Stage 2: ML prediction (>=80% confidence)
// ---------------------------------------------------------------------------

std::optional<IdentificationResult> HybridEngine::mlPredict(
        double weight_kg, double impedance_ohm, const std::string& timestamp,
        const std::vector<ScaleUser>& users) {

    if (!model_loaded_) return std::nullopt;

    try {
        // Extract features for the single measurement
        auto features = ml::ScaleFeatureEngine::extractSingle(
            weight_kg, impedance_ohm, timestamp);

        // Scale features
        auto scaled = scaler_.transformRow(features);

        // Predict class and probabilities
        int predicted_class = model_.predictClass(scaled);
        auto probas = model_.predictProba(scaled);

        // Find max probability
        double max_proba = 0;
        for (auto p : probas) max_proba = std::max(max_proba, p);

        // Build result
        IdentificationResult result;
        if (predicted_class >= 0 && predicted_class < static_cast<int>(class_names_.size())) {
            result.user_id = class_names_[predicted_class];
        }

        // Look up user name
        auto name_it = user_name_map_.find(result.user_id);
        result.user_name = (name_it != user_name_map_.end()) ? name_it->second : result.user_id;

        result.confidence = max_proba * 100.0;
        result.method = "ml_random_forest";

        // Include all probabilities (user_name -> probability)
        for (size_t i = 0; i < probas.size() && i < class_names_.size(); ++i) {
            auto nit = user_name_map_.find(class_names_[i]);
            std::string name = (nit != user_name_map_.end()) ? nit->second : class_names_[i];
            result.ml_probabilities[name] = probas[i];
        }

        // Only return if confidence >= 80%
        if (result.confidence >= 80.0) {
            return result;
        }

        spdlog::info("HybridEngine: ML confidence {:.1f}% < 80% threshold, skipping",
                     result.confidence);
        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("HybridEngine: ML prediction failed: {}", e.what());
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// Stage 3: Tolerance match (user-defined tolerance, 75% confidence)
// ---------------------------------------------------------------------------

std::optional<IdentificationResult> HybridEngine::toleranceMatch(
        double weight_kg, const std::vector<ScaleUser>& users) {

    struct Candidate {
        const ScaleUser* user;
        double diff;
    };

    std::vector<Candidate> candidates;

    for (const auto& user : users) {
        if (user.expected_weight_kg <= 0) continue;

        double tolerance = user.weight_tolerance_kg;
        if (tolerance <= 0) tolerance = 3.0;  // default

        double diff = std::abs(weight_kg - user.expected_weight_kg);
        if (diff <= tolerance) {
            candidates.push_back({&user, diff});
        }
    }

    if (candidates.empty()) return std::nullopt;

    // Pick the closest match
    auto best = std::min_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) { return a.diff < b.diff; });

    IdentificationResult result;
    result.user_id = best->user->id;
    result.user_name = best->user->name;
    result.confidence = 75.0;
    result.method = "deterministic_tolerance_match";
    return result;
}

}  // namespace hms_colada
