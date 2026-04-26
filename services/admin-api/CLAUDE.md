# services/admin-api/ — Express admin REST + WebSocket API

Bun (1.x) + Express 4 + Socket.IO 4 + Mongoose 8 + amqplib. Source
is `.js` (ESM) — Bun runs the existing Node-style code unchanged.
Build/run, env vars, and the full route table are in `README.md`;
this file is for editing the code.

The runtime + lockfile (`bun.lock`) moved to Bun in Phase 1 of the
production deployment plan. The source-level migration to Bun + TS +
Hono + native WebSocket + Drizzle is Phase 2 — defer wholesale
rewrites until then.

## Routes mount under `/api`

`src/index.js` mounts every router under `/api/*` (e.g.
`/api/auth/login`, `/api/players`, `/api/health`). This lets a single
Cloudflare Tunnel hostname (`admin.arsiamons.com`) serve both
admin-web and admin-api without path collisions — admin-web has
page routes at `/players`, `/me`, `/health`, `/gamestats` that would
otherwise shadow the API. The `/socket.io/*` upgrade is a peer
endpoint Socket.IO mounts directly on the HTTP server (not under
`/api`); the tunnel routes both `/api/*` and `/socket.io/*` to
`localhost:24080`. **Don't add an `app.use(...)` outside the `/api`
router** unless you also add a tunnel ingress rule for it.

## Production runtime (Phase 1)

Containerised on the admin/data box. The systemd unit
(`silencer-admin-api.service`) reads its image ref from
`/etc/silencer/admin-api.image` (an `EnvironmentFile`-shape file
with `IMAGE=ghcr.io/...:<sha>`) and runs:

```
docker run --rm --network host --env-file /etc/silencer/admin-api.env $IMAGE
```

Env file (`/etc/silencer/admin-api.env`, mode 0600) is provisioned by
cloud-init from terraform variables. `MONGO_URL` and `AMQP_URL`
point at `127.0.0.1` (Mongo + LavinMQ run as systemd on the same box,
host-networked).

Deploy: `.github/workflows/deploy-admin-api.yml` is path-filtered to
this directory. It builds an ARM64 OCI image, pushes to GHCR, SSHes
to the admin/data box, overwrites `/etc/silencer/admin-api.image`,
and `systemctl restart silencer-admin-api`. The unit crash-loops
quietly (`Restart=always`) until the first deploy writes a real image
ref.

## Per-file

- `src/index.js` — Express bootstrap + Socket.IO server. Wires
  routes, runs `seed()` (creates `admin/admin` superadmin on first
  boot), starts AMQP consumer + backup scheduler.
- `src/config.js` — Single source of truth for `PORT`, `MONGO_URL`,
  `AMQP_URL`, `JWT_SECRET`, `LOBBY_PLAYER_AUTH_URL`. Defaults
  target local-dev (`localhost:28017`, `localhost:25672`).
- `src/auth/jwt.js` — `signToken` / `verifyToken` + Express
  middlewares: `requireAuth` (any valid JWT), `requirePlayer`
  (type === 'player'), `requireRole(minRole)` (admin role hierarchy).
- `src/db/connection.js` — single `mongoose.connect(MONGO_URL)`.
- `src/db/models/` — Mongoose schemas: `Player`, `Session`, `Event`,
  `MatchStat`, `AdminUser`. The lobby's Go `mongosync.go` writes the
  same `players` collection — schema changes here that affect
  read fields are fine; schema changes that affect write semantics
  must be coordinated with `services/lobby/mongosync.go`.
- `src/routes/` — REST endpoints (one file per top-level path).
  - `auth.js` — admin login, player-login (proxies to lobby
    `/player-auth`), admin-user CRUD.
  - `players.js` — list/detail/match-history; **`PATCH /:id/ban`
    and `DELETE /:id` proxy to the Go lobby's internal HTTP
    (`LOBBY_PLAYER_AUTH_URL` `/ban`, `/delete-player`)** so live
    clients are kicked immediately. Failure to reach the lobby is
    logged but not fatal — the DB write still wins.
  - `me.js` — player self-service (player JWT only).
  - `backup.js` — manual trigger / status / list (admin role).
  - `gamestats.js`, `events.js`, `sessions.js`, `stats.js` — read
    endpoints over the AMQP-persisted models.
- `src/ws/index.js` — Socket.IO server. Handshake auth via JWT in
  `socket.handshake.auth.token`. `liveState` + two `Map`s
  (`onlinePlayers`, `activeGames`) are the in-memory snapshot
  fanned to clients via `snapshot` / per-event broadcasts.
- `src/amqp/consumer.js` — Single AMQP topic queue
  (`admin-dashboard`) bound to exchange `silencer.events` with
  `#`. Each message `persistEvent(type, data)` writes to MongoDB
  **then** calls `handleLobbyEvent(type, data)` to update the live
  WebSocket snapshot. Auto-reconnects every 5 s on connection
  loss.
- `src/backup/manager.js` — `mongodump --archive --gzip` invocation,
  in-progress state guard, prune to `BACKUP_KEEP`, optional GitHub
  upload. **`mongodump` is a runtime dependency**: the Dockerfile
  installs `mongodb-tools`; local dev needs it on PATH.
- `src/backup/github.js` — Commits the latest archive to
  `zsilencer.archive.gz` (filename intentionally preserved across
  the rebrand — it's a stable identifier in the external backup
  repo) in the repo named by `GITHUB_BACKUP_REPO`. Disabled if
  `GITHUB_BACKUP_REPO` or `GITHUB_TOKEN` is unset. (Compose default:
  `Arsia-Mons/silencer-mongo-backup`.) Uses contents API + base64
  upload, no clone.
- `src/backup/scheduler.js` — `node-cron` driver. Schedule from
  `BACKUP_CRON`.

## Invariants

- **Exchange name is `silencer.events`** (renamed from
  `zsilencer.events` in Phase 3) and the queue is `admin-dashboard`.
  Both must agree with `services/lobby/events.go`.
- **Routing key wildcard `#`** — we want every event the lobby
  publishes. If you ever bind a more specific pattern, the live
  snapshot will drift.
- **JWT roles**: rank order in `auth/jwt.js`
  (`viewer < moderator < manager < admin < superadmin`). Adding a
  role means updating the `ROLE_RANK` table; deleting one means
  re-ranking everyone in MongoDB first.
- **Ban / delete must reach the lobby.** REST handlers fire
  `notifyLobbyBan` / `notifyLobbyDelete` after the DB write; the
  lobby's `services/lobby/playerauth.go` listens on
  `LOBBY_PLAYER_AUTH_URL` (default `http://lobby:15171` in compose,
  Docker-internal). If you change either side, update both.
- **Don't sign player JWTs with `type !== 'player'`** — the
  `requirePlayer` middleware filters on this and the player portal
  pages assume it.

## Gotchas

- **Dev port mapping is unusual.** Inside Docker the API listens on
  `:24080`. Mongo and RabbitMQ are exposed at host ports `28017`
  and `25672` respectively (defaults in `config.js`) so local-dev
  outside Docker just works. Inside compose, the URLs become
  `mongodb://mongo:27017/silencer` and
  `amqp://silencer:silencer@rabbitmq:5672/`. In production they're
  both `127.0.0.1` (host-networked Mongo + LavinMQ on the admin/data
  box).
- **Default seed `admin/admin`** runs only if the `AdminUser`
  collection is empty. Don't ship to prod without changing it.
- **`fetch` is global** in Bun (and was in Node 22) — no `node-fetch`
  import needed.
- **`bun install --frozen-lockfile`** is what the Dockerfile uses,
  reading `bun.lock`. If you change dependencies, run `bun install`
  locally so the lockfile updates before commit.
- **Single AMQP queue, durable.** Restarting the API doesn't lose
  events as long as the broker is up; restarting LavinMQ/RabbitMQ
  without durable storage will. The compose volume `rabbitmq-data`
  covers that for dev; in prod the LavinMQ data dir is on its own
  EBS volume (`/var/lib/lavinmq`).
