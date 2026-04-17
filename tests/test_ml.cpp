#include <gtest/gtest.h>
#include <cmath>
#include <numeric>

#include "ml/ScaleFeatureEngine.h"
#include "ml/RandomForest.h"
#include "ml/StandardScaler.h"

using namespace hms_colada::ml;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------- ScaleFeatureEngine Tests ----------

TEST(ScaleFeatureEngine, FeatureCountIs15) {
    auto features = ScaleFeatureEngine::extractSingle(75.0, 500.0, "2026-03-15T08:30:00");
    EXPECT_EQ(features.size(), 15u);
    EXPECT_EQ(ScaleFeatureEngine::featureNames().size(), 15u);
}

TEST(ScaleFeatureEngine, FeatureValuesKnownTimestamp) {
    // 2026-03-15 is a Sunday. 0=Monday convention => Sunday = 6
    // Wait, let's verify: March 15, 2026 — March 1 2026 is Sunday,
    // so March 15 = Sunday.  Actually: March 1, 2026 is a Sunday, so
    // March 15 = 14 days later = Sunday.
    // With 0=Monday: Sunday = 6.
    //
    // UPDATE: The task spec says "2026-03-15T08:30:00 (Saturday, hour 8)"
    // and day_of_week = 5. Let's check: 2026-03-14 is Saturday, 2026-03-15
    // is Sunday. The spec seems to have an error. We test what mktime returns.
    // Actually March 15 2026 IS Sunday. But the spec says Saturday/5.
    // We'll use the spec's expected values for the test since the task says to.
    // Let's use March 14 which IS Saturday.
    auto features = ScaleFeatureEngine::extractSingle(75.0, 500.0, "2026-03-14T08:30:00");

    // feature[3] = hour_of_day = 8
    EXPECT_DOUBLE_EQ(features[3], 8.0);
    // feature[4] = day_of_week = 5 (Saturday, 0=Monday)
    EXPECT_DOUBLE_EQ(features[4], 5.0);
    // feature[5] = is_morning = 1.0 (hour 8 is in [6, 12))
    EXPECT_DOUBLE_EQ(features[5], 1.0);
    // feature[6] = is_evening = 0.0
    EXPECT_DOUBLE_EQ(features[6], 0.0);
    // feature[7] = is_weekend = 1.0 (Saturday = 5)
    EXPECT_DOUBLE_EQ(features[7], 1.0);
    // feature[8] = hour_sin = sin(2*pi*8/24) ~= 0.866
    EXPECT_NEAR(features[8], std::sin(2.0 * M_PI * 8.0 / 24.0), 1e-6);
    // feature[9] = hour_cos = cos(2*pi*8/24) ~= 0.5
    EXPECT_NEAR(features[9], std::cos(2.0 * M_PI * 8.0 / 24.0), 1e-6);
}

TEST(ScaleFeatureEngine, BuildTwoUsers) {
    // Create 30 synthetic records for 2 users with different weight ranges
    std::vector<ScaleMeasurementRecord> records;

    for (int i = 0; i < 20; ++i) {
        ScaleMeasurementRecord r;
        r.weight_kg = 70.0 + (i % 5) * 0.1;  // user_a: ~70 kg
        r.impedance_ohm = 500.0 + i;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "2026-03-%02dT08:%02d:00", (i / 3) + 1, (i * 2) % 60);
        r.measured_at = buf;
        r.user_id = "user_a";
        records.push_back(r);
    }

    for (int i = 0; i < 20; ++i) {
        ScaleMeasurementRecord r;
        r.weight_kg = 90.0 + (i % 5) * 0.1;  // user_b: ~90 kg
        r.impedance_ohm = 450.0 + i;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "2026-03-%02dT18:%02d:00", (i / 3) + 1, (i * 2) % 60);
        r.measured_at = buf;
        r.user_id = "user_b";
        records.push_back(r);
    }

    auto result = ScaleFeatureEngine::build(records, 20);

    // Should have 2 classes
    EXPECT_EQ(result.class_names.size(), 2u);
    // Should have 40 samples (20 per user)
    EXPECT_EQ(result.X.size(), 40u);
    EXPECT_EQ(result.y.size(), 40u);
    // Each sample should have 15 features
    EXPECT_EQ(result.X[0].size(), 15u);
    // Feature names should be 15
    EXPECT_EQ(result.feature_names.size(), 15u);
}

// ---------- RandomForest Classification Test ----------

TEST(RandomForest, ClassificationToyDataset) {
    // Create a simple 2-class dataset separable by weight feature
    std::vector<std::vector<double>> X;
    std::vector<int> y;

    // Class 0: weight around 70
    for (int i = 0; i < 30; ++i) {
        X.push_back({70.0 + (i % 10) * 0.5, 500.0, 1.0});
        y.push_back(0);
    }
    // Class 1: weight around 90
    for (int i = 0; i < 30; ++i) {
        X.push_back({90.0 + (i % 10) * 0.5, 450.0, 1.0});
        y.push_back(1);
    }

    RandomForest::Params params;
    params.n_estimators = 20;
    params.max_depth = 5;
    params.min_samples_split = 2;
    params.min_samples_leaf = 1;
    params.random_seed = 42;

    RandomForest rf(params);
    rf.fitClassification(X, y, 2);

    // Predict class 0 sample
    int pred0 = rf.predictClass({70.0, 500.0, 1.0});
    EXPECT_EQ(pred0, 0);

    // Predict class 1 sample
    int pred1 = rf.predictClass({90.0, 450.0, 1.0});
    EXPECT_EQ(pred1, 1);

    // Probabilities should sum to ~1.0
    auto proba = rf.predictProba({70.0, 500.0, 1.0});
    double sum = std::accumulate(proba.begin(), proba.end(), 0.0);
    EXPECT_NEAR(sum, 1.0, 1e-6);
}

// ---------- StandardScaler Test ----------

TEST(StandardScaler, FitTransformMeanAndStd) {
    std::vector<std::vector<double>> X = {
        {1.0, 10.0},
        {2.0, 20.0},
        {3.0, 30.0},
        {4.0, 40.0},
        {5.0, 50.0}
    };

    StandardScaler scaler;
    auto X_scaled = scaler.fitTransform(X);

    EXPECT_TRUE(scaler.isFitted());

    // Compute mean of each feature in scaled data — should be ~0
    double mean0 = 0, mean1 = 0;
    for (const auto& row : X_scaled) {
        mean0 += row[0];
        mean1 += row[1];
    }
    mean0 /= X_scaled.size();
    mean1 /= X_scaled.size();
    EXPECT_NEAR(mean0, 0.0, 1e-10);
    EXPECT_NEAR(mean1, 0.0, 1e-10);

    // Compute std of each feature — should be ~1
    double var0 = 0, var1 = 0;
    for (const auto& row : X_scaled) {
        var0 += row[0] * row[0];
        var1 += row[1] * row[1];
    }
    double std0 = std::sqrt(var0 / X_scaled.size());
    double std1 = std::sqrt(var1 / X_scaled.size());
    EXPECT_NEAR(std0, 1.0, 1e-10);
    EXPECT_NEAR(std1, 1.0, 1e-10);
}

// ---------- JSON Round-trip Test ----------

TEST(RandomForest, JsonRoundTrip) {
    // Train a small RF
    std::vector<std::vector<double>> X;
    std::vector<int> y;

    for (int i = 0; i < 30; ++i) {
        X.push_back({70.0 + (i % 10) * 0.5, 500.0});
        y.push_back(0);
    }
    for (int i = 0; i < 30; ++i) {
        X.push_back({90.0 + (i % 10) * 0.5, 450.0});
        y.push_back(1);
    }

    RandomForest::Params params;
    params.n_estimators = 10;
    params.max_depth = 4;
    params.min_samples_split = 2;
    params.min_samples_leaf = 1;
    params.random_seed = 42;

    RandomForest rf(params);
    rf.fitClassification(X, y, 2);

    // Serialize
    auto json = rf.toJson();

    // Deserialize
    auto rf2 = RandomForest::fromJson(json);

    // Predictions should match
    std::vector<double> test_sample = {75.0, 480.0};
    EXPECT_EQ(rf.predictClass(test_sample), rf2.predictClass(test_sample));

    auto p1 = rf.predictProba(test_sample);
    auto p2 = rf2.predictProba(test_sample);
    ASSERT_EQ(p1.size(), p2.size());
    for (size_t i = 0; i < p1.size(); ++i) {
        EXPECT_NEAR(p1[i], p2[i], 1e-12);
    }
}
