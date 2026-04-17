import { Injectable, signal, computed } from '@angular/core';

@Injectable({ providedIn: 'root' })
export class UnitService {
  private useImperial = signal(true);

  isLbs = computed(() => this.useImperial());
  weightUnit = computed(() => this.useImperial() ? 'lbs' : 'kg');
  heightUnit = computed(() => this.useImperial() ? 'ft' : 'cm');
  systemLabel = computed(() => this.useImperial() ? 'lbs' : 'kg');

  constructor() {
    const saved = localStorage.getItem('weight_unit');
    if (saved === 'kg') this.useImperial.set(false);
  }

  toggle(): void {
    this.useImperial.update(v => !v);
    localStorage.setItem('weight_unit', this.useImperial() ? 'lbs' : 'kg');
  }

  convertWeight(kg: number): number {
    return this.useImperial() ? kg * 2.20462 : kg;
  }

  weightToKg(display: number): number {
    return this.useImperial() ? display / 2.20462 : display;
  }

  formatWeight(kg: number, decimals = 1): string {
    return this.convertWeight(kg).toFixed(decimals);
  }

  convertHeight(cm: number): number {
    return this.useImperial() ? cm / 2.54 : cm;
  }

  heightToCm(display: number): number {
    return this.useImperial() ? display * 2.54 : display;
  }

  formatHeight(cm: number): string {
    if (this.useImperial()) {
      const totalInches = cm / 2.54;
      const feet = Math.floor(totalInches / 12);
      const inches = Math.round(totalInches % 12);
      return `${feet}'${inches}"`;
    }
    return `${cm.toFixed(0)} cm`;
  }
}
