import { Injectable, signal, computed } from '@angular/core';

@Injectable({ providedIn: 'root' })
export class UnitService {
  private useLbs = signal(true);

  isLbs = computed(() => this.useLbs());
  weightUnit = computed(() => this.useLbs() ? 'lbs' : 'kg');

  constructor() {
    const saved = localStorage.getItem('weight_unit');
    if (saved === 'kg') this.useLbs.set(false);
  }

  toggle(): void {
    this.useLbs.update(v => !v);
    localStorage.setItem('weight_unit', this.useLbs() ? 'lbs' : 'kg');
  }

  convertWeight(kg: number): number {
    return this.useLbs() ? kg * 2.20462 : kg;
  }

  formatWeight(kg: number, decimals = 1): string {
    const val = this.convertWeight(kg);
    return val.toFixed(decimals);
  }
}
