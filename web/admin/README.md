# Silencer Admin Web

Next.js 14 admin dashboard and player self-service portal for Silencer.

## Stack

| Layer     | Technology              |
|-----------|-------------------------|
| Framework | Next.js 14 (App Router) |
| Styling   | Tailwind CSS            |
| Realtime  | Socket.IO client        |
| Auth      | JWT in localStorage     |

## Running

```bash
# Development
npm run dev   # starts on :24000

# Production (via Docker Compose from repo root)
PUBLIC_ADDR=<vm-ip> sudo -E docker compose up -d --build admin-web
```

Build-time environment variables (passed as Docker build args):

| Variable                | Default                       | Description                        |
|-------------------------|-------------------------------|------------------------------------|
| `NEXT_PUBLIC_API_URL`   | `http://localhost:24080`      | Admin API base URL                 |
| `NEXT_PUBLIC_WS_URL`    | `ws://localhost:24080`        | WebSocket URL                      |
| `PORT`                  | `24000`                       | Next.js listen port                |

## Pages

### Admin Dashboard (requires admin login)

| Route                       | Description                                                   |
|-----------------------------|---------------------------------------------------------------|
| `/login`                    | Admin login form. Also links to player portal login.          |
| `/`                         | Live dashboard: online players, active games, recent events, live game list. WebSocket-powered, restores state on navigation. |
| `/players`                  | Searchable, paginated player list. Click a row to drill in.   |
| `/players/[accountId]`      | Full player profile: identity, combat stats, economy, agency breakdown, weapon accuracy, paginated match history. Ban/unban button. Delete button (superadmin only, with confirm dialog). Ban and delete calls are proxied to the Go lobby for real-time enforcement — clients are blocked immediately without a server restart. |
| `/sessions`                 | Game session history.                                         |
| `/events`                   | Lobby event log.                                              |
| `/changelog`                | Version history of admin panel features.                      |
| `/health`                   | Service status (MongoDB, RabbitMQ, WebSocket). MongoDB backup panel with auto-backup schedule, manual trigger button, backup history table, and GitHub link. |
| `/users`                    | Admin user management: create, edit role, reset password, delete. Superadmin only. |

### Player Portal (requires player login via lobby credentials)

| Route   | Description                                                             |
|---------|-------------------------------------------------------------------------|
| `/login?mode=player` | Player login form (uses lobby token exchange).          |
| `/me`   | Player self-service profile: agency selector (NOXIS / LAZARUS / CALIBER / STATIC / BLACKROSE), lifetime combat stats, weapon accuracy table, objectives/tech/hacking breakdown, economy stats, paginated match history. |

## Key Libraries

### `lib/auth.js`
- `useAuth()` — redirects to `/login` if no admin JWT in localStorage
- `usePlayerAuth()` — redirects to `/login?mode=player` if no player JWT
- `login(username, password)` / `logout()`

### `lib/api.js`
Thin fetch wrapper that injects the JWT from localStorage.

Key functions:

| Function                          | Description                              |
|-----------------------------------|------------------------------------------|
| `getPlayers(params)`              | List players (search, page, limit)       |
| `getPlayer(id)`                   | Single player                            |
| `getPlayerMatches(id, page)`      | Player match history (admin view)        |
| `banPlayer(id, banned, reason)`   | Ban/unban                                |
| `getSessions(params)`             | Game sessions                            |
| `getEvents(params)`               | Lobby events                             |
| `getStats()`                      | Health/stats snapshot                    |
| `getAdminUsers()`                 | List admin users                         |
| `createAdminUser(data)`           | Create admin user                        |
| `updateAdminUser(id, data)`       | Update role/username                     |
| `resetAdminPassword(id, pw)`      | Reset another user's password            |
| `deleteAdminUser(id)`             | Delete admin user                        |
| `changeOwnPassword(cur, new)`     | Change own password                      |
| `getMyProfile()`                  | Player portal: own profile               |
| `getMyMatches(page)`              | Player portal: own match history         |
| `triggerBackup()`                 | Trigger manual MongoDB backup            |
| `getBackupStatus()`               | Backup in-progress state + last result   |
| `listBackups()`                   | List local backup files                  |

### `lib/socket.js`
Socket.IO singleton. Connects once on first use, stays connected across page navigations.

```js
const wsConnected = useSocket({
  onPlayerConnected: (data) => { ... },
  onGameStarted:     (data) => { ... },
  // etc.
});
```

Emits `getSnapshot` on mount so the dashboard always receives current state even if the socket was already connected from a previous page.

### `lib/changelog.js`
Structured changelog data consumed by `/changelog` page. Add new entries here when shipping features.

```js
{ version: '1.8.0', date: '2026-04-24', changes: ['...'] }
```

## Components

| Component        | Description                                              |
|------------------|----------------------------------------------------------|
| `Sidebar`        | Navigation sidebar with logo, page links, WS status dot  |
| `StatCard`       | Single metric card (label + value + color variant)       |

## Tailwind Theme

Custom game-themed color palette defined in `tailwind.config.js`:

| Token              | Usage                            |
|--------------------|----------------------------------|
| `game-primary`     | Accent green — active states     |
| `game-danger`      | Red — bans, errors               |
| `game-warning`     | Yellow — warnings                |
| `game-info`        | Blue — informational             |
| `game-text`        | Primary text                     |
| `game-textDim`     | Secondary text / labels          |
| `game-muted`       | Muted / placeholder text         |
| `game-bgCard`      | Card background                  |
| `game-bgHover`     | Row hover background             |
| `game-border`      | Card / table borders             |
| `game-dark`        | Dark background variant          |

## Authentication Flow

### Admin
1. `POST /auth/login` → receives JWT
2. JWT stored in `localStorage` as `zs_token`
3. `useAuth()` hook checks token on every protected page, redirects to `/login` if missing
4. All API calls inject `Authorization: Bearer <token>` header

### Player
1. Player logs in with their in-game callsign + password via `POST /auth/player-login`
2. Lobby validates credentials, returns a player JWT
3. Token stored as `zs_player_token`
4. `usePlayerAuth()` guards portal pages
5. Player can only see their own data (`/me`)

## Backup Panel (`/health`)

- **Auto-backup indicator**: shows schedule and whether GitHub upload is configured
- **BACKUP NOW button**: calls `POST /backup/trigger`, polls `GET /backup/status` every 2s while in-progress
- **Last result**: shows filename, size, timestamp, and a GitHub commit link if upload succeeded
- **Backup table**: lists all local `.gz` archives with size and date
- **Restore command**: shown inline for reference

## Player Detail (`/players/[accountId]`)

Full admin view of any player. Sections:
1. **Identity** — account ID, login count, first/last seen, ban reason
2. **Lifetime Combat** — kills, deaths, K/D, suicides, poisons, NPC kills
3. **Economy** — credits earned/spent, heals, health packs, powerups
4. **Agencies** — level, W/L, XP for all 5 agencies (NOXIS/LAZARUS/CALIBER/STATIC/BLACKROSE)
5. **Weapon Accuracy** — fires, hits, kills, accuracy % per weapon
6. **Match History** — paginated table with date, agency, K/D, win/loss

## Docker Notes

The Next.js app runs in standalone mode (`output: 'standalone'` in `next.config.mjs`). The `public/` directory must be explicitly copied in the Dockerfile runner stage — it is not bundled automatically.

```dockerfile
COPY --from=builder /app/public ./public
COPY --from=builder /app/.next/standalone ./
COPY --from=builder /app/.next/static ./.next/static
```
