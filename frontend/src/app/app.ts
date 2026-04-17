import { Component, inject } from '@angular/core';
import { RouterOutlet, RouterLink, RouterLinkActive } from '@angular/router';
import { UnitService } from './services/unit.service';

@Component({
  selector: 'app-root',
  standalone: true,
  imports: [RouterOutlet, RouterLink, RouterLinkActive],
  template: `
    <nav class="nav-bar">
      <span class="nav-title">HMS Colada</span>
      <a routerLink="/dashboard" routerLinkActive="active">Dashboard</a>
      <a routerLink="/users" routerLinkActive="active">Users</a>
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
export class AppComponent {
  units = inject(UnitService);
}
