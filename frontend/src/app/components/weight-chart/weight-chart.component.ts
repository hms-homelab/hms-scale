import { Component, Input, ViewChild, ElementRef, AfterViewInit, OnChanges, OnDestroy, SimpleChanges } from '@angular/core';
import { Chart, registerables } from 'chart.js';
import zoomPlugin from 'chartjs-plugin-zoom';

Chart.register(...registerables, zoomPlugin);

@Component({
  selector: 'app-weight-chart',
  standalone: true,
  template: `
    <div class="chart-card">
      <div class="chart-header">
        <span class="chart-title">{{ title }}</span>
      </div>
      <div class="chart-container">
        <canvas #chartCanvas></canvas>
      </div>
    </div>
  `,
  styles: [`
    .chart-card {
      background: #1e1e2f;
      border-radius: 8px;
      padding: 16px;
    }
    .chart-header {
      margin-bottom: 8px;
    }
    .chart-title {
      color: #e0e0e0;
      font-size: 14px;
      font-weight: 600;
    }
    .chart-container {
      position: relative;
      height: 280px;
    }
  `]
})
export class WeightChartComponent implements AfterViewInit, OnChanges, OnDestroy {
  @Input() title = 'Weight Trend';
  @Input() labels: string[] = [];
  @Input() data: number[] = [];

  @ViewChild('chartCanvas') canvasRef!: ElementRef<HTMLCanvasElement>;
  private chart: Chart | null = null;
  private viewReady = false;

  ngAfterViewInit() {
    this.viewReady = true;
    this.renderChart();
  }

  ngOnChanges(_changes: SimpleChanges) {
    if (this.viewReady) {
      this.renderChart();
    }
  }

  ngOnDestroy() {
    this.chart?.destroy();
  }

  private renderChart() {
    if (!this.canvasRef?.nativeElement || !this.data.length) return;

    this.chart?.destroy();

    this.chart = new Chart(this.canvasRef.nativeElement, {
      type: 'line',
      data: {
        labels: this.labels,
        datasets: [{
          label: 'Weight (lbs)',
          data: this.data,
          borderColor: '#64b5f6',
          backgroundColor: 'rgba(100, 181, 246, 0.1)',
          borderWidth: 2,
          pointRadius: 4,
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
            borderColor: '#444',
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
            ticks: { color: '#888', font: { size: 10 }, maxRotation: 45 },
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
