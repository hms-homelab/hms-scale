import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { Observable } from 'rxjs';
import { map } from 'rxjs/operators';
import {
  ScaleUser,
  ScaleMeasurement,
  DailyAverage,
  WeeklyTrend,
  UserSummary,
  ProgressPoint,
  MlStatus,
  MlPrediction,
  HabitInsights,
  DashboardData,
} from '../models/user.model';

@Injectable({ providedIn: 'root' })
export class ColadaApiService {
  constructor(private http: HttpClient) {}

  // --- Users ---

  getUsers(): Observable<ScaleUser[]> {
    return this.http.get<{ users: ScaleUser[]; count: number }>('/api/users').pipe(
      map(r => r.users)
    );
  }

  createUser(user: Partial<ScaleUser>): Observable<ScaleUser> {
    return this.http.post<ScaleUser>('/api/users', user);
  }

  updateUser(id: string, user: Partial<ScaleUser>): Observable<ScaleUser> {
    return this.http.put<ScaleUser>(`/api/users/${id}`, user);
  }

  deleteUser(id: string): Observable<void> {
    return this.http.delete<void>(`/api/users/${id}`);
  }

  // --- Measurements ---

  getMeasurements(userId: string, days = 90): Observable<ScaleMeasurement[]> {
    const params = new HttpParams().set('user_id', userId).set('days', days);
    return this.http.get<{ measurements: ScaleMeasurement[]; count: number }>('/api/measurements', { params }).pipe(
      map(r => r.measurements)
    );
  }

  getUnassigned(): Observable<ScaleMeasurement[]> {
    return this.http.get<{ measurements: ScaleMeasurement[]; count: number }>('/api/measurements/unassigned').pipe(
      map(r => r.measurements)
    );
  }

  assignMeasurement(measurementId: string, userId: string): Observable<void> {
    return this.http.post<void>(`/api/measurements/${measurementId}/assign`, { user_id: userId });
  }

  // --- ML ---

  triggerTraining(): Observable<{ status: string; message: string }> {
    return this.http.post<{ status: string; message: string }>('/api/ml/train', {});
  }

  getMlStatus(): Observable<MlStatus> {
    return this.http.get<MlStatus>('/api/ml/status');
  }

  predict(weight: number, impedance: number): Observable<MlPrediction> {
    return this.http.post<MlPrediction>('/api/ml/predict', { weight_kg: weight, impedance_ohm: impedance });
  }

  // --- Analytics ---

  getDailyAverages(userId: string, days = 90): Observable<DailyAverage[]> {
    const params = new HttpParams().set('user_id', userId).set('days', days);
    return this.http.get<{ daily_averages: DailyAverage[] }>('/api/analytics/daily', { params }).pipe(
      map(r => r.daily_averages)
    );
  }

  getWeeklyTrends(userId: string, weeks = 12): Observable<WeeklyTrend[]> {
    const params = new HttpParams().set('user_id', userId).set('weeks', weeks);
    return this.http.get<{ weekly_trends: WeeklyTrend[] }>('/api/analytics/weekly', { params }).pipe(
      map(r => r.weekly_trends)
    );
  }

  getSummary(userId: string): Observable<UserSummary> {
    const params = new HttpParams().set('user_id', userId);
    return this.http.get<UserSummary>('/api/analytics/summary', { params });
  }

  getProgress(userId: string, days = 90, metric = 'weight_kg'): Observable<ProgressPoint[]> {
    const params = new HttpParams().set('user_id', userId).set('days', days).set('metric', metric);
    return this.http.get<{ data: ProgressPoint[] }>('/api/analytics/progress', { params }).pipe(
      map(r => r.data)
    );
  }

  // --- Habits ---

  getHabitInsights(userId: string, days = 90): Observable<HabitInsights> {
    const params = new HttpParams().set('user_id', userId).set('days', days);
    return this.http.get<HabitInsights>('/api/habits/insights', { params });
  }

  getHabitPredictions(userId: string, days = 30): Observable<any> {
    const params = new HttpParams().set('user_id', userId).set('days', days);
    return this.http.get('/api/habits/predictions', { params });
  }

  // --- Dashboard ---

  getDashboard(): Observable<DashboardData> {
    return this.http.get<DashboardData>('/api/dashboard');
  }
}
