import { Component, Input, ViewChild, ElementRef, AfterViewInit, OnChanges, OnDestroy } from '@angular/core';
import { Chart, registerables } from 'chart.js';

Chart.register(...registerables);

@Component({
  selector: 'app-body-comp-chart',
  standalone: true,
  template: `
    <div class="chart-card">
      <div class="chart-header">
        <span class="chart-title">Body Composition</span>
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
      margin-bottom: 8px;
    }
    .chart-title {
      color: #e0e0e0;
      font-size: 14px;
      font-weight: 600;
    }
    canvas { width: 100% !important; max-height: 280px; }
  `]
})
export class BodyCompChartComponent implements AfterViewInit, OnChanges, OnDestroy {
  @Input() fatMass = 0;
  @Input() muscleMass = 0;
  @Input() boneMass = 0;
  @Input() otherMass = 0;

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
    if (!this.canvasRef?.nativeElement) return;
    const total = this.fatMass + this.muscleMass + this.boneMass + this.otherMass;
    if (total <= 0) return;

    this.chart = new Chart(this.canvasRef.nativeElement, {
      type: 'doughnut',
      data: {
        labels: ['Fat', 'Muscle', 'Bone', 'Other'],
        datasets: [{
          data: [this.fatMass, this.muscleMass, this.boneMass, this.otherMass],
          backgroundColor: ['#ef5350', '#66bb6a', '#ffa726', '#42a5f5'],
          borderColor: '#1e1e2f',
          borderWidth: 2,
        }],
      },
      options: {
        responsive: true,
        maintainAspectRatio: true,
        plugins: {
          legend: {
            position: 'bottom',
            labels: { color: '#ccc', font: { size: 12 }, padding: 16 },
          },
          tooltip: {
            backgroundColor: '#1e1e2f',
            titleColor: '#e0e0e0',
            bodyColor: '#ccc',
            borderColor: '#333',
            borderWidth: 1,
            callbacks: {
              label: (ctx) => {
                const val = ctx.parsed;
                const pct = ((val / total) * 100).toFixed(1);
                return `${ctx.label}: ${val.toFixed(1)} kg (${pct}%)`;
              },
            },
          },
        },
      },
    });
  }
}
