#pragma once

/**
 * Body Composition Calculator
 * Orchestrates BIA formulas to produce comprehensive body composition results
 */

#include "BIAFormulas.h"
#include <string>

namespace hms_colada {

class BodyCompositionCalculator {
public:
    /**
     * Calculate all body composition metrics
     *
     * @param weight_kg Body weight in kg
     * @param height_cm Height in cm
     * @param impedance_ohm Bioelectrical impedance in ohms (0 = no BIA)
     * @param age Age in years
     * @param sex "male" or "female"
     * @return BodyCompositionResult with all calculated metrics
     */
    BodyCompositionResult calculate(double weight_kg, double height_cm,
                                    double impedance_ohm, int age,
                                    const std::string& sex) const;
};

} // namespace hms_colada
