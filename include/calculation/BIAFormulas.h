#pragma once

/**
 * Bioelectrical Impedance Analysis (BIA) Formulas
 * Header-only validated scientific equations for body composition estimation
 *
 * References:
 * - Kyle et al. (2004): Body fat percentage
 * - Janssen et al. (2000): Muscle mass estimation
 * - Watson et al. (1980): Total body water
 * - Mifflin-St Jeor (1990): Basal metabolic rate
 * - Hologic: DXA bone mass estimation
 */

#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>

#include "models/ScaleModels.h"

namespace hms_colada {
namespace bia {

/// Calculate age from date of birth string "YYYY-MM-DD"
inline int calculateAge(const std::string& date_of_birth) {
    int year, month, day;
    std::sscanf(date_of_birth.c_str(), "%d-%d-%d", &year, &month, &day);

    std::time_t now = std::time(nullptr);
    std::tm* today = std::localtime(&now);

    int age = (today->tm_year + 1900) - year;
    // Subtract 1 if birthday hasn't occurred yet this year
    if ((today->tm_mon + 1) < month ||
        ((today->tm_mon + 1) == month && today->tm_mday < day)) {
        age--;
    }
    return age;
}

/// BMI = weight / height_m^2
inline double calculateBMI(double weight_kg, double height_cm) {
    double height_m = height_cm / 100.0;
    return weight_kg / (height_m * height_m);
}

/**
 * Kyle et al. (2004) - Body fat percentage
 * Male:   BF% = 0.8*BMI + 0.14*age - 0.02*(height_cm^2/impedance) - 12.0
 * Female: BF% = 0.9*BMI + 0.12*age - 0.018*(height_cm^2/impedance) - 9.0
 * Clamped [5.0, 50.0]
 */
inline double calculateBodyFatKyle(double weight_kg, double height_cm,
                                    double impedance_ohm, int age,
                                    const std::string& sex) {
    double bmi = calculateBMI(weight_kg, height_cm);
    double impedance_index = (height_cm * height_cm) / impedance_ohm;

    double body_fat_pct;
    if (sex == "male") {
        body_fat_pct = 0.8 * bmi + 0.14 * age - 0.02 * impedance_index - 12.0;
    } else {
        body_fat_pct = 0.9 * bmi + 0.12 * age - 0.018 * impedance_index - 9.0;
    }

    return std::clamp(body_fat_pct, 5.0, 50.0);
}

/**
 * Janssen et al. (2000) - Skeletal muscle mass
 * SM = (height_cm^2/impedance * 0.401) + (sex_factor * 3.825) + (age * -0.071) + 5.102
 * sex_factor: 1=male, 0=female. Clamped [30%-80% of body weight]
 */
inline double calculateMuscleMassJanssen(double weight_kg, double height_cm,
                                          double impedance_ohm, int age,
                                          const std::string& sex) {
    double sex_factor = (sex == "male") ? 1.0 : 0.0;

    double muscle_mass_kg =
        ((height_cm * height_cm) / impedance_ohm) * 0.401 +
        (sex_factor * 3.825) +
        (age * -0.071) +
        5.102;

    double min_muscle = weight_kg * 0.30;
    double max_muscle = weight_kg * 0.80;

    return std::clamp(muscle_mass_kg, min_muscle, max_muscle);
}

/**
 * Watson et al. (1980) - Total body water percentage
 * Male:   TBW_L = 2.447 - 0.09156*age + 0.1074*height_cm + 0.3362*weight
 * Female: TBW_L = -2.097 + 0.1069*height_cm + 0.2466*weight
 * Result: (TBW_L / weight) * 100, clamped [40%, 75%]
 */
inline double calculateBodyWaterWatson(double weight_kg, double height_cm,
                                        int age, const std::string& sex) {
    double tbw_liters;
    if (sex == "male") {
        tbw_liters = 2.447 - (0.09156 * age) + (0.1074 * height_cm) +
                     (0.3362 * weight_kg);
    } else {
        tbw_liters = -2.097 + (0.1069 * height_cm) + (0.2466 * weight_kg);
    }

    double tbw_percentage = (tbw_liters / weight_kg) * 100.0;
    return std::clamp(tbw_percentage, 40.0, 75.0);
}

/**
 * Mifflin-St Jeor (1990) - BMR kcal/day
 * Male:   10*weight + 6.25*height_cm - 5*age + 5
 * Female: 10*weight + 6.25*height_cm - 5*age - 161
 */
inline int calculateBMRMifflin(double weight_kg, double height_cm, int age,
                                const std::string& sex) {
    double bmr;
    if (sex == "male") {
        bmr = (10.0 * weight_kg) + (6.25 * height_cm) - (5.0 * age) + 5.0;
    } else {
        bmr = (10.0 * weight_kg) + (6.25 * height_cm) - (5.0 * age) - 161.0;
    }
    return static_cast<int>(bmr);
}

/**
 * Hologic DXA bone mass estimation
 * Male: bone = weight * 0.055, Female: bone = weight * 0.045
 * Adjusted: bone * (height_m / 1.70)
 */
inline double calculateBoneMassHologic(double weight_kg, double height_cm,
                                        const std::string& sex) {
    double height_m = height_cm / 100.0;
    double bone_mass_kg;

    if (sex == "male") {
        bone_mass_kg = weight_kg * 0.055;
    } else {
        bone_mass_kg = weight_kg * 0.045;
    }

    double height_factor = height_m / 1.70;
    bone_mass_kg *= height_factor;

    // Round to 2 decimal places (matching Python round(x, 2))
    return std::round(bone_mass_kg * 100.0) / 100.0;
}

/**
 * Omron-style visceral fat rating (1-30)
 * base = (BMI-20)*0.5 + (BF%-15)*0.3
 * +age adj: (age-30)*0.1 if age>30
 * +sex adj: 2.0 if male
 * Clamped [1, 30]
 */
inline int calculateVisceralFatRating(double bmi, double body_fat_pct, int age,
                                       const std::string& sex) {
    double base_rating = (bmi - 20.0) * 0.5 + (body_fat_pct - 15.0) * 0.3;
    double age_adjustment = (age > 30) ? (age - 30) * 0.1 : 0.0;
    double sex_adjustment = (sex == "male") ? 2.0 : 0.0;

    double rating = base_rating + age_adjustment + sex_adjustment;
    return std::clamp(static_cast<int>(rating), 1, 30);
}

/**
 * Metabolic age
 * Expected BMR: Male: 1650-(age-25)*3, Female: 1350-(age-25)*2.5
 * metabolic_age = age - int((actual_BMR - expected_BMR) / 50.0)
 * Clamped [15, 85]
 */
inline int calculateMetabolicAge(int bmr_kcal, int age,
                                  const std::string& sex) {
    double expected_bmr;
    if (sex == "male") {
        expected_bmr = 1650.0 - (age - 25) * 3.0;
    } else {
        expected_bmr = 1350.0 - (age - 25) * 2.5;
    }

    double bmr_difference = bmr_kcal - expected_bmr;
    int age_adjustment_years = static_cast<int>(bmr_difference / 50.0);

    int metabolic_age = age - age_adjustment_years;
    return std::clamp(metabolic_age, 15, 85);
}

/// Protein % = (lean_mass * 0.20 / weight) * 100
inline double calculateProteinPercentage(double weight_kg,
                                          double lean_mass_kg) {
    double protein_kg = lean_mass_kg * 0.20;
    double protein_pct = (protein_kg / weight_kg) * 100.0;
    // Round to 1 decimal place (matching Python round(x, 1))
    return std::round(protein_pct * 10.0) / 10.0;
}

/// Lean mass = weight - (weight * body_fat_pct / 100)
inline double calculateLeanMass(double weight_kg, double body_fat_pct) {
    return weight_kg - (weight_kg * body_fat_pct / 100.0);
}

} // namespace bia
} // namespace hms_colada
