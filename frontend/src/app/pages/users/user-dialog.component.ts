import { Component, Inject, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatDialogModule, MatDialogRef, MAT_DIALOG_DATA } from '@angular/material/dialog';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatSelectModule } from '@angular/material/select';
import { MatButtonModule } from '@angular/material/button';
import { MatSlideToggleModule } from '@angular/material/slide-toggle';
import { ScaleUser } from '../../models/user.model';
import { UnitService } from '../../services/unit.service';

@Component({
  selector: 'app-user-dialog',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatDialogModule,
    MatFormFieldModule,
    MatInputModule,
    MatSelectModule,
    MatButtonModule,
    MatSlideToggleModule,
  ],
  template: `
    <h2 mat-dialog-title>{{ data ? 'Edit User' : 'Add User' }}</h2>
    <mat-dialog-content>
      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Name</mat-label>
        <input matInput [(ngModel)]="form.name" required>
      </mat-form-field>

      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Date of Birth</mat-label>
        <input matInput type="date" [(ngModel)]="form.date_of_birth" required>
      </mat-form-field>

      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Sex</mat-label>
        <mat-select [(value)]="form.sex" required>
          <mat-option value="male">Male</mat-option>
          <mat-option value="female">Female</mat-option>
        </mat-select>
      </mat-form-field>

      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Height ({{ units.isLbs() ? 'inches' : 'cm' }})</mat-label>
        <input matInput type="number" [(ngModel)]="displayHeight" required>
      </mat-form-field>

      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Expected Weight ({{ units.weightUnit() }})</mat-label>
        <input matInput type="number" [(ngModel)]="displayWeight" required>
      </mat-form-field>

      <mat-form-field appearance="outline" class="full-width">
        <mat-label>Tolerance ({{ units.weightUnit() }})</mat-label>
        <input matInput type="number" [(ngModel)]="displayTolerance" required>
      </mat-form-field>

      @if (data) {
        <mat-slide-toggle [(ngModel)]="form.is_active">Active</mat-slide-toggle>
      }
    </mat-dialog-content>
    <mat-dialog-actions align="end">
      <button mat-button mat-dialog-close>Cancel</button>
      <button mat-raised-button color="primary" (click)="save()" [disabled]="!isValid()">
        {{ data ? 'Update' : 'Create' }}
      </button>
    </mat-dialog-actions>
  `,
  styles: [`
    .full-width { width: 100%; margin-bottom: 8px; }
    mat-dialog-content { display: flex; flex-direction: column; min-width: 360px; }
  `]
})
export class UserDialogComponent {
  units = inject(UnitService);
  form: Partial<ScaleUser>;
  displayHeight = 0;
  displayWeight = 0;
  displayTolerance = 0;

  constructor(
    public dialogRef: MatDialogRef<UserDialogComponent>,
    @Inject(MAT_DIALOG_DATA) public data: ScaleUser | null,
  ) {
    this.form = data ? { ...data } : {
      name: '',
      date_of_birth: '',
      sex: 'male',
      height_cm: 170,
      expected_weight_kg: 70,
      weight_tolerance_kg: 5,
      is_active: true,
    };
    this.displayHeight = +this.units.convertHeight(this.form.height_cm ?? 170).toFixed(1);
    this.displayWeight = +this.units.convertWeight(this.form.expected_weight_kg ?? 70).toFixed(1);
    this.displayTolerance = +this.units.convertWeight(this.form.weight_tolerance_kg ?? 5).toFixed(1);
  }

  isValid(): boolean {
    return !!(this.form.name && this.form.date_of_birth && this.form.sex
      && this.form.height_cm && this.displayWeight > 0
      && this.displayTolerance != null);
  }

  save(): void {
    if (!this.isValid()) return;
    this.form.height_cm = this.units.heightToCm(this.displayHeight);
    this.form.expected_weight_kg = this.units.weightToKg(this.displayWeight);
    this.form.weight_tolerance_kg = this.units.weightToKg(this.displayTolerance);
    this.dialogRef.close(this.form);
  }
}
