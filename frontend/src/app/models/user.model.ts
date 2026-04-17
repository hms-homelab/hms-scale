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
  schedule: string;
  metrics?: {
    accuracy: number;
    cv_accuracy: number;
    cv_std: number;
    n_samples: number;
    n_users: number;
    trained_at: string;
    feature_importance: Record<string, number>;
  };
}

export interface MlPrediction {
  predicted_user_id: string;
  predicted_user_name: string;
  confidence: number;
  alternatives: Array<{ user_id: string; name: string; confidence: number }>;
}

export interface HabitInsights {
  consistency: {
    consistency_score: number;
    current_streak_days: number;
    max_streak_days: number;
    avg_days_between: number;
    longest_gap_days: number;
    total_measurements: number;
  };
  weight: {
    current_weight: number;
    starting_weight: number;
    total_change_kg: number;
    trend_direction: string;
    min_weight: number;
    max_weight: number;
    weight_range: number;
    slope_kg_per_measurement: number;
    volatility: number;
    is_volatile: boolean;
    avg_7d: number;
    avg_30d: number;
  };
  predictions: {
    predicted_weight_7d: number;
    predicted_weight_30d: number;
    predicted_weight_90d: number;
    rate_kg_per_week: number;
    rate_lbs_per_week: number;
    trend_confidence: string;
  };
  body_comp: {
    body_fat_change: number;
    muscle_change_kg: number;
    body_fat_trend: string;
    muscle_trend: string;
    is_recomposing: boolean;
  };
  weekly: {
    heaviest_day: string;
    lightest_day: string;
    weekly_variation_kg: number;
    has_weekend_effect: boolean;
    day_averages: Record<string, number>;
  };
  recommendations: Array<{ category: string; priority: string; title: string; message: string; actionable_step: string }>;
  alerts: Array<{ type: string; severity: string; message: string }>;
}

export interface DashboardData {
  users: any[];
  total_users: number;
  total_measurements: number;
}
