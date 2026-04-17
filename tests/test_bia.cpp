#include <gtest/gtest.h>
#include "calculation/BIAFormulas.h"
#include "calculation/BodyCompositionCalculator.h"

using namespace hms_colada;
using namespace hms_colada::bia;

// =============================================================================
// BMI Tests
// =============================================================================

TEST(BIAFormulas, BMI_Male_80kg_180cm) {
    EXPECT_NEAR(calculateBMI(80, 180), 24.6914, 0.001);
}

TEST(BIAFormulas, BMI_Female_60kg_165cm) {
    EXPECT_NEAR(calculateBMI(60, 165), 22.0386, 0.001);
}

TEST(BIAFormulas, BMI_HighWeight_130kg_170cm) {
    EXPECT_NEAR(calculateBMI(130, 170), 44.9827, 0.001);
}

// =============================================================================
// Body Fat Kyle Tests
// =============================================================================

TEST(BIAFormulas, BodyFatKyle_Male_30y_180cm_80kg_500ohm) {
    EXPECT_NEAR(calculateBodyFatKyle(80, 180, 500, 30, "male"), 10.6571, 0.001);
}

TEST(BIAFormulas, BodyFatKyle_Female_25y_165cm_60kg_550ohm) {
    EXPECT_NEAR(calculateBodyFatKyle(60, 165, 550, 25, "female"), 12.9437, 0.001);
}

TEST(BIAFormulas, BodyFatKyle_HighBMI_Male_50y_170cm_130kg_600ohm) {
    EXPECT_NEAR(calculateBodyFatKyle(130, 170, 600, 50, "male"), 30.0228, 0.001);
}

TEST(BIAFormulas, BodyFatKyle_Young_Male_18y) {
    EXPECT_NEAR(calculateBodyFatKyle(70, 175, 480, 18, "male"), 7.5297, 0.001);
}

TEST(BIAFormulas, BodyFatKyle_Old_Female_80y) {
    EXPECT_NEAR(calculateBodyFatKyle(55, 160, 600, 80, "female"), 19.1679, 0.001);
}

TEST(BIAFormulas, BodyFatKyle_ClampedLow) {
    // Very low impedance (high lean mass) should clamp to 5.0
    double result = calculateBodyFatKyle(80, 190, 200, 18, "male");
    EXPECT_GE(result, 5.0);
}

TEST(BIAFormulas, BodyFatKyle_ClampedHigh) {
    // Very high impedance + high BMI should clamp to 50.0
    double result = calculateBodyFatKyle(150, 150, 1000, 80, "female");
    EXPECT_LE(result, 50.0);
}

// =============================================================================
// Muscle Mass Janssen Tests
// =============================================================================

TEST(BIAFormulas, MuscleMassJanssen_Male_30y_180cm_80kg_500ohm) {
    EXPECT_NEAR(calculateMuscleMassJanssen(80, 180, 500, 30, "male"), 32.7818, 0.001);
}

TEST(BIAFormulas, MuscleMassJanssen_Female_25y_165cm_60kg_550ohm) {
    EXPECT_NEAR(calculateMuscleMassJanssen(60, 165, 550, 25, "female"), 23.1765, 0.001);
}

TEST(BIAFormulas, MuscleMassJanssen_HighBMI_Clamped) {
    // 130kg male - muscle should be clamped to [39, 104]
    double result = calculateMuscleMassJanssen(130, 170, 600, 50, "male");
    EXPECT_NEAR(result, 39.0, 0.001);  // Clamped to 30% of 130kg
}

TEST(BIAFormulas, MuscleMassJanssen_Old_Female) {
    EXPECT_NEAR(calculateMuscleMassJanssen(55, 160, 600, 80, "female"), 16.5313, 0.001);
}

// =============================================================================
// Body Water Watson Tests
// =============================================================================

TEST(BIAFormulas, BodyWaterWatson_Male_30y_180cm_80kg) {
    EXPECT_NEAR(calculateBodyWaterWatson(80, 180, 30, "male"), 57.4103, 0.001);
}

TEST(BIAFormulas, BodyWaterWatson_Female_25y_165cm_60kg) {
    EXPECT_NEAR(calculateBodyWaterWatson(60, 165, 25, "female"), 50.5625, 0.001);
}

TEST(BIAFormulas, BodyWaterWatson_HighBMI_Male) {
    EXPECT_NEAR(calculateBodyWaterWatson(130, 170, 50, "male"), 46.0254, 0.001);
}

TEST(BIAFormulas, BodyWaterWatson_ClampedRange) {
    double result = calculateBodyWaterWatson(80, 180, 30, "male");
    EXPECT_GE(result, 40.0);
    EXPECT_LE(result, 75.0);
}

// =============================================================================
// BMR Mifflin Tests
// =============================================================================

TEST(BIAFormulas, BMRMifflin_Male_30y_180cm_80kg) {
    EXPECT_EQ(calculateBMRMifflin(80, 180, 30, "male"), 1780);
}

TEST(BIAFormulas, BMRMifflin_Female_25y_165cm_60kg) {
    EXPECT_EQ(calculateBMRMifflin(60, 165, 25, "female"), 1345);
}

TEST(BIAFormulas, BMRMifflin_HighBMI_Male) {
    EXPECT_EQ(calculateBMRMifflin(130, 170, 50, "male"), 2117);
}

TEST(BIAFormulas, BMRMifflin_Young_Male_18y) {
    EXPECT_EQ(calculateBMRMifflin(70, 175, 18, "male"), 1708);
}

TEST(BIAFormulas, BMRMifflin_Old_Female_80y) {
    EXPECT_EQ(calculateBMRMifflin(55, 160, 80, "female"), 989);
}

// =============================================================================
// Bone Mass Hologic Tests
// =============================================================================

TEST(BIAFormulas, BoneMassHologic_Male_80kg_180cm) {
    EXPECT_NEAR(calculateBoneMassHologic(80, 180, "male"), 4.66, 0.01);
}

TEST(BIAFormulas, BoneMassHologic_Female_60kg_165cm) {
    EXPECT_NEAR(calculateBoneMassHologic(60, 165, "female"), 2.62, 0.01);
}

TEST(BIAFormulas, BoneMassHologic_HighBMI_Male) {
    EXPECT_NEAR(calculateBoneMassHologic(130, 170, "male"), 7.15, 0.01);
}

// =============================================================================
// Visceral Fat Rating Tests
// =============================================================================

TEST(BIAFormulas, VisceralFat_Male_30y) {
    double bmi = calculateBMI(80, 180);
    double bf = calculateBodyFatKyle(80, 180, 500, 30, "male");
    EXPECT_EQ(calculateVisceralFatRating(bmi, bf, 30, "male"), 3);
}

TEST(BIAFormulas, VisceralFat_Female_25y) {
    double bmi = calculateBMI(60, 165);
    double bf = calculateBodyFatKyle(60, 165, 550, 25, "female");
    EXPECT_EQ(calculateVisceralFatRating(bmi, bf, 25, "female"), 1);
}

TEST(BIAFormulas, VisceralFat_HighBMI_Male_50y) {
    double bmi = calculateBMI(130, 170);
    double bf = calculateBodyFatKyle(130, 170, 600, 50, "male");
    EXPECT_EQ(calculateVisceralFatRating(bmi, bf, 50, "male"), 20);
}

TEST(BIAFormulas, VisceralFat_Young_Male_18y) {
    double bmi = calculateBMI(70, 175);
    double bf = calculateBodyFatKyle(70, 175, 480, 18, "male");
    EXPECT_EQ(calculateVisceralFatRating(bmi, bf, 18, "male"), 1);
}

TEST(BIAFormulas, VisceralFat_Old_Female_80y) {
    double bmi = calculateBMI(55, 160);
    double bf = calculateBodyFatKyle(55, 160, 600, 80, "female");
    EXPECT_EQ(calculateVisceralFatRating(bmi, bf, 80, "female"), 6);
}

TEST(BIAFormulas, VisceralFat_ClampedLow) {
    // Very low BMI and BF should clamp to 1
    EXPECT_EQ(calculateVisceralFatRating(15.0, 5.0, 18, "female"), 1);
}

TEST(BIAFormulas, VisceralFat_ClampedHigh) {
    // Very high BMI and BF should clamp to 30
    EXPECT_EQ(calculateVisceralFatRating(50.0, 50.0, 80, "male"), 30);
}

// =============================================================================
// Metabolic Age Tests
// =============================================================================

TEST(BIAFormulas, MetabolicAge_Male_30y) {
    EXPECT_EQ(calculateMetabolicAge(1780, 30, "male"), 28);
}

TEST(BIAFormulas, MetabolicAge_Female_25y) {
    EXPECT_EQ(calculateMetabolicAge(1345, 25, "female"), 25);
}

TEST(BIAFormulas, MetabolicAge_HighBMI_Male_50y) {
    EXPECT_EQ(calculateMetabolicAge(2117, 50, "male"), 40);
}

TEST(BIAFormulas, MetabolicAge_Young_Male_18y) {
    EXPECT_EQ(calculateMetabolicAge(1708, 18, "male"), 18);
}

TEST(BIAFormulas, MetabolicAge_Old_Female_80y) {
    EXPECT_EQ(calculateMetabolicAge(989, 80, "female"), 84);
}

TEST(BIAFormulas, MetabolicAge_ClampedLow) {
    // Very high BMR should clamp metabolic age to 15
    EXPECT_EQ(calculateMetabolicAge(5000, 20, "male"), 15);
}

TEST(BIAFormulas, MetabolicAge_ClampedHigh) {
    // Very low BMR should clamp metabolic age to 85
    EXPECT_EQ(calculateMetabolicAge(100, 80, "male"), 85);
}

// =============================================================================
// Lean Mass & Protein Tests
// =============================================================================

TEST(BIAFormulas, LeanMass_Male) {
    double bf = calculateBodyFatKyle(80, 180, 500, 30, "male");
    EXPECT_NEAR(calculateLeanMass(80, bf), 71.4743, 0.001);
}

TEST(BIAFormulas, LeanMass_Female) {
    double bf = calculateBodyFatKyle(60, 165, 550, 25, "female");
    EXPECT_NEAR(calculateLeanMass(60, bf), 52.2338, 0.001);
}

TEST(BIAFormulas, ProteinPercentage_Male) {
    double bf = calculateBodyFatKyle(80, 180, 500, 30, "male");
    double lean = calculateLeanMass(80, bf);
    EXPECT_NEAR(calculateProteinPercentage(80, lean), 17.9, 0.1);
}

TEST(BIAFormulas, ProteinPercentage_Female) {
    double bf = calculateBodyFatKyle(60, 165, 550, 25, "female");
    double lean = calculateLeanMass(60, bf);
    EXPECT_NEAR(calculateProteinPercentage(60, lean), 17.4, 0.1);
}

// =============================================================================
// BodyCompositionCalculator Tests
// =============================================================================

TEST(BodyCompositionCalculator, FullCalculation_Male) {
    BodyCompositionCalculator calc;
    auto result = calc.calculate(80, 180, 500, 30, "male");

    EXPECT_NEAR(result.bmi, 24.6914, 0.001);
    EXPECT_NEAR(result.body_fat_percentage, 10.6571, 0.001);
    EXPECT_NEAR(result.muscle_mass_kg, 32.7818, 0.001);
    EXPECT_NEAR(result.body_water_percentage, 57.4103, 0.001);
    EXPECT_EQ(result.bmr_kcal, 1780);
    EXPECT_NEAR(result.bone_mass_kg, 4.66, 0.01);
    EXPECT_EQ(result.visceral_fat_rating, 3);
    EXPECT_EQ(result.metabolic_age, 28);
    EXPECT_NEAR(result.lean_mass_kg, 71.4743, 0.001);
    EXPECT_NEAR(result.protein_percentage, 17.9, 0.1);
}

TEST(BodyCompositionCalculator, FullCalculation_Female) {
    BodyCompositionCalculator calc;
    auto result = calc.calculate(60, 165, 550, 25, "female");

    EXPECT_NEAR(result.bmi, 22.0386, 0.001);
    EXPECT_NEAR(result.body_fat_percentage, 12.9437, 0.001);
    EXPECT_NEAR(result.muscle_mass_kg, 23.1765, 0.001);
    EXPECT_NEAR(result.body_water_percentage, 50.5625, 0.001);
    EXPECT_EQ(result.bmr_kcal, 1345);
    EXPECT_NEAR(result.bone_mass_kg, 2.62, 0.01);
    EXPECT_EQ(result.visceral_fat_rating, 1);
    EXPECT_EQ(result.metabolic_age, 25);
    EXPECT_NEAR(result.lean_mass_kg, 52.2338, 0.001);
    EXPECT_NEAR(result.protein_percentage, 17.4, 0.1);
}

TEST(BodyCompositionCalculator, NoImpedance_BMIOnly) {
    BodyCompositionCalculator calc;
    auto result = calc.calculate(80, 180, 0, 30, "male");

    // BMI should be calculated
    EXPECT_NEAR(result.bmi, 24.6914, 0.001);

    // All BIA metrics should be zero/default
    EXPECT_DOUBLE_EQ(result.body_fat_percentage, 0);
    EXPECT_DOUBLE_EQ(result.lean_mass_kg, 0);
    EXPECT_DOUBLE_EQ(result.muscle_mass_kg, 0);
    EXPECT_DOUBLE_EQ(result.bone_mass_kg, 0);
    EXPECT_DOUBLE_EQ(result.body_water_percentage, 0);
    EXPECT_EQ(result.visceral_fat_rating, 0);
    EXPECT_EQ(result.bmr_kcal, 0);
    EXPECT_EQ(result.metabolic_age, 0);
    EXPECT_DOUBLE_EQ(result.protein_percentage, 0);
}

TEST(BodyCompositionCalculator, NegativeImpedance_BMIOnly) {
    BodyCompositionCalculator calc;
    auto result = calc.calculate(80, 180, -100, 30, "male");

    EXPECT_NEAR(result.bmi, 24.6914, 0.001);
    EXPECT_DOUBLE_EQ(result.body_fat_percentage, 0);
}

TEST(BodyCompositionCalculator, HighBMI_Male_50y) {
    BodyCompositionCalculator calc;
    auto result = calc.calculate(130, 170, 600, 50, "male");

    EXPECT_NEAR(result.bmi, 44.9827, 0.001);
    EXPECT_NEAR(result.body_fat_percentage, 30.0228, 0.001);
    EXPECT_NEAR(result.muscle_mass_kg, 39.0, 0.001);
    EXPECT_NEAR(result.body_water_percentage, 46.0254, 0.001);
    EXPECT_EQ(result.bmr_kcal, 2117);
    EXPECT_NEAR(result.bone_mass_kg, 7.15, 0.01);
    EXPECT_EQ(result.visceral_fat_rating, 20);
    EXPECT_EQ(result.metabolic_age, 40);
}

// =============================================================================
// calculateAge Tests (basic validation -- exact result depends on current date)
// =============================================================================

TEST(BIAFormulas, CalculateAge_NotNegative) {
    int age = calculateAge("2000-01-01");
    EXPECT_GT(age, 0);
    EXPECT_LT(age, 200);
}

TEST(BIAFormulas, CalculateAge_Recent) {
    // Someone born in 1990 should be ~35-36 in 2026
    int age = calculateAge("1990-06-15");
    EXPECT_GE(age, 35);
    EXPECT_LE(age, 37);
}
