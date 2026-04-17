#include "services/MLTrainingService.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace fs = std::filesystem;

namespace hms_colada {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string MLTrainingService::defaultModelDir() {
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.hms-colada/models";
    return "/tmp/hms-colada/models";
}

std::string MLTrainingService::modelPath(const std::string& filename) const {
    return (fs::path(config_.model_dir) / filename).string();
}

std::string MLTrainingService::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

MLTrainingService::MLTrainingService(IScaleDatabase* db, const Config& config)
    : db_(db),
      config_(config),
      last_trained_(std::chrono::system_clock::time_point{}) {
    if (config_.model_dir.empty()) {
        config_.model_dir = defaultModelDir();
    }
}

MLTrainingService::~MLTrainingService() {
    stop();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void MLTrainingService::start() {
    if (!config_.enabled) {
        spdlog::info("MLTrainingService: disabled by config");
        return;
    }

    // Ensure model directory exists
    std::error_code ec;
    fs::create_directories(config_.model_dir, ec);
    if (ec) {
        spdlog::error("MLTrainingService: failed to create model_dir '{}': {}",
                      config_.model_dir, ec.message());
    }

    // Try to load existing models
    loadModel();

    // Spawn worker thread
    running_ = true;
    worker_ = std::thread(&MLTrainingService::runLoop, this);
    spdlog::info("MLTrainingService: started (schedule={})", config_.schedule);
}

void MLTrainingService::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    spdlog::info("MLTrainingService: stopped");
}

void MLTrainingService::triggerTraining() {
    train_requested_ = true;
    spdlog::info("MLTrainingService: manual training triggered");
}

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

Json::Value MLTrainingService::getStatus() const {
    Json::Value s;

    std::lock_guard<std::mutex> lock(metrics_mutex_);
    s["status"] = status_;
    s["schedule"] = config_.schedule;
    s["min_measurements"] = config_.min_measurements;
    s["model_dir"] = config_.model_dir;

    if (!last_error_.empty()) {
        s["last_error"] = last_error_;
    }

    if (!metrics_.trained_at.empty()) {
        Json::Value m;
        m["accuracy"] = metrics_.accuracy;
        m["cv_accuracy"] = metrics_.cv_accuracy;
        m["cv_std"] = metrics_.cv_std;
        m["n_samples"] = metrics_.n_samples;
        m["n_users"] = metrics_.n_users;
        m["trained_at"] = metrics_.trained_at;

        Json::Value fi;
        for (const auto& [name, imp] : metrics_.feature_importance) {
            fi[name] = imp;
        }
        m["feature_importance"] = fi;

        s["metrics"] = m;
    }

    return s;
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void MLTrainingService::runLoop() {
    spdlog::info("MLTrainingService: worker thread running");

    while (running_) {
        // Sleep in 1-second increments so we react to stop quickly
        for (int i = 0; i < 60 && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running_) break;

        bool should_train = train_requested_.exchange(false) || shouldTrainNow();
        if (!should_train) continue;

        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            status_ = "training";
            last_error_.clear();
        }

        try {
            trainModel();

            std::lock_guard<std::mutex> lock(metrics_mutex_);
            status_ = "complete";
            last_trained_ = std::chrono::system_clock::now();
            spdlog::info("MLTrainingService: training complete - accuracy={:.4f} cv={:.4f}",
                         metrics_.accuracy, metrics_.cv_accuracy);

        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            status_ = "error";
            last_error_ = e.what();
            spdlog::error("MLTrainingService: training failed: {}", e.what());
        }
    }
}

bool MLTrainingService::shouldTrainNow() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - last_trained_);

    if (config_.schedule == "daily") {
        return elapsed.count() >= 24;
    } else if (config_.schedule == "weekly") {
        return elapsed.count() >= 24 * 7;
    } else if (config_.schedule == "monthly") {
        return elapsed.count() >= 24 * 30;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Training pipeline
// ---------------------------------------------------------------------------

void MLTrainingService::trainModel() {
    spdlog::info("MLTrainingService: starting training pipeline");

    // 1. Load all measurements from DB
    auto measurements = db_->getMeasurementsForML(config_.min_measurements);
    if (measurements.empty()) {
        throw std::runtime_error("No measurements available for training");
    }

    spdlog::info("MLTrainingService: loaded {} measurements", measurements.size());

    // 2. Convert to ScaleMeasurementRecord vector
    std::vector<ml::ScaleMeasurementRecord> records;
    records.reserve(measurements.size());
    for (const auto& m : measurements) {
        if (m.user_id.empty()) continue;  // skip unassigned
        records.push_back({m.weight_kg, m.impedance_ohm, m.measured_at, m.user_id});
    }

    if (records.empty()) {
        throw std::runtime_error("No assigned measurements available for training");
    }

    // 3. Build feature matrix + labels
    auto fe_result = ml::ScaleFeatureEngine::build(records, config_.min_measurements);
    if (fe_result.X.empty()) {
        throw std::runtime_error("Feature engineering produced no samples (insufficient data per user)");
    }

    int n_classes = static_cast<int>(fe_result.class_names.size());
    int n_samples = static_cast<int>(fe_result.X.size());
    int n_features = static_cast<int>(fe_result.X[0].size());

    spdlog::info("MLTrainingService: {} samples, {} features, {} classes",
                 n_samples, n_features, n_classes);

    if (n_classes < 2) {
        throw std::runtime_error("Need at least 2 users for classification training");
    }

    // 4. Split 80/20 (simple stratified: take every 5th sample for test)
    std::vector<std::vector<double>> X_train, X_test;
    std::vector<int> y_train, y_test;

    for (int i = 0; i < n_samples; ++i) {
        if (i % 5 == 0) {
            X_test.push_back(fe_result.X[i]);
            y_test.push_back(fe_result.y[i]);
        } else {
            X_train.push_back(fe_result.X[i]);
            y_train.push_back(fe_result.y[i]);
        }
    }

    if (X_test.empty() || X_train.empty()) {
        throw std::runtime_error("Insufficient data for train/test split");
    }

    spdlog::info("MLTrainingService: train={} test={}", X_train.size(), X_test.size());

    // 5. Fit scaler on training set
    ml::StandardScaler scaler;
    auto X_train_scaled = scaler.fitTransform(X_train);
    auto X_test_scaled = scaler.transform(X_test);

    // 6. Grid search over hyperparameters
    struct HyperCombo {
        int n_estimators;
        int max_depth;
        int min_samples_split;
        double cv_accuracy;
    };

    std::vector<HyperCombo> combos;
    for (int n_est : {100, 200}) {
        for (int depth : {6, 8, 10}) {
            for (int min_split : {3, 5}) {
                combos.push_back({n_est, depth, min_split, 0.0});
            }
        }
    }

    double best_cv = -1;
    int best_idx = 0;

    for (size_t i = 0; i < combos.size(); ++i) {
        ml::RandomForest::Params params;
        params.n_estimators = combos[i].n_estimators;
        params.max_depth = combos[i].max_depth;
        params.min_samples_split = combos[i].min_samples_split;
        params.class_weight_balanced = true;

        auto cv_result = ml::CrossValidator::cvClassification(
            X_train_scaled, y_train, n_classes, params, 5, 42);

        combos[i].cv_accuracy = cv_result.accuracy_mean;

        spdlog::debug("MLTrainingService: n_est={} depth={} split={} -> cv_acc={:.4f}",
                      combos[i].n_estimators, combos[i].max_depth,
                      combos[i].min_samples_split, cv_result.accuracy_mean);

        if (cv_result.accuracy_mean > best_cv) {
            best_cv = cv_result.accuracy_mean;
            best_idx = static_cast<int>(i);
        }
    }

    auto& best = combos[best_idx];
    spdlog::info("MLTrainingService: best params - n_est={} depth={} split={} cv_acc={:.4f}",
                 best.n_estimators, best.max_depth, best.min_samples_split, best.cv_accuracy);

    // 7. Train final model with best params on full training set
    ml::RandomForest::Params best_params;
    best_params.n_estimators = best.n_estimators;
    best_params.max_depth = best.max_depth;
    best_params.min_samples_split = best.min_samples_split;
    best_params.class_weight_balanced = true;

    ml::RandomForest model(best_params);
    model.fitClassification(X_train_scaled, y_train, n_classes);

    // 8. Evaluate on test set
    int correct = 0;
    for (size_t i = 0; i < X_test_scaled.size(); ++i) {
        int pred = model.predictClass(X_test_scaled[i]);
        if (pred == y_test[i]) ++correct;
    }
    double test_accuracy = static_cast<double>(correct) / X_test_scaled.size();

    spdlog::info("MLTrainingService: test accuracy={:.4f}", test_accuracy);

    // 9. Get feature importances
    auto importances = model.featureImportances();
    auto feature_names = ml::ScaleFeatureEngine::featureNames();

    std::map<std::string, double> fi_map;
    for (size_t i = 0; i < importances.size() && i < feature_names.size(); ++i) {
        fi_map[feature_names[i]] = importances[i];
    }

    // 10. Store results
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        model_ = std::move(model);
        scaler_ = scaler;
        class_names_ = fe_result.class_names;

        metrics_.accuracy = test_accuracy;
        metrics_.cv_accuracy = best.cv_accuracy;
        metrics_.cv_std = 0;  // not tracked per-combo, use CV result
        metrics_.n_samples = n_samples;
        metrics_.n_users = n_classes;
        metrics_.trained_at = currentTimestamp();
        metrics_.feature_importance = fi_map;
    }

    // 11. Build user name map from DB for classes file
    auto all_users = db_->getUsers(false);
    std::map<std::string, std::string> user_name_map;
    for (const auto& u : all_users) {
        user_name_map[u.id] = u.name;
    }

    // 12. Save model + scaler + classes
    saveModel();

    // Also save classes file with user name map
    {
        nlohmann::json classes_json;
        classes_json["class_names"] = class_names_;
        nlohmann::json name_map;
        for (const auto& uid : class_names_) {
            auto it = user_name_map.find(uid);
            name_map[uid] = (it != user_name_map.end()) ? it->second : uid;
        }
        classes_json["user_name_map"] = name_map;

        std::ofstream f(modelPath("user_classifier_classes.json"));
        f << classes_json.dump(2);
    }
}

// ---------------------------------------------------------------------------
// Model persistence
// ---------------------------------------------------------------------------

void MLTrainingService::saveModel() {
    std::error_code ec;
    fs::create_directories(config_.model_dir, ec);

    // Save model
    {
        nlohmann::json j;
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            j = model_.toJson();
        }
        std::ofstream f(modelPath("user_classifier.json"));
        f << j.dump(2);
    }

    // Save scaler
    {
        nlohmann::json j;
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            j = scaler_.toJson();
        }
        std::ofstream f(modelPath("user_classifier_scaler.json"));
        f << j.dump(2);
    }

    spdlog::info("MLTrainingService: models saved to {}", config_.model_dir);
}

void MLTrainingService::loadModel() {
    auto model_path = modelPath("user_classifier.json");
    auto scaler_path = modelPath("user_classifier_scaler.json");
    auto classes_path = modelPath("user_classifier_classes.json");

    if (!fs::exists(model_path) || !fs::exists(scaler_path)) {
        spdlog::info("MLTrainingService: no existing models found in {}", config_.model_dir);
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        {
            std::ifstream f(model_path);
            nlohmann::json j = nlohmann::json::parse(f);
            model_ = ml::RandomForest::fromJson(j);
        }

        {
            std::ifstream f(scaler_path);
            nlohmann::json j = nlohmann::json::parse(f);
            scaler_ = ml::StandardScaler::fromJson(j);
        }

        if (fs::exists(classes_path)) {
            std::ifstream f(classes_path);
            nlohmann::json j = nlohmann::json::parse(f);
            class_names_ = j.at("class_names").get<std::vector<std::string>>();
        }

        spdlog::info("MLTrainingService: loaded existing models from {}", config_.model_dir);

    } catch (const std::exception& e) {
        spdlog::error("MLTrainingService: failed to load models: {}", e.what());
    }
}

}  // namespace hms_colada
