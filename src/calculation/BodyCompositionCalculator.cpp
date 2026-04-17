#include "calculation/BodyCompositionCalculator.h"

namespace hms_colada {

BodyCompositionResult BodyCompositionCalculator::calculate(
    double weight_kg, double height_cm, double impedance_ohm, int age,
    const std::string& sex) const {

    BodyCompositionResult result;

    // BMI is always calculable with weight + height
    result.bmi = bia::calculateBMI(weight_kg, height_cm);

    // If no impedance data, return BMI-only result
    if (impedance_ohm <= 0) {
        return result;
    }

    // 1. Body Fat Percentage (Kyle formula)
    result.body_fat_percentage =
        bia::calculateBodyFatKyle(weight_kg, height_cm, impedance_ohm, age, sex);

    // 2. Muscle Mass (Janssen formula)
    result.muscle_mass_kg =
        bia::calculateMuscleMassJanssen(weight_kg, height_cm, impedance_ohm, age, sex);

    // 3. Body Water Percentage (Watson formula)
    result.body_water_percentage =
        bia::calculateBodyWaterWatson(weight_kg, height_cm, age, sex);

    // 4. BMR (Mifflin-St Jeor)
    result.bmr_kcal =
        bia::calculateBMRMifflin(weight_kg, height_cm, age, sex);

    // 5. Bone Mass (Hologic estimation)
    result.bone_mass_kg =
        bia::calculateBoneMassHologic(weight_kg, height_cm, sex);

    // 6. Visceral Fat Rating (Omron-style)
    result.visceral_fat_rating =
        bia::calculateVisceralFatRating(result.bmi, result.body_fat_percentage, age, sex);

    // 7. Metabolic Age
    result.metabolic_age =
        bia::calculateMetabolicAge(result.bmr_kcal, age, sex);

    // 8. Lean Mass (derived from body fat)
    result.lean_mass_kg =
        bia::calculateLeanMass(weight_kg, result.body_fat_percentage);

    // 9. Protein Percentage
    result.protein_percentage =
        bia::calculateProteinPercentage(weight_kg, result.lean_mass_kg);

    return result;
}

} // namespace hms_colada
