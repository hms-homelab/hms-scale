import { Component, OnInit, ViewChild, ElementRef, AfterViewInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { Chart, registerables } from 'chart.js';
import { ColadaApiService } from '../../services/colada-api.service';
import { MlStatus } from '../../models/user.model';
import { MetricCardComponent } from '../../components/metric-card/metric-card.component';

Chart.register(...registerables);

@Component({
  selector: 'app-ml-panel',
  standalone: true,
  imports: [
    CommonModule,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    MetricCardComponent,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>ML Model</h1>
        <button mat-raised-button color="primary" (click)="train()" [disabled]="training">
          @if (training) {
            <mat-spinner diameter="20" />
          } @else {
            <mat-icon>model_training</mat-icon>
          }
          Train Model
        </button>
      </div>

      @if (status) {
        <div class="cards-row">
          <app-metric-card label="Status" [value]="status.status" />
          <app-metric-card label="Accuracy" [value]="(status.accuracy * 100 | number:'1.1-1') + '%'" />
          <app-metric-card label="CV Accuracy" [value]="(status.cv_accuracy * 100 | number:'1.1-1') + '%'" />
          <app-metric-card label="Samples" [value]="status.n_samples" />
          <app-metric-card label="Users" [value]="status.n_users" />
        </div>

        @if (status.trained_at) {
          <div class="trained-at">
            Last trained: {{ status.trained_at | date:'medium' }}
          </div>
        }

        <div class="chart-card">
          <div class="chart-title">Feature Importance</div>
          <canvas #featureCanvas></canvas>
        </div>
      } @else {
        <div class="empty-state">
          <p>No ML model trained yet. Click "Train Model" to begin.</p>
        </div>
      }

      @if (trainMessage) {
        <div class="train-message" [class.error]="trainError">{{ trainMessage }}</div>
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
    .trained-at {
      color: #888;
      font-size: 13px;
      margin-bottom: 16px;
    }
    .chart-card {
      background: #1e1e2f;
      border-radius: 8px;
      padding: 16px;
    }
    .chart-title {
      color: #e0e0e0;
      font-size: 14px;
      font-weight: 600;
      margin-bottom: 8px;
    }
    .empty-state {
      text-align: center;
      padding: 48px;
      color: #888;
      font-size: 16px;
    }
    .train-message {
      margin-top: 16px;
      padding: 12px 16px;
      border-radius: 8px;
      background: #1b5e20;
      color: #c8e6c9;
      &.error { background: #b71c1c; color: #ffcdd2; }
    }
  `]
})
export class MlPanelComponent implements OnInit, AfterViewInit {
  private api = inject(ColadaApiService);

  @ViewChild('featureCanvas') featureCanvasRef!: ElementRef<HTMLCanvasElement>;

  status: MlStatus | null = null;
  training = false;
  trainMessage = '';
  trainError = false;
  private featureChart: Chart | null = null;

  ngOnInit(): void {
    this.loadStatus();
  }

  ngAfterViewInit(): void {
    // Chart rendered after status loads via loadStatus -> renderFeatureChart
  }

  loadStatus(): void {
    this.api.getMlStatus().subscribe({
      next: (s) => {
        this.status = s;
        setTimeout(() => this.renderFeatureChart(), 100);
      },
      error: () => { this.status = null; }
    });
  }

  train(): void {
    this.training = true;
    this.trainMessage = '';
    this.api.triggerTraining().subscribe({
      next: (res) => {
        this.training = false;
        this.trainMessage = res.message || 'Training complete';
        this.trainError = false;
        this.loadStatus();
      },
      error: (err) => {
        this.training = false;
        this.trainMessage = err.error?.message || 'Training failed';
        this.trainError = true;
      }
    });
  }

  private renderFeatureChart(): void {
    if (!this.featureCanvasRef?.nativeElement || !this.status?.feature_importance) return;
    this.featureChart?.destroy();

    const entries = Object.entries(this.status.feature_importance)
      .sort((a, b) => b[1] - a[1]);
    const labels = entries.map(e => e[0]);
    const data = entries.map(e => e[1]);

    this.featureChart = new Chart(this.featureCanvasRef.nativeElement, {
      type: 'bar',
      data: {
        labels,
        datasets: [{
          label: 'Importance',
          data,
          backgroundColor: '#64b5f6',
          borderRadius: 4,
        }],
      },
      options: {
        indexAxis: 'y',
        responsive: true,
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#1e1e2f',
            titleColor: '#e0e0e0',
            bodyColor: '#ccc',
          },
        },
        scales: {
          x: {
            ticks: { color: '#888' },
            grid: { color: '#2a2a3a' },
          },
          y: {
            ticks: { color: '#ccc', font: { size: 12 } },
            grid: { display: false },
          },
        },
      },
    });
  }
}
