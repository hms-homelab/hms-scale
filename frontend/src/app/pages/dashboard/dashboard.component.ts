import { Component, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatSelectModule } from '@angular/material/select';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MetricCardComponent } from '../../components/metric-card/metric-card.component';
import { WeightChartComponent } from '../../components/weight-chart/weight-chart.component';
import { BodyCompChartComponent } from '../../components/body-comp-chart/body-comp-chart.component';
import { ColadaApiService } from '../../services/colada-api.service';
import { ScaleUser, ScaleMeasurement, DailyAverage } from '../../models/user.model';

@Component({
  selector: 'app-dashboard',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatSelectModule,
    MatFormFieldModule,
    MetricCardComponent,
    WeightChartComponent,
    BodyCompChartComponent,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>Dashboard</h1>
        <mat-form-field appearance="outline" class="user-select">
          <mat-label>User</mat-label>
          <mat-select [(value)]="selectedUserId" (selectionChange)="onUserChange()">
            @for (user of users; track user.id) {
              <mat-option [value]="user.id">{{ user.name }}</mat-option>
            }
          </mat-select>
        </mat-form-field>
      </div>

      @if (latestMeasurement) {
        <div class="cards-row">
          <app-metric-card label="Weight" [value]="latestMeasurement.weight_lbs | number:'1.1-1'" unit="lbs" />
          <app-metric-card label="Body Fat" [value]="latestMeasurement.composition.body_fat_percentage | number:'1.1-1'" unit="%" />
          <app-metric-card label="Muscle Mass" [value]="latestMeasurement.composition.muscle_mass_kg * 2.20462 | number:'1.1-1'" unit="lbs" />
          <app-metric-card label="BMI" [value]="latestMeasurement.composition.bmi | number:'1.1-1'" unit="" />
          <app-metric-card label="BMR" [value]="latestMeasurement.composition.bmr_kcal | number:'1.0-0'" unit="kcal" />
          <app-metric-card label="Body Water" [value]="latestMeasurement.composition.body_water_percentage | number:'1.1-1'" unit="%" />
        </div>
      }

      <div class="charts-grid">
        <app-weight-chart
          title="Weight Trend (90 days)"
          [labels]="weightLabels"
          [data]="weightData"
        />
        @if (latestMeasurement) {
          <app-body-comp-chart
            [fatMass]="fatMass"
            [muscleMass]="muscleMass"
            [boneMass]="boneMass"
            [otherMass]="otherMass"
          />
        }
      </div>
    </div>
  `,
  styles: [`
    .page-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 24px;
      h1 { font-size: 24px; font-weight: 400; color: #fff; }
    }
    .user-select { width: 200px; }
    .charts-grid {
      display: grid;
      grid-template-columns: 2fr 1fr;
      gap: 16px;
    }
    @media (max-width: 768px) {
      .charts-grid { grid-template-columns: 1fr; }
      .page-header { flex-direction: column; gap: 12px; align-items: flex-start; }
    }
  `]
})
export class DashboardComponent implements OnInit {
  private api = inject(ColadaApiService);

  users: ScaleUser[] = [];
  selectedUserId = '';
  latestMeasurement: ScaleMeasurement | null = null;
  dailyAverages: DailyAverage[] = [];

  weightLabels: string[] = [];
  weightData: number[] = [];

  fatMass = 0;
  muscleMass = 0;
  boneMass = 0;
  otherMass = 0;

  ngOnInit(): void {
    this.api.getDashboard().subscribe({
      next: (data: any) => {
        this.users = data.users ?? [];
        if (this.users.length > 0) {
          this.selectedUserId = this.users[0].id;
          const firstUser = data.users[0];
          if (firstUser?.latest_measurement) {
            this.latestMeasurement = firstUser.latest_measurement;
            this.updateComposition();
          }
          this.loadUserData();
        }
      },
      error: () => {
        this.api.getUsers().subscribe(users => {
          this.users = users;
          if (users.length > 0) {
            this.selectedUserId = users[0].id;
            this.loadUserData();
          }
        });
      }
    });
  }

  onUserChange(): void {
    this.loadUserData();
  }

  private loadUserData(): void {
    if (!this.selectedUserId) return;

    this.api.getDailyAverages(this.selectedUserId, 90).subscribe(averages => {
      const sorted = [...averages].sort((a, b) => a.date.localeCompare(b.date));
      this.dailyAverages = sorted;
      this.weightLabels = sorted.map(a => {
        const d = new Date(a.date);
        return `${d.getMonth() + 1}/${d.getDate()}`;
      });
      this.weightData = sorted.map(a => a.avg_weight_kg * 2.20462);
    });

    this.api.getMeasurements(this.selectedUserId, 1).subscribe(measurements => {
      if (measurements.length > 0) {
        this.latestMeasurement = measurements[0];
        this.updateComposition();
      }
    });
  }

  private updateComposition(): void {
    if (!this.latestMeasurement?.composition) return;
    const c = this.latestMeasurement.composition;
    const totalWeight = this.latestMeasurement.weight_kg;
    this.fatMass = (c.body_fat_percentage / 100) * totalWeight;
    this.muscleMass = c.muscle_mass_kg;
    this.boneMass = c.bone_mass_kg;
    this.otherMass = Math.max(0, totalWeight - this.fatMass - this.muscleMass - this.boneMass);
  }
}
