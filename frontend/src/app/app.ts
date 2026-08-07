import { Component, OnInit, inject } from '@angular/core';
import { RouterOutlet, RouterLink, RouterLinkActive } from '@angular/router';
import { UnitService } from './services/unit.service';
import { ColadaApiService } from './services/colada-api.service';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, RouterLink, RouterLinkActive],
  template: `
    <nav class="nav-bar">
      <span class="nav-title">HMS Colada</span>
      <a routerLink="/dashboard" routerLinkActive="active">Dashboard</a>
      <a routerLink="/users" routerLinkActive="active">Users</a>
      <a routerLink="/unassigned" routerLinkActive="active">
        Unassigned
        @if (api.unassignedCount() > 0) {
          <span class="badge">{{ api.unassignedCount() }}</span>
        }
      </a>
      <a routerLink="/ml" routerLinkActive="active">ML</a>
      <a routerLink="/habits" routerLinkActive="active">Habits</a>
      <a routerLink="/settings" routerLinkActive="active">Settings</a>
      <button class="unit-toggle" (click)="units.toggle()">
        {{ units.weightUnit() }}
      </button>
    </nav>
    <main>
      <router-outlet />
    </main>
  `,
  styles: [`
    :host { display: block; min-height: 100vh; background: #121212; }
    main { max-width: 1200px; margin: 0 auto; }
    .badge {
      display: inline-block;
      min-width: 16px;
      margin-left: 6px;
      padding: 1px 5px;
      border-radius: 8px;
      background: #ffa726;
      color: #121212;
      font-size: 11px;
      font-weight: 600;
      line-height: 14px;
      text-align: center;
    }
    .unit-toggle {
      margin-left: auto;
      padding: 4px 12px;
      border: 1px solid #555;
      border-radius: 4px;
      background: transparent;
      color: #ccc;
      font-size: 12px;
      cursor: pointer;
      text-transform: uppercase;
      letter-spacing: 1px;
      &:hover { background: rgba(255,255,255,0.1); }
    }
  `]
})
export class AppComponent implements OnInit {
  units = inject(UnitService);
  api = inject(ColadaApiService);

  ngOnInit(): void {
    this.api.refreshUnassignedCount();
  }
}
