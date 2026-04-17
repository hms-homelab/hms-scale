import { Component } from '@angular/core';
import { RouterOutlet, RouterLink, RouterLinkActive } from '@angular/router';

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
    </nav>
    <main>
      <router-outlet />
    </main>
  `,
  styles: [`
    :host { display: block; min-height: 100vh; background: #121212; }
    main { max-width: 1200px; margin: 0 auto; }
  `]
})
export class AppComponent {}
