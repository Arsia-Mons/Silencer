# services/admin-api/ — Express admin REST + WebSocket API

Node.js 22 (ESM) + Express 4 + Socket.IO 4 + Mongoose 8 + amqplib.
Build/run, env vars, and the full route table are in `README.md`;
this file is for editing the code. Bun + TS migration is a separate
future phase — the universal "JS = Bun + TS" rule is **deferred**
here for now.

## Per-file

- `src/index.js` — Express bootstrap + Socket.IO server. Wires
  routes, runs `seed()` (creates `admin/admin` superadmin on first
  boot), starts AMQP consumer + backup scheduler.
- `src/config.js` — Single source of truth for `PORT`, `MONGO_URL`,
  `RABBITMQ_URL`, `JWT_SECRET`, `LOBBY_PLAYER_AUTH_URL`. Defaults
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
  `silencer.archive.gz` in the configured repo (default
  `Arsia-Mons/silencer-mongo-backup`). Uses contents API + base64
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
  `amqp://silencer:silencer@rabbitmq:5672/`.
- **Default seed `admin/admin`** runs only if the `AdminUser`
  collection is empty. Don't ship to prod without changing it.
- **`fetch` is global** (Node 22) — no `node-fetch` import needed.
- **No `package-lock.json` -> `npm ci`** in the Dockerfile — uses
  `npm install --omit=dev`. If you change dependencies, regenerate
  the lockfile so future tooling has it pinned.
- **Single AMQP queue, durable.** Restarting the API doesn't lose
  events as long as RabbitMQ is up; restarting RabbitMQ without
  durable storage will. The compose volume `rabbitmq-data` covers
  that.
