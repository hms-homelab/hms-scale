import { Component, Input } from '@angular/core';

@Component({
  selector: 'app-metric-card',
  standalone: true,
  template: `
    <div class="metric-card">
      <div class="label">{{ label }}</div>
      <div class="value">{{ value }}</div>
      <div class="unit">{{ unit }}</div>
    </div>
  `,
  styles: [`
    .metric-card {
      background: #1e1e1e;
      border-radius: 12px;
      padding: 16px 20px;
      text-align: center;
      min-width: 140px;
    }
    .label {
      font-size: 11px;
      color: #999;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 4px;
    }
    .value {
      font-size: 28px;
      font-weight: 500;
      color: #fff;
      margin: 4px 0;
    }
    .unit {
      font-size: 12px;
      color: #666;
    }
  `]
})
export class MetricCardComponent {
  @Input() label = '';
  @Input() value: string | number | null = '-';
  @Input() unit = '';
}
