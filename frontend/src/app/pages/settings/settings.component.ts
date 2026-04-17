import { Component, OnInit, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatInputModule } from '@angular/material/input';
import { MatSelectModule } from '@angular/material/select';
import { MatSlideToggleModule } from '@angular/material/slide-toggle';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatSnackBar, MatSnackBarModule } from '@angular/material/snack-bar';
import { MatDividerModule } from '@angular/material/divider';
import { HttpClient } from '@angular/common/http';

interface AppConfig {
  web_port: number;
  static_dir: string;
  setup_complete: boolean;
  database: {
    host: string;
    port: number;
    name: string;
    user: string;
    password: string;
  };
  mqtt: {
    enabled: boolean;
    broker: string;
    port: number;
    username: string;
    password: string;
    client_id: string;
    scale_topic: string;
    user_selector_topic: string;
  };
  ml_training: {
    enabled: boolean;
    schedule: string;
    model_dir: string;
    min_measurements: number;
  };
}

@Component({
  selector: 'app-settings',
  standalone: true,
  imports: [
    CommonModule,
    FormsModule,
    MatFormFieldModule,
    MatInputModule,
    MatSelectModule,
    MatSlideToggleModule,
    MatButtonModule,
    MatIconModule,
    MatSnackBarModule,
    MatDividerModule,
  ],
  template: `
    <div class="page-container">
      <div class="page-header">
        <h1>Settings</h1>
        <button mat-raised-button color="primary" (click)="save()" [disabled]="saving">
          <mat-icon>save</mat-icon>
          {{ saving ? 'Saving...' : 'Save Config' }}
        </button>
      </div>

      @if (config) {
        <!-- Database -->
        <section class="section">
          <h2>Database (PostgreSQL)</h2>
          <div class="form-grid">
            <mat-form-field appearance="outline">
              <mat-label>Host</mat-label>
              <input matInput [(ngModel)]="config.database.host">
            </mat-form-field>
            <mat-form-field appearance="outline">
              <mat-label>Port</mat-label>
              <input matInput type="number" [(ngModel)]="config.database.port">
            </mat-form-field>
            <mat-form-field appearance="outline">
              <mat-label>Database Name</mat-label>
              <input matInput [(ngModel)]="config.database.name">
            </mat-form-field>
            <mat-form-field appearance="outline">
              <mat-label>User</mat-label>
              <input matInput [(ngModel)]="config.database.user">
            </mat-form-field>
            <mat-form-field appearance="outline">
              <mat-label>Password</mat-label>
              <input matInput type="password" [(ngModel)]="config.database.password" placeholder="(unchanged)">
            </mat-form-field>
          </div>
        </section>

        <mat-divider />

        <!-- MQTT -->
        <section class="section">
          <div class="section-header">
            <h2>MQTT</h2>
            <mat-slide-toggle [(ngModel)]="config.mqtt.enabled" color="primary">
              {{ config.mqtt.enabled ? 'Enabled' : 'Disabled' }}
            </mat-slide-toggle>
          </div>
          @if (config.mqtt.enabled) {
            <div class="form-grid">
              <mat-form-field appearance="outline">
                <mat-label>Broker</mat-label>
                <input matInput [(ngModel)]="config.mqtt.broker">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Port</mat-label>
                <input matInput type="number" [(ngModel)]="config.mqtt.port">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Username</mat-label>
                <input matInput [(ngModel)]="config.mqtt.username">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Password</mat-label>
                <input matInput type="password" [(ngModel)]="config.mqtt.password" placeholder="(unchanged)">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Client ID</mat-label>
                <input matInput [(ngModel)]="config.mqtt.client_id">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Scale Topic</mat-label>
                <input matInput [(ngModel)]="config.mqtt.scale_topic">
              </mat-form-field>
            </div>
          }
        </section>

        <mat-divider />

        <!-- ML Training -->
        <section class="section">
          <div class="section-header">
            <h2>ML Training</h2>
            <mat-slide-toggle [(ngModel)]="config.ml_training.enabled" color="primary">
              {{ config.ml_training.enabled ? 'Enabled' : 'Disabled' }}
            </mat-slide-toggle>
          </div>
          @if (config.ml_training.enabled) {
            <div class="form-grid">
              <mat-form-field appearance="outline">
                <mat-label>Schedule</mat-label>
                <mat-select [(ngModel)]="config.ml_training.schedule">
                  <mat-option value="daily">Daily</mat-option>
                  <mat-option value="weekly">Weekly</mat-option>
                  <mat-option value="monthly">Monthly</mat-option>
                </mat-select>
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Min Measurements per User</mat-label>
                <input matInput type="number" [(ngModel)]="config.ml_training.min_measurements">
              </mat-form-field>
              <mat-form-field appearance="outline">
                <mat-label>Model Directory</mat-label>
                <input matInput [(ngModel)]="config.ml_training.model_dir" placeholder="(default: ~/.hms-colada/models)">
              </mat-form-field>
            </div>
          }
        </section>

        <mat-divider />

        <!-- Web Server -->
        <section class="section">
          <h2>Web Server</h2>
          <div class="form-grid">
            <mat-form-field appearance="outline">
              <mat-label>Port</mat-label>
              <input matInput type="number" [(ngModel)]="config.web_port">
            </mat-form-field>
            <mat-form-field appearance="outline">
              <mat-label>Static Files Directory</mat-label>
              <input matInput [(ngModel)]="config.static_dir">
            </mat-form-field>
          </div>
        </section>

        @if (configPath) {
          <div class="config-path">
            Config file: {{ configPath }}
          </div>
        }
      } @else {
        <div class="empty-state">Loading configuration...</div>
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
    .section {
      padding: 20px 0;
      h2 { font-size: 18px; font-weight: 400; color: #ccc; margin-bottom: 16px; }
    }
    .section-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
      h2 { margin-bottom: 0; }
    }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
      gap: 8px 16px;
    }
    .config-path {
      margin-top: 24px;
      padding: 12px 16px;
      background: #1a1a2e;
      border-radius: 6px;
      color: #666;
      font-size: 12px;
      font-family: monospace;
    }
    .empty-state {
      text-align: center;
      padding: 48px;
      color: #888;
    }
    mat-divider {
      border-color: #333;
    }
  `]
})
export class SettingsComponent implements OnInit {
  private http = inject(HttpClient);
  private snackBar = inject(MatSnackBar);

  config: AppConfig | null = null;
  configPath = '';
  saving = false;

  ngOnInit(): void {
    this.http.get<any>('/api/config').subscribe({
      next: (data) => {
        this.configPath = data.config_path ?? '';
        this.config = {
          web_port: data.web_port ?? 8889,
          static_dir: data.static_dir ?? './static/browser',
          setup_complete: data.setup_complete ?? false,
          database: {
            host: data.database?.host ?? 'localhost',
            port: data.database?.port ?? 5432,
            name: data.database?.name ?? 'colada_scale',
            user: data.database?.user ?? 'colada_user',
            password: '',
          },
          mqtt: {
            enabled: data.mqtt?.enabled ?? true,
            broker: data.mqtt?.broker ?? 'localhost',
            port: data.mqtt?.port ?? 1883,
            username: data.mqtt?.username ?? '',
            password: '',
            client_id: data.mqtt?.client_id ?? 'hms_colada',
            scale_topic: data.mqtt?.scale_topic ?? 'giraffe_scale/measurement',
            user_selector_topic: data.mqtt?.user_selector_topic ?? 'colada_scale/user_selector/set',
          },
          ml_training: {
            enabled: data.ml_training?.enabled ?? false,
            schedule: data.ml_training?.schedule ?? 'weekly',
            model_dir: data.ml_training?.model_dir ?? '',
            min_measurements: data.ml_training?.min_measurements ?? 20,
          },
        };
      },
      error: () => {
        this.snackBar.open('Failed to load config', 'OK', { duration: 3000 });
      }
    });
  }

  save(): void {
    if (!this.config) return;
    this.saving = true;

    const payload: any = {
      web_port: this.config.web_port,
      static_dir: this.config.static_dir,
      database: { ...this.config.database },
      mqtt: { ...this.config.mqtt },
      ml_training: { ...this.config.ml_training },
    };

    if (!payload.database.password) delete payload.database.password;
    if (!payload.mqtt.password) delete payload.mqtt.password;

    this.http.put<any>('/api/config', payload).subscribe({
      next: (resp) => {
        this.saving = false;
        const msg = resp.restart_required
          ? 'Config saved. Restart service for changes to take effect.'
          : 'Config saved.';
        this.snackBar.open(msg, 'OK', { duration: 5000 });
      },
      error: (err) => {
        this.saving = false;
        this.snackBar.open('Failed to save: ' + (err.error?.error ?? 'Unknown error'), 'OK', { duration: 5000 });
      }
    });
  }
}
