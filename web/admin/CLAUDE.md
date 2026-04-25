# web/admin/ — Next.js 14 admin dashboard + player portal + designer

App Router, Tailwind, Socket.IO client, JWT-in-localStorage auth.
Routes, env vars, and the auth flow are in `README.md`; this file
is for editing the code. Bun + TS migration is a separate future
phase — the universal "JS = Bun + TS" rule is **deferred** here for
now.

## Per-route

- `app/page.js` — admin dashboard (live online players, active
  games, recent events). Snapshot-driven via `useSocket`.
- `app/login/page.js` — admin login + `?mode=player` toggle for the
  player portal login.
- `app/players/page.js` + `app/players/[accountId]/page.js` — list
  + detail (full lifetime stats, weapon accuracy, paginated match
  history). Detail has Ban/Unban (admin) and Delete (superadmin)
  buttons that hit the API which proxies to the lobby in real time.
- `app/me/page.js` — player self-service portal (own profile +
  match history). Player JWT only.
- `app/sessions/`, `app/events/`, `app/audit/`, `app/health/`,
  `app/changelog/`, `app/howto/`, `app/users/`,
  `app/gamestats/` — read-only or settings pages. `health` also
  hosts the MongoDB backup panel (trigger / status / list).
- `app/designer/` — embedded level designer (folded in here, not
  the standalone `designer/` dir which is being retired in Phase 5).
  Hooks: `useSilMap.js` (parses `.SIL` map files via `pako`),
  `useGameData.js` (fetches the actor/tile catalogues from the
  lobby's map API). `MapCanvas.js` is the core editor surface;
  the panels and context menus are siblings.

## Per-library

- `lib/api.js` — Thin `fetch` wrapper. Reads `zs_token` (admin)
  from localStorage and injects `Authorization: Bearer <token>`.
  All admin RPCs live here as named exports.
- `lib/auth.js` — `useAuth()` / `usePlayerAuth()` hooks redirect
  to the right login page when the matching localStorage key is
  missing. Two storage keys: `zs_token` (admin) and
  `zs_player_token` (player) — they don't overlap; logging out of
  one leaves the other alone.
- `lib/socket.js` — Singleton Socket.IO client over
  `NEXT_PUBLIC_WS_URL`. Reconnects forever (2 s back-off). Sends
  `getSnapshot` on every (re)connect so a freshly-mounted page
  always shows current state, not the cached state from before
  navigation. **Recreates the socket if the JWT changes** — this
  matters when the user logs out and re-logs as a different role.
- `lib/changelog.js` — Static structured changelog data consumed
  by `/changelog`. Add new entries here when shipping features.

## Components

- `components/Sidebar.js` — Nav + WebSocket connection dot. Logo
  src is `/logo.png`.
- `components/StatCard.js` — Single metric tile (`game-*` Tailwind
  tokens defined in `tailwind.config.js`).

## Invariants

- **Two distinct localStorage keys.** `zs_token` for admins,
  `zs_player_token` for players. Don't unify them — the player
  portal must never see admin-scope data on accident.
- **All API URLs from env.** `NEXT_PUBLIC_API_URL` and
  `NEXT_PUBLIC_WS_URL` are baked in at build time (Docker build
  args). Don't hardcode `http://localhost:...` outside the
  fallback in `next.config.mjs` / `lib/socket.js`.
- **`getSocket()` is a singleton** — don't create per-component
  sockets, you'll fan out duplicate `snapshot` events and waste a
  connection per mount.
- **Designer parses raw `.SIL` bytes** in the browser via `pako`.
  The map binary format mirrors the engine's `world.cpp
  AllocateMapData` 64 KiB cap; if you change the parser, mirror it
  in `clients/silencer/src/world.cpp` and the lobby's
  `services/lobby/maps.go` validation.

## Gotchas

- **Standalone Next.js output.** `next.config.mjs` sets
  `output: 'standalone'`. The Dockerfile must explicitly
  `COPY public/` and `.next/static/` to the runner stage — the
  standalone bundle does **not** include them.
- **Dev port is `:24000`.** `npm run dev` and `npm start` both
  bind it. Compose maps host:container 1:1 (`24000:24000`).
- **Socket.IO transports limited to `['websocket']`.** No HTTP
  long-polling fallback; if the API is behind a proxy that
  rewrites WebSocket frames, the dashboard will silently spin on
  "connecting".
- **`'use client'`** at the top of every interactive page/lib —
  forgetting it on a hook file silently moves it to the server
  bundle and breaks `localStorage` access.
- **`output: 'standalone'`** also means `next start` is wrong in
  prod — the Dockerfile invokes `node server.js` directly.
