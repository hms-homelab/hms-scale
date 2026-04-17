import { Component, Input, ViewChild, ElementRef, AfterViewInit, OnChanges, OnDestroy } from '@angular/core';
import { Chart, ChartDataset, registerables } from 'chart.js';
import annotationPlugin from 'chartjs-plugin-annotation';
import zoomPlugin from 'chartjs-plugin-zoom';

Chart.register(...registerables, annotationPlugin, zoomPlugin);

@Component({
  selector: 'app-weight-chart',
  standalone: true,
  template: `
    <div class="chart-card">
      <div class="chart-header">
        <span class="chart-title">{{ title }}</span>
      </div>
      <canvas #chartCanvas></canvas>
    </div>
  `,
  styles: [`
    .chart-card {
      background: #1e1e2f;
      border-radius: 8px;
      padding: 16px;
      margin-bottom: 16px;
    }
    .chart-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 8px;
    }
    .chart-title {
      color: #e0e0e0;
      font-size: 14px;
      font-weight: 600;
    }
    canvas { width: 100% !important; }
  `]
})
export class WeightChartComponent implements AfterViewInit, OnChanges, OnDestroy {
  @Input() title = 'Weight Trend';
  @Input() labels: string[] = [];
  @Input() data: number[] = [];
  @Input() height = 250;

  @ViewChild('chartCanvas') canvasRef!: ElementRef<HTMLCanvasElement>;
  private chart: Chart | null = null;

  ngAfterViewInit() {
    this.renderChart();
  }

  ngOnChanges() {
    if (this.chart) {
      this.chart.destroy();
      this.renderChart();
    }
  }

  ngOnDestroy() {
    this.chart?.destroy();
  }

  private renderChart() {
    if (!this.canvasRef?.nativeElement || !this.labels.length) return;
    const canvas = this.canvasRef.nativeElement;
    canvas.height = this.height;

    const skip = Math.max(1, Math.floor(this.labels.length / 15));

    this.chart = new Chart(canvas, {
      type: 'line',
      data: {
        labels: this.labels,
        datasets: [{
          label: 'Weight (lbs)',
          data: this.data,
          borderColor: '#64b5f6',
          backgroundColor: '#64b5f633',
          borderWidth: 2,
          pointRadius: 3,
          pointHitRadius: 6,
          pointBackgroundColor: '#64b5f6',
          tension: 0.3,
          fill: true,
        }],
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: { mode: 'index', intersect: false },
        plugins: {
          legend: { display: false },
          tooltip: {
            backgroundColor: '#1e1e2f',
            titleColor: '#e0e0e0',
            bodyColor: '#ccc',
            borderColor: '#333',
            borderWidth: 1,
          },
          zoom: {
            pan: { enabled: true, mode: 'x' },
            zoom: {
              wheel: { enabled: true },
              pinch: { enabled: true },
              mode: 'x',
            },
          },
        },
        scales: {
          x: {
            ticks: {
              color: '#888',
              font: { size: 10 },
              maxRotation: 45,
              autoSkip: false,
              callback: (_val: any, idx: number) => idx % skip === 0 ? this.labels[idx] : '',
            },
            grid: { color: '#2a2a3a' },
          },
          y: {
            ticks: { color: '#888', font: { size: 10 } },
            grid: { color: '#2a2a3a' },
          },
        },
      },
    });
  }
}
