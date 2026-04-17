import { Component, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatSelectModule } from '@angular/material/select';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatListModule } from '@angular/material/list';
import { ColadaApiService } from '../../services/colada-api.service';
import { ScaleUser, HabitInsights } from '../../models/user.model';
import { MetricCardComponent } from '../../components/metric-card/metric-card.component';

@Component({
  selector: 'app-habits',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatSelectModule,
    MatFormFieldModule,
    MatIconModule,
    MatListModule,
    MetricCardComponent,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>Habits & Insights</h1>
        <mat-form-field appearance="outline" class="user-select">
          <mat-label>User</mat-label>
          <mat-select [(value)]="selectedUserId" (selectionChange)="onUserChange()">
            @for (user of users; track user.id) {
              <mat-option [value]="user.id">{{ user.name }}</mat-option>
            }
          </mat-select>
        </mat-form-field>
      </div>

      @if (insights) {
        <!-- Consistency -->
        <section class="section">
          <h2>Consistency</h2>
          <div class="cards-row">
            <app-metric-card label="Score" [value]="insights.consistency.consistency_score | number:'1.0-0'" unit="/ 100" />
            <app-metric-card label="Streak" [value]="insights.consistency.current_streak_days" unit="days" />
            <app-metric-card label="Avg Gap" [value]="insights.consistency.avg_days_between | number:'1.1-1'" unit="days" />
            <app-metric-card label="Longest Gap" [value]="insights.consistency.longest_gap_days" unit="days" />
          </div>
        </section>

        <!-- Weight Trend -->
        <section class="section">
          <h2>Weight Trend</h2>
          <div class="cards-row">
            <app-metric-card label="Direction" [value]="insights.weight.trend_direction" />
            <app-metric-card label="Change" [value]="insights.weight.total_change_kg | number:'1.2-2'" unit="kg" />
            <app-metric-card label="Rate" [value]="insights.predictions.rate_kg_per_week | number:'1.2-2'" unit="kg/wk" />
          </div>

          <div class="card predictions">
            <h3>Predictions</h3>
            <div class="prediction-list">
              <div class="prediction-item">
                <span class="date">7 days</span>
                <span class="weight">{{ insights.predictions.predicted_weight_7d | number:'1.1-1' }} kg</span>
              </div>
              <div class="prediction-item">
                <span class="date">30 days</span>
                <span class="weight">{{ insights.predictions.predicted_weight_30d | number:'1.1-1' }} kg</span>
              </div>
              <div class="prediction-item">
                <span class="date">90 days</span>
                <span class="weight">{{ insights.predictions.predicted_weight_90d | number:'1.1-1' }} kg</span>
              </div>
            </div>
          </div>
        </section>

        <!-- Body Composition -->
        <section class="section">
          <h2>Body Composition</h2>
          <div class="cards-row">
            <app-metric-card label="Fat Trend" [value]="insights.body_comp.body_fat_trend" />
            <app-metric-card label="Muscle Trend" [value]="insights.body_comp.muscle_trend" />
            <app-metric-card label="Recomposing" [value]="insights.body_comp.is_recomposing ? 'Yes' : 'No'" />
            <app-metric-card label="Fat Change" [value]="insights.body_comp.body_fat_change | number:'1.1-1'" unit="%" />
            <app-metric-card label="Muscle Change" [value]="insights.body_comp.muscle_change_kg | number:'1.1-1'" unit="kg" />
          </div>
        </section>

        <!-- Recommendations -->
        @if (insights.recommendations.length) {
          <section class="section">
            <h2>Recommendations</h2>
            <mat-list class="insights-list">
              @for (rec of insights.recommendations; track rec.title) {
                <mat-list-item>
                  <mat-icon matListItemIcon>lightbulb</mat-icon>
                  <span matListItemTitle>{{ rec.title }}</span>
                  <span matListItemLine>{{ rec.message }}</span>
                </mat-list-item>
              }
            </mat-list>
          </section>
        }

        <!-- Alerts -->
        @if (insights.alerts.length) {
          <section class="section">
            <h2>Alerts</h2>
            <mat-list class="insights-list alerts">
              @for (alert of insights.alerts; track alert.message) {
                <mat-list-item>
                  <mat-icon matListItemIcon color="warn">warning</mat-icon>
                  <span matListItemTitle>{{ alert.message }}</span>
                </mat-list-item>
              }
            </mat-list>
          </section>
        }
      } @else if (selectedUserId) {
        <div class="empty-state">Loading habit insights...</div>
      } @else {
        <div class="empty-state">Select a user to view habit insights.</div>
      }
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
    .section {
      margin-bottom: 32px;
      h2 { font-size: 18px; font-weight: 400; color: #ccc; margin-bottom: 12px; }
    }
    .predictions {
      margin-top: 12px;
      h3 { font-size: 14px; color: #aaa; margin-bottom: 8px; }
    }
    .prediction-list {
      display: flex;
      flex-wrap: wrap;
      gap: 12px;
    }
    .prediction-item {
      background: #252538;
      border-radius: 6px;
      padding: 8px 14px;
      .date { color: #888; font-size: 12px; margin-right: 8px; }
      .weight { color: #fff; font-weight: 500; }
    }
    .insights-list {
      background: #1e1e1e;
      border-radius: 8px;
    }
    .empty-state {
      text-align: center;
      padding: 48px;
      color: #888;
      font-size: 16px;
    }
  `]
})
export class HabitsComponent implements OnInit {
  private api = inject(ColadaApiService);

  users: ScaleUser[] = [];
  selectedUserId = '';
  insights: HabitInsights | null = null;

  ngOnInit(): void {
    this.api.getUsers().subscribe(users => {
      this.users = users;
      if (users.length > 0) {
        this.selectedUserId = users[0].id;
        this.loadInsights();
      }
    });
  }

  onUserChange(): void {
    this.loadInsights();
  }

  private loadInsights(): void {
    if (!this.selectedUserId) return;
    this.insights = null;
    this.api.getHabitInsights(this.selectedUserId, 90).subscribe({
      next: (insights) => this.insights = insights,
      error: () => this.insights = null,
    });
  }
}
