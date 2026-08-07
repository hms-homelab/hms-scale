import { Component, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatTableModule } from '@angular/material/table';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatSelectModule } from '@angular/material/select';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatSnackBar, MatSnackBarModule } from '@angular/material/snack-bar';
import { ColadaApiService } from '../../services/colada-api.service';
import { UnitService } from '../../services/unit.service';
import { ScaleMeasurement, ScaleUser } from '../../models/user.model';

/**
 * Measurements the identification engine could not attribute to anyone, with a
 * picker to claim each one. The backend has always had the endpoints for this
 * (GET /api/measurements/unassigned, POST /api/measurements/{id}/assign) but
 * nothing in the UI called them.
 */
@Component({
  selector: 'app-unassigned',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatTableModule,
    MatButtonModule,
    MatIconModule,
    MatSelectModule,
    MatFormFieldModule,
    MatSnackBarModule,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>Unassigned</h1>
        @if (measurements.length) {
          <span class="count">{{ measurements.length }} pending</span>
        }
      </div>

      @if (measurements.length) {
      <div class="card">
        <table mat-table [dataSource]="measurements" class="unassigned-table">
          <ng-container matColumnDef="measured_at">
            <th mat-header-cell *matHeaderCellDef>Measured</th>
            <td mat-cell *matCellDef="let m">{{ formatWhen(m.measured_at) }}</td>
          </ng-container>

          <ng-container matColumnDef="weight">
            <th mat-header-cell *matHeaderCellDef>Weight</th>
            <td mat-cell *matCellDef="let m" class="weight">
              {{ units.formatWeight(m.weight_kg) }} {{ units.weightUnit() }}
            </td>
          </ng-container>

          <ng-container matColumnDef="impedance_ohm">
            <th mat-header-cell *matHeaderCellDef>Impedance</th>
            <td mat-cell *matCellDef="let m">
              {{ m.impedance_ohm ? (m.impedance_ohm | number:'1.0-0') + ' Ω' : '—' }}
            </td>
          </ng-container>

          <ng-container matColumnDef="actions">
            <th mat-header-cell *matHeaderCellDef>Assign to</th>
            <td mat-cell *matCellDef="let m">
              <div class="assign">
                <mat-form-field appearance="outline" subscriptSizing="dynamic">
                  <mat-select [(ngModel)]="selection[m.id]" placeholder="Select user">
                    @for (u of users; track u.id) {
                      <mat-option [value]="u.id">{{ u.name }}</mat-option>
                    }
                  </mat-select>
                </mat-form-field>
                <button
                  mat-raised-button
                  color="primary"
                  [disabled]="!selection[m.id] || pending[m.id]"
                  (click)="assign(m)">
                  <mat-icon>how_to_reg</mat-icon> Assign
                </button>
              </div>
            </td>
          </ng-container>

          <tr mat-header-row *matHeaderRowDef="displayedColumns"></tr>
          <tr mat-row *matRowDef="let row; columns: displayedColumns;"></tr>
        </table>
      </div>
      } @else {
        <div class="card empty-state">
          <mat-icon>check_circle</mat-icon>
          <p>Nothing waiting — every measurement has been matched to a user.</p>
        </div>
      }
    </div>
  `,
  styles: [`
    .page-header {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 24px;
      h1 { font-size: 24px; font-weight: 400; color: #fff; }
    }
    .count {
      padding: 2px 10px;
      border-radius: 12px;
      background: rgba(255,167,38,0.15);
      color: #ffa726;
      font-size: 12px;
      letter-spacing: 0.5px;
    }
    .unassigned-table { width: 100%; }
    .card { overflow-x: auto; }
    .weight { font-variant-numeric: tabular-nums; }
    .assign {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 8px 0;
      mat-form-field { width: 160px; }
    }
    .empty-state {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 8px;
      padding: 48px 24px;
      color: #888;
      mat-icon { font-size: 40px; width: 40px; height: 40px; color: #4caf50; }
      p { margin: 0; }
    }
  `]
})
export class UnassignedComponent implements OnInit {
  private api = inject(ColadaApiService);
  private snackBar = inject(MatSnackBar);
  units = inject(UnitService);

  measurements: ScaleMeasurement[] = [];
  users: ScaleUser[] = [];
  /** measurement id -> chosen user id */
  selection: Record<string, string> = {};
  /** measurement id -> request in flight, so Assign cannot be double-fired */
  pending: Record<string, boolean> = {};
  displayedColumns = ['measured_at', 'weight', 'impedance_ohm', 'actions'];

  ngOnInit(): void {
    this.api.getUsers().subscribe(users => this.users = users.filter(u => u.is_active));
    this.load();
  }

  load(): void {
    this.api.getUnassigned().subscribe(m => this.measurements = m);
  }

  assign(m: ScaleMeasurement): void {
    const userId = this.selection[m.id];
    if (!userId || this.pending[m.id]) return;

    this.pending[m.id] = true;
    this.api.assignMeasurement(m.id, userId).subscribe({
      next: () => {
        const name = this.users.find(u => u.id === userId)?.name ?? 'user';
        this.snackBar.open(
          `${this.units.formatWeight(m.weight_kg)} ${this.units.weightUnit()} assigned to ${name}`,
          'Dismiss',
          { duration: 4000 });
        delete this.pending[m.id];
        delete this.selection[m.id];
        // Reload rather than splice locally: assigning can change what the
        // server considers unassigned beyond this one row.
        this.load();
      },
      error: err => {
        delete this.pending[m.id];
        this.snackBar.open(`Could not assign: ${err?.error?.error ?? err.message}`, 'Dismiss',
          { duration: 6000 });
      },
    });
  }

  /**
   * PostgreSQL hands back "2026-08-07 10:27:54-04" — a space instead of the T,
   * and a bare two-digit offset. Safari rejects both, so normalise before
   * parsing rather than rendering "Invalid Date".
   */
  formatWhen(ts: string): string {
    if (!ts) return '—';
    let iso = ts.replace(' ', 'T');
    iso = iso.replace(/([+-]\d{2})$/, '$1:00');
    const d = new Date(iso);
    if (isNaN(d.getTime())) return ts;
    return d.toLocaleString(undefined, {
      month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit',
    });
  }
}
