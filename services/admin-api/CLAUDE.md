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

## Production runtime

Containerised on the admin/data box: ARM64 image → GHCR → SSH →
image-ref swap → `systemctl restart`. Workflow is
`.github/workflows/deploy-admin-api.yml`, path-filtered. Systemd
unit + env file + Mongo/LavinMQ co-location are described in
`infra/terraform/CLAUDE.md`.

## Per-file

- `src/index.js` — Express bootstrap + Socket.IO. Runs `seed()`
  (creates `admin/admin` superadmin on first boot), starts AMQP
  consumer + backup scheduler.
- `src/config.js` — single source of truth for env defaults
  (`localhost:28017`, `localhost:25672` for local-dev outside Docker).
- `src/auth/jwt.js` — middlewares: `requireAuth`, `requirePlayer`
  (type === 'player'), `requireRole(minRole)`.
- `src/db/models/` — Mongoose schemas. `ActorDef.js` and `BehaviorTree.js`
  are present but **not used by current routes** — actordefs and BTs are
  filesystem-only. They remain as legacy stubs. The lobby's
  `services/lobby/mongosync.go` writes the same `players`
  collection — coordinate any schema change that affects write
  semantics.
- `src/routes/actors.js` — actor definition endpoints (filesystem-based).
  `GET /actors` lists IDs; `GET /actors/:id` returns JSON; `PUT /actors/:id`
  writes to `shared/assets/actordefs/<id>.json` (admin only); `DELETE` removes
  the file. The game client fetches these via the `adminapiurl` config key on
  each map load — no client rebuild needed after editing in the actor editor.
- `src/routes/behaviortrees.js` — behavior tree endpoints, same shape as
  actors. Reads/writes `shared/assets/behaviortrees/<id>.json`. Node type
  whitelist enforced server-side (`Selector`, `Sequence`, `Leaf`, etc.).
  The game client fetches these at startup for the BT interpreter.
- `src/routes/players.js` — `PATCH /:id/ban` and `DELETE /:id`
  proxy to the lobby's internal HTTP (`LOBBY_PLAYER_AUTH_URL`)
  so live clients are kicked. Lobby unreachable is logged but
  not fatal — DB write wins.
- `src/ws/index.js` — Socket.IO server. JWT in
  `socket.handshake.auth.token`. `liveState` +
  `onlinePlayers` / `activeGames` `Map`s are the in-memory
  snapshot fanned via `snapshot` / per-event broadcasts.
- `src/amqp/consumer.js` — single durable topic queue
  `admin-dashboard` bound to `silencer.events` with `#`.
  Each message: `persistEvent` (Mongo write) **then**
  `handleLobbyEvent` (live snapshot update). Auto-reconnects.
- `src/backup/manager.js` — `mongodump --archive --gzip`.
  **`mongodump` is a runtime dependency**: Dockerfile installs
  `mongodb-tools`; local dev needs it on PATH.
- `src/backup/github.js` — commits the archive as
  `zsilencer.archive.gz` (filename intentionally preserved across
  the rebrand — stable identifier in the external backup repo).
  Disabled if `GITHUB_BACKUP_REPO` or `GITHUB_TOKEN` is unset.

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
