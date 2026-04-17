#pragma once

#include "database/IScaleDatabase.h"
#include "ml/RandomForest.h"
#include "ml/StandardScaler.h"
#include "ml/ScaleFeatureEngine.h"
#include "ml/CrossValidator.h"

#include <json/json.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace hms_colada {

class MLTrainingService {
public:
    struct Config {
        bool enabled = false;
        std::string schedule = "weekly";  // daily, weekly, monthly
        std::string model_dir;            // default: ~/.hms-colada/models/
        int min_measurements = 20;        // per user minimum
    };

    MLTrainingService(IScaleDatabase* db, const Config& config);
    ~MLTrainingService();

    void start();   // Start background thread
    void stop();    // Stop gracefully

    void triggerTraining();          // Manual trigger from API
    Json::Value getStatus() const;   // Return training status + metrics

private:
    void runLoop();          // Background thread main loop
    bool shouldTrainNow();   // Check schedule
    void trainModel();       // Actual training logic
    void saveModel();
    void loadModel();

    IScaleDatabase* db_;
    Config config_;

    // ML components
    ml::RandomForest model_;
    ml::StandardScaler scaler_;
    std::vector<std::string> class_names_;

    // Training results
    struct TrainingMetrics {
        double accuracy = 0;
        double cv_accuracy = 0;
        double cv_std = 0;
        int n_samples = 0;
        int n_users = 0;
        std::string trained_at;
        std::map<std::string, double> feature_importance;
    } metrics_;

    // Threading
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> train_requested_{false};
    mutable std::mutex metrics_mutex_;
    std::string last_error_;
    std::string status_ = "idle";  // idle, training, complete, error

    // Schedule tracking
    std::chrono::system_clock::time_point last_trained_;

    // Helpers
    std::string modelPath(const std::string& filename) const;
    static std::string currentTimestamp();
    static std::string defaultModelDir();
};

}  // namespace hms_colada
