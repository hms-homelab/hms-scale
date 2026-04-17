import { Component, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatTableModule } from '@angular/material/table';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatDialog, MatDialogModule } from '@angular/material/dialog';
import { MatChipsModule } from '@angular/material/chips';
import { ColadaApiService } from '../../services/colada-api.service';
import { UnitService } from '../../services/unit.service';
import { ScaleUser } from '../../models/user.model';
import { UserDialogComponent } from './user-dialog.component';

@Component({
  selector: 'app-users',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatTableModule,
    MatButtonModule,
    MatIconModule,
    MatDialogModule,
    MatChipsModule,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>Users</h1>
        <button mat-raised-button color="primary" (click)="openDialog()">
          <mat-icon>add</mat-icon> Add User
        </button>
      </div>

      <div class="card">
        <table mat-table [dataSource]="users" class="users-table">
          <ng-container matColumnDef="name">
            <th mat-header-cell *matHeaderCellDef>Name</th>
            <td mat-cell *matCellDef="let user">{{ user.name }}</td>
          </ng-container>

          <ng-container matColumnDef="sex">
            <th mat-header-cell *matHeaderCellDef>Sex</th>
            <td mat-cell *matCellDef="let user">{{ user.sex }}</td>
          </ng-container>

          <ng-container matColumnDef="height_cm">
            <th mat-header-cell *matHeaderCellDef>Height</th>
            <td mat-cell *matCellDef="let user">{{ units.formatHeight(user.height_cm) }}</td>
          </ng-container>

          <ng-container matColumnDef="expected_weight_kg">
            <th mat-header-cell *matHeaderCellDef>Expected Weight</th>
            <td mat-cell *matCellDef="let user">{{ units.formatWeight(user.expected_weight_kg) }} {{ units.weightUnit() }}</td>
          </ng-container>

          <ng-container matColumnDef="weight_tolerance_kg">
            <th mat-header-cell *matHeaderCellDef>Tolerance</th>
            <td mat-cell *matCellDef="let user">&plusmn;{{ units.formatWeight(user.weight_tolerance_kg) }} {{ units.weightUnit() }}</td>
          </ng-container>

          <ng-container matColumnDef="is_active">
            <th mat-header-cell *matHeaderCellDef>Status</th>
            <td mat-cell *matCellDef="let user">
              <mat-chip [highlighted]="user.is_active" [color]="user.is_active ? 'primary' : 'warn'">
                {{ user.is_active ? 'Active' : 'Inactive' }}
              </mat-chip>
            </td>
          </ng-container>

          <ng-container matColumnDef="actions">
            <th mat-header-cell *matHeaderCellDef>Actions</th>
            <td mat-cell *matCellDef="let user">
              <button mat-icon-button (click)="openDialog(user)" matTooltip="Edit">
                <mat-icon>edit</mat-icon>
              </button>
              <button mat-icon-button color="warn" (click)="deleteUser(user)" matTooltip="Delete">
                <mat-icon>delete</mat-icon>
              </button>
            </td>
          </ng-container>

          <tr mat-header-row *matHeaderRowDef="displayedColumns"></tr>
          <tr mat-row *matRowDef="let row; columns: displayedColumns;"></tr>
        </table>
      </div>
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
    .users-table { width: 100%; }
    .card { overflow-x: auto; }
  `]
})
export class UsersComponent implements OnInit {
  private api = inject(ColadaApiService);
  private dialog = inject(MatDialog);
  units = inject(UnitService);

  users: ScaleUser[] = [];
  displayedColumns = ['name', 'sex', 'height_cm', 'expected_weight_kg', 'weight_tolerance_kg', 'is_active', 'actions'];

  ngOnInit(): void {
    this.loadUsers();
  }

  loadUsers(): void {
    this.api.getUsers().subscribe(users => this.users = users);
  }

  openDialog(user?: ScaleUser): void {
    const dialogRef = this.dialog.open(UserDialogComponent, {
      width: '480px',
      data: user ? { ...user } : null,
    });

    dialogRef.afterClosed().subscribe(result => {
      if (!result) return;
      const obs = user
        ? this.api.updateUser(user.id, result)
        : this.api.createUser(result);
      obs.subscribe(() => this.loadUsers());
    });
  }

  deleteUser(user: ScaleUser): void {
    if (confirm(`Delete user "${user.name}"?`)) {
      this.api.deleteUser(user.id).subscribe(() => this.loadUsers());
    }
  }
}
