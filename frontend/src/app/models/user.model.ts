export interface ScaleUser {
  id: string;
  name: string;
  date_of_birth: string;
  sex: string;
  height_cm: number;
  expected_weight_kg: number;
  weight_tolerance_kg: number;
  is_active: boolean;
  created_at: string;
  updated_at: string;
  last_measurement_at: string;
}

export interface ScaleMeasurement {
  id: string;
  user_id: string;
  weight_kg: number;
  weight_lbs: number;
  impedance_ohm: number;
  composition: BodyCompositionResult;
  identification_confidence: number;
  identification_method: string;
  measured_at: string;
  created_at: string;
}

export interface BodyCompositionResult {
  body_fat_percentage: number;
  lean_mass_kg: number;
  muscle_mass_kg: number;
  bone_mass_kg: number;
  body_water_percentage: number;
  visceral_fat_rating: number;
  bmi: number;
  bmr_kcal: number;
  metabolic_age: number;
  protein_percentage: number;
}

export interface DailyAverage {
  date: string;
  user_id: string;
  avg_weight_kg: number;
  min_weight_kg: number;
  max_weight_kg: number;
  avg_body_fat: number;
  measurement_count: number;
}

export interface WeeklyTrend {
  week_start: string;
  avg_weight_kg: number;
  avg_body_fat: number;
  avg_muscle_mass: number;
  avg_bmi: number;
  measurement_count: number;
}

export interface UserSummary {
  user: ScaleUser;
  latest_measurement: ScaleMeasurement | null;
  measurement_count: number;
  weight_change_30d: number;
  body_fat_change_30d: number;
}

export interface ProgressPoint {
  date: string;
  value: number;
}

export interface MlStatus {
  status: string;
  trained_at: string;
  accuracy: number;
  cv_accuracy: number;
  n_samples: number;
  n_users: number;
  feature_importance: Record<string, number>;
}

export interface MlPrediction {
  predicted_user_id: string;
  predicted_user_name: string;
  confidence: number;
  alternatives: Array<{ user_id: string; name: string; confidence: number }>;
}

export interface HabitInsights {
  consistency: {
    score: number;
    streak_days: number;
    avg_days_between: number;
    longest_gap_days: number;
  };
  weight_trend: {
    direction: string;
    change_kg: number;
    change_pct: number;
    predictions: Array<{ date: string; predicted_weight_kg: number }>;
  };
  body_composition: {
    fat_trend: string;
    muscle_trend: string;
    recomposition_status: string;
    fat_change_30d: number;
    muscle_change_30d: number;
  };
  recommendations: string[];
  alerts: string[];
}

export interface DashboardData {
  users: ScaleUser[];
  recent_measurements: ScaleMeasurement[];
  unassigned_count: number;
}
