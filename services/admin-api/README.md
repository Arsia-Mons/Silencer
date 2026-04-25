# zSILENCER Admin API

Express.js REST + WebSocket API that powers the admin dashboard and player portal.

## Stack

| Layer     | Technology                        |
|-----------|-----------------------------------|
| Runtime   | Node.js 22 (ESM)                  |
| Framework | Express 4                         |
| Database  | MongoDB 7 via Mongoose 8          |
| Realtime  | Socket.IO 4                       |
| Queue     | RabbitMQ via amqplib              |
| Auth      | JWT (jsonwebtoken) + bcryptjs     |

## Running

```bash
# Development (auto-restart on change)
npm run dev

# Production (via Docker Compose from repo root)
PUBLIC_ADDR=<vm-ip> sudo -E docker compose up -d --build admin-api
```

Environment variables (set in `.env` beside `docker-compose.yml`):

| Variable              | Default                               | Description                        |
|-----------------------|---------------------------------------|------------------------------------|
| `PORT`                | `24080`                               | HTTP listen port                   |
| `MONGO_URL`           | `mongodb://mongo:27017/silencer`      | MongoDB connection string          |
| `RABBITMQ_URL`        | `amqp://zsilencer:zsilencer@...`      | RabbitMQ AMQP URL                  |
| `JWT_SECRET`          | `changeme-in-production`              | Secret for signing JWTs            |
| `LOBBY_PLAYER_AUTH_URL` | `http://lobby:15171`               | Lobby HTTP auth endpoint           |
| `BACKUP_DIR`          | `/backups`                            | Directory for mongodump archives   |
| `BACKUP_CRON`         | `0 */6 * * *`                        | Auto-backup schedule (cron)        |
| `BACKUP_KEEP`         | `10`                                  | Max local backup files to keep     |
| `GITHUB_TOKEN`        | *(unset)*                             | PAT with `repo` scope for backups  |
| `GITHUB_BACKUP_REPO`  | `Arsia-Mons/silencer-mongo-backup`   | GitHub repo to push backups to     |

## API Routes

All routes except `/health` and `/auth/login` require a valid JWT in `Authorization: Bearer <token>`.

### Auth — `/auth`

| Method | Path                        | Role     | Description                            |
|--------|-----------------------------|----------|----------------------------------------|
| POST   | `/auth/login`               | —        | Admin login → returns JWT              |
| POST   | `/auth/player-login`        | —        | Player login via lobby token           |
| GET    | `/auth/users`               | admin    | List admin users                       |
| POST   | `/auth/users`               | superadmin | Create admin user                    |
| PATCH  | `/auth/users/:id`           | superadmin | Update admin user role/username      |
| PATCH  | `/auth/users/:id/password`  | superadmin | Reset another user's password        |
| DELETE | `/auth/users/:id`           | superadmin | Delete admin user                    |
| PATCH  | `/auth/me/password`         | any admin | Change own password                  |

### Players — `/players`

| Method | Path                            | Role  | Description                            |
|--------|---------------------------------|-------|----------------------------------------|
| GET    | `/players`                      | admin | List players (search, page, limit)     |
| GET    | `/players/:accountId`           | admin      | Single player with all fields          |
| GET    | `/players/:accountId/matches`   | admin      | Paginated match history for a player   |
| PATCH  | `/players/:accountId/ban`       | admin      | Ban or unban a player; notifies the Go lobby in real time via internal HTTP so clients are blocked immediately |
| DELETE | `/players/:accountId`           | superadmin | Delete player from MongoDB and remove from lobby in-memory store via internal `POST /delete-player` |

### Sessions — `/sessions`

| Method | Path         | Role  | Description         |
|--------|--------------|-------|---------------------|
| GET    | `/sessions`  | admin | List game sessions  |

### Events — `/events`

| Method | Path       | Role  | Description            |
|--------|------------|-------|------------------------|
| GET    | `/events`  | admin | List lobby events      |

### Stats — `/stats`

| Method | Path     | Role  | Description                                |
|--------|----------|-------|--------------------------------------------|
| GET    | `/stats` | admin | Live counts: players, games, DB/queue status |

### Backup — `/backup`

| Method | Path                | Role  | Description                                         |
|--------|---------------------|-------|-----------------------------------------------------|
| POST   | `/backup/trigger`   | admin | Trigger an immediate mongodump (async, 202 response)|
| GET    | `/backup/status`    | admin | Current backup state + last result                  |
| GET    | `/backup/list`      | admin | List all local backup `.gz` files                   |

### Player self-service — `/me`

Authenticated via player JWT (issued by lobby player-auth, not admin JWT).

| Method | Path          | Description                        |
|--------|---------------|------------------------------------|
| GET    | `/me`         | Own player profile + lifetime stats |
| GET    | `/me/matches` | Own match history (paginated)       |

## WebSocket Events (Socket.IO)

Connect to `ws://<host>:24080`. Clients must send JWT in handshake auth: `{ token }`.

| Event (server → client) | Payload                        | Description                          |
|--------------------------|--------------------------------|--------------------------------------|
| `snapshot`               | Full live state                | Sent on connect + on `getSnapshot`   |
| `player_connected`       | `{ accountId, name }`          | Player joined lobby                  |
| `player_disconnected`    | `{ accountId }`                | Player left lobby                    |
| `game_started`           | `{ sessionId, players }`       | Game session started                 |
| `game_ended`             | `{ sessionId, results }`       | Game session ended                   |
| `stats_registered`       | Match stats summary            | Stats saved after a match            |

| Event (client → server) | Description                         |
|--------------------------|-------------------------------------|
| `getSnapshot`            | Request full current state resent   |

## Backup System

Backups run via `mongodump` inside the admin-api container (MongoDB tools installed in Dockerfile).

**Auto-backup**: scheduled by `node-cron` using `BACKUP_CRON`. Default: every 6 hours.

**Manual backup**: `POST /backup/trigger` returns `202 Accepted` immediately. Poll `GET /backup/status` to check completion. Returns `409` if a backup is already running.

**Format**: Single `.gz` archive per backup (`--archive --gzip`). Stored in `BACKUP_DIR` (Docker volume `backup-data`). Oldest files pruned when count exceeds `BACKUP_KEEP`.

**GitHub push**: If `GITHUB_TOKEN` and `GITHUB_BACKUP_REPO` are set, each backup is committed to `zsilencer.archive.gz` in that repo. Git history provides rollback — browse commits at `github.com/<repo>/commits`.

**Restore**:
```bash
# On the VM host (mongo exposed on 28017).
# NOTE: container/volume names like `zsilencer-mongo-1` /
# `zsilencer_*` come from the docker-compose project name (the
# repo dir is still `zSilencer/`); they're decoupled from the
# `silencer` Mongo DB name and are renamed in a later phase.
docker exec -i zsilencer-mongo-1 mongorestore \
  --uri=mongodb://localhost:27017/silencer \
  --archive --gzip < /path/to/backup.gz

# Or from inside the backup volume
docker run --rm \
  -v zsilencer_backup-data:/backups \
  -v zsilencer_mongo-data:/data/db \
  mongo:7 mongorestore \
  --uri=mongodb://mongo:27017/silencer \
  --archive=/backups/zsilencer-<timestamp>.archive.gz --gzip
```

## Database Models

### Player
Persisted after first login. Key fields:
- `accountId` (Number) — from lobby, primary key
- `name` — callsign
- `agencies[]` — one entry per agency: `level`, `wins`, `losses`, `xpToNextLevel`, stat bonuses
- `lifetimeStats` — cumulative totals `$inc`'d after every match (37 fields mirroring `MatchStat`)
- `banned`, `banReason`
- `loginCount`, `firstSeen`, `lastSeen`
- `ipHistory[]` — `{ ip, firstSeen, lastSeen, count }`

### MatchStat
One document per match per player. Key fields:
- `accountId`, `sessionId`, `team`, `win`
- All 37 stat fields (kills, deaths, weapon fires/hits/kills × 4, objectives, economy)

### Session
One document per game session.

### Event
Lobby events (player join/leave, game start/end).

### AdminUser
- `username`, `passHash` (bcrypt), `role` (`admin` | `superadmin`), `createdBy`
- Default seed: `admin` / `admin` — **change on first deploy**

## Stat Fields (37 total, 148-byte blob)

| Field               | Description                         |
|---------------------|-------------------------------------|
| blasterFires/Hits/Kills | Blaster weapon stats            |
| laserFires/Hits/Kills   | Laser weapon stats              |
| rocketFires/Hits/Kills  | Rocket weapon stats             |
| flamerFires/Hits/Kills  | Flamer weapon stats             |
| civiliansKilled     | Civilian NPC kills                  |
| guardsKilled        | Guard NPC kills                     |
| robotsKilled        | Robot NPC kills                     |
| defenseKilled       | Defense unit kills                  |
| secretsPickedUp/Returned/Stolen/Dropped | Intel objective stats |
| powerupsPickedUp    | Powerup pickups                     |
| deaths / kills / suicides / poisons | Combat totals           |
| tractsPlanted       | Tracts planted                      |
| grenadesThrown / neutronsThrown / empsThrown / shapedThrown / plasmasThrown / flaresThrown / poisonFlaresThrown | Throwable usage |
| healthPacksUsed     | Health pack uses                    |
| filesReturned       | Intel files returned (objective)    |
| creditsmade         | Credits earned at credit machines   |
| creditsspent        | Credits spent on items/repairs      |
| healsdone           | Heals performed at heal machines    |
