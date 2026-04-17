#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct BodyCompositionResult {
    double body_fat_percentage = 0;
    double lean_mass_kg = 0;
    double muscle_mass_kg = 0;
    double bone_mass_kg = 0;
    double body_water_percentage = 0;
    int visceral_fat_rating = 0;
    double bmi = 0;
    int bmr_kcal = 0;
    int metabolic_age = 0;
    double protein_percentage = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BodyCompositionResult,
    body_fat_percentage, lean_mass_kg, muscle_mass_kg, bone_mass_kg,
    body_water_percentage, visceral_fat_rating, bmi, bmr_kcal,
    metabolic_age, protein_percentage)

struct ScaleUser {
    std::string id;                      // UUID
    std::string name;
    std::string date_of_birth;           // YYYY-MM-DD
    std::string sex;                     // "male" or "female"
    double height_cm = 0;
    double expected_weight_kg = 0;
    double weight_tolerance_kg = 3.0;
    bool is_active = true;
    std::string created_at;
    std::string updated_at;
    std::string last_measurement_at;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScaleUser,
    id, name, date_of_birth, sex, height_cm,
    expected_weight_kg, weight_tolerance_kg, is_active,
    created_at, updated_at, last_measurement_at)

struct ScaleMeasurement {
    std::string id;                      // UUID
    std::string user_id;                 // may be empty (unassigned)
    double weight_kg = 0;
    double weight_lbs = 0;
    double impedance_ohm = 0;           // 0 means no impedance
    BodyCompositionResult composition;
    double identification_confidence = 0;
    std::string identification_method;
    std::string measured_at;
    std::string created_at;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScaleMeasurement,
    id, user_id, weight_kg, weight_lbs, impedance_ohm,
    composition, identification_confidence, identification_method,
    measured_at, created_at)

struct DailyAverage {
    std::string date;
    std::string user_id;
    double avg_weight_kg = 0;
    double min_weight_kg = 0;
    double max_weight_kg = 0;
    double avg_body_fat = 0;
    int measurement_count = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DailyAverage,
    date, user_id, avg_weight_kg, min_weight_kg, max_weight_kg,
    avg_body_fat, measurement_count)

struct WeeklyTrend {
    std::string week_start;
    std::string user_id;
    double avg_weight_kg = 0;
    double weight_change_kg = 0;
    double avg_body_fat = 0;
    int measurement_count = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WeeklyTrend,
    week_start, user_id, avg_weight_kg, weight_change_kg,
    avg_body_fat, measurement_count)
