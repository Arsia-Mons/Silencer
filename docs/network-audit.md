# Network audit — silencer client + Go lobby

Every wire-level network call between the Silencer C++ client (game
binary, also runs as a headless dedicated server with `-s`) and the
Go lobby server (`services/lobby/`). The point is to have one place
that says exactly what's flying over the wire today, so we can decide
whether the shape is right before adding more.

The lobby's binary TCP opcodes are exhaustively documented in
[`shared/lobby-protocol/protocol.md`](../shared/lobby-protocol/protocol.md).
This doc references that file rather than duplicating the per-opcode
payloads.

> **Scope**: silencer client + lobby server only. The admin API
> (`services/admin-api/`) and admin web (`web/admin/`) are downstream
> of the lobby's RabbitMQ event stream and the map API; we note where
> the client/lobby cross those boundaries but don't audit those
> services here.

---

## 1. Silencer client → outbound

### 1.1 TCP lobby connection

| | |
|---|---|
| **Protocol** | TCP, plaintext, persistent |
| **Endpoint** | `SILENCER_LOBBY_HOST:SILENCER_LOBBY_PORT` (compile-time, default `127.0.0.1:517`; CI sets `lobby.arsiamons.com`) |
| **Source** | `clients/silencer/src/lobby.cpp:55-84` (Connect), `lobby.cpp:101-450` (DoNetwork) |
| **Payload** | Custom binary framing `[len:u8][opcode:u8][payload]`, max 255-byte payload. 14 opcodes. See `shared/lobby-protocol/protocol.md` |
| **Purpose** | Authenticate, list/create games, chat, presence, end-of-match stats — the entire metagame. |
| **Lifecycle** | Opens at startup → version handshake → auth → continuous until user logs out or game ends. Re-auth on reconnect (no session tokens). |
| **Timing** | Server pings every 10 s; reference client read timeout 20 s; server read deadline 30 s. |

### 1.2 UDP peer-to-peer game traffic

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Endpoint** | Per-peer `ip:port` discovered via the lobby's `opNewGame.hostname` field (`"ip,port"` string) |
| **Source** | `clients/silencer/src/world.cpp:22-24` (socket), `world.cpp` `DoNetwork_Authority` / `DoNetwork_Replica` |
| **Payload** | Custom message types (`MSG_PING`, `MSG_INPUT`, `MSG_SNAPSHOT`, `MSG_CONNECT`, `MSG_DISCONNECT`, `MSG_GAMEINFO`, `MSG_CHAT`, etc.). Authority broadcasts snapshots; replicas send inputs. |
| **Purpose** | The actual game. Authority simulates; replicas send inputs and apply deltas. |
| **Lifecycle** | After joining a game (`opSetGame` status=playing) until game ends or peer leaves. |
| **Timing** | Tick rate ~30 Hz. Authority emits snapshots every tick (~500–2000 bytes); replicas emit inputs ~1–4×/sec. |

### 1.3 UDP heartbeat to lobby (dedicated-server mode only)

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Endpoint** | Lobby `ip:port` (passed as `silencer -s <lobbyaddr> <lobbyport> ...` args; always `127.0.0.1` because the lobby spawns the dedicated locally) |
| **Source** | `clients/silencer/src/dedicatedserver.cpp:61-74` (`SendHeartBeat`) |
| **Payload** | 8 bytes fixed: `[0x00][game_id:u32 LE][port:u16 LE][state:u8]` |
| **Purpose** | Tells the lobby this dedicated process is alive and what game it's hosting. Lobby aborts pending creates if no heartbeat in 30 s. |
| **Lifecycle** | From dedicated-server boot until process exit. |
| **Timing** | Every ~100 ticks ≈ 3.3 s at 30 Hz. |

### 1.4 HTTPS map download (admin API)

| | |
|---|---|
| **Protocol** | HTTPS via libcurl |
| **Endpoint** | `GET {adminapiurl}/api/maps` (list), `GET {adminapiurl}/api/maps/by-sha1/{sha1hex}` (download) |
| **Source** | `clients/silencer/src/mapfetch.cpp:86-169` (`FetchMapFromServer`) |
| **Payload** | GET, no body. Response: list is JSON array `[{name, sha1}, ...]`; download is raw `.SIL` map file (≤65535 bytes). |
| **Purpose** | Pull community-uploaded maps that aren't bundled with the client. |
| **Lifecycle** | Triggered when player joins a game whose map hash isn't found locally; also when opening Create Game screen. |
| **Timing** | 3 s connect, 10 s total per request. SHA-1 verified after download. |

### 1.5 HTTPS actordef fetch (admin API)

| | |
|---|---|
| **Protocol** | HTTPS via libcurl |
| **Endpoint** | `GET {adminapiurl}/api/actors` (list of IDs), `GET {adminapiurl}/api/actors/{id}` (one definition) |
| **Source** | `clients/silencer/src/actordef.cpp:217-262` (`FetchActorDefs`) |
| **Payload** | GET, no body. Response: JSON. List is `["player","guard",...]`; per-actor is the JSON `ActorDef` (sequences, frames, hurtboxes). |
| **Purpose** | Hot-load NPC animation/audio/hitbox definitions from the admin tier without rebuilding the client. |
| **Lifecycle** | On every map load, before gameplay starts. |
| **Timing** | 3 s connect, 5 s total per request. Total response cap 4 MB across all actors. |

### 1.6 HTTPS behavior tree fetch (admin API)

| | |
|---|---|
| **Protocol** | HTTPS via libcurl |
| **Endpoint** | `GET {adminapiurl}/api/behaviortrees` (list of IDs), `GET {adminapiurl}/api/behaviortrees/{id}` (one tree) |
| **Source** | `clients/silencer/src/behaviortree.cpp:303-349` (`FetchBehaviorTrees`/`BTCurlGet`) |
| **Payload** | GET, no body. Response: JSON tree definition (Selector/Sequence/Leaf nodes). |
| **Purpose** | Hot-load NPC AI logic without rebuilding the client. |
| **Lifecycle** | On every map load. |
| **Timing** | Same shape as actordef. |

### 1.7 HTTPS update installer download

| | |
|---|---|
| **Protocol** | HTTPS via libcurl (HTTP allowed only for `127.0.0.1`) |
| **Endpoint** | URL returned by lobby's `opVersion` rejection payload |
| **Source** | `clients/silencer/src/updaterdownload.cpp:63-113` (`Fetch`) |
| **Payload** | GET, no body. Response: binary installer (`.dmg` / `.exe`). |
| **Purpose** | Auto-update the client when the lobby rejects on version mismatch. |
| **Lifecycle** | Once at startup, only on version-rejection path; user must consent via modal first. |
| **Timing** | 15 s connect timeout, follows up to 5 redirects. SHA-256 verified after download. |

---

## 2. Silencer client → inbound (listeners)

### 2.1 UDP P2P game socket

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Port** | OS-assigned ephemeral; advertised to peers via `opNewGame.hostname` |
| **Source** | `clients/silencer/src/world.cpp:22-24` |
| **Purpose** | Receive the other side of section 1.2 — inputs (if AUTHORITY) or snapshots (if replica). |
| **Lifecycle** | Same socket as 1.2 — bind on game start, close on game end. Same socket also serves the dedicated-server heartbeat send (1.3). |

### 2.2 TCP CLI control socket

| | |
|---|---|
| **Protocol** | TCP loopback only (`INADDR_LOOPBACK`), plaintext, JSON-lines |
| **Port** | `--control-port <n>` flag |
| **Source** | `clients/silencer/src/controlserver.cpp:28-62` (`Start`), `controlserver.cpp:126+` (Accept/Handle) |
| **Payload** | Newline-delimited JSON: request `{"id":int,"op":string,"args":object}`; response `{"id":int,"ok":bool,"result":...|"error":string}`. 1 MiB line cap. |
| **Purpose** | Headless automation for tests + agent-driven UI verification (see `tests/cli-agent/` and `clients/cli/`). |
| **Lifecycle** | Started at boot iff `--control-port` supplied; runs for the process lifetime. Listen backlog 16. |

---

## 3. Go lobby → inbound (listeners)

### 3.1 TCP — game lobby protocol

| | |
|---|---|
| **Protocol** | TCP, plaintext |
| **Port** | `-addr` (default `:517`; dev `:15170`) |
| **Source** | `services/lobby/main.go:96` (`net.ListenTCP`), `services/lobby/client.go` (per-conn handler) |
| **Payload** | Same binary protocol as 1.1 — see `shared/lobby-protocol/protocol.md`. |
| **Purpose** | Accept silencer-client connections; serve the metagame. |
| **Lifecycle** | One goroutine per accepted connection (`serveClient`). 30 s read deadline; pings every 10 s. |

### 3.2 UDP — dedicated server heartbeats

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Port** | Same as `-addr`, just UDP (default `:517`) |
| **Source** | `services/lobby/main.go:106` (`net.ListenUDP`), `services/lobby/udp.go:11-35` (`serveUDP`) |
| **Payload** | 8 bytes — see 1.3. |
| **Purpose** | Track liveness of dedicated server processes the lobby spawned, and the game state (lobby/pregame/playing). |
| **Lifecycle** | Single goroutine for the server lifetime. 512-byte read buffer. |

### 3.3 HTTP — public map API

| | |
|---|---|
| **Protocol** | HTTP, plaintext |
| **Port** | `-map-api-addr` (default `:8080`; prod `:15172`, fronted by Cloudflare Tunnel + admin HTTPS proxy) |
| **Source** | `services/lobby/maps.go:184-295` (`StartMapAPIServer`) |
| **Endpoints** | `GET /api/maps` (list JSON), `POST /api/maps` (upload `.SIL`, requires `X-Api-Key`), `GET /api/maps/by-sha1/{hex}`, `GET /api/maps/{name}` |
| **Payload** | Map files are raw bytes ≤65535. Listing is JSON `[{name, sha1, size, author, upload_time}]`. CORS open. |
| **Purpose** | Section 1.4's server side. Plus designer-uploaded community maps. |
| **Lifecycle** | Long-lived HTTP server. |

### 3.4 HTTP — internal player auth

| | |
|---|---|
| **Protocol** | HTTP, plaintext (intra-VM only) |
| **Port** | `-player-auth-addr` (default `:15171`) |
| **Source** | `services/lobby/playerauth.go:15-110` |
| **Endpoints** | `POST /player-auth` (validate creds → accountId), `POST /ban`, `POST /delete-player` |
| **Payload** | JSON, ≤256 bytes per request |
| **Purpose** | Lets the admin web app authenticate users against the same store the lobby uses, and admin-mutate (ban/delete). |
| **Lifecycle** | 5 s read+write timeouts per request. Long-lived HTTP server. |

---

## 4. Go lobby → outbound

### 4.1 Process spawn — dedicated server

| | |
|---|---|
| **Protocol** | `fork`+`exec`, not network. Listed here because it kicks off the network calls in 1.2/1.3/2.1. |
| **Source** | `services/lobby/proc.go:33-78` (`procManager.Start`) |
| **Command** | `silencer -s 127.0.0.1 <lobbyport> <gameid> <accountid> [<gameport>]` |
| **Lifecycle** | Per `opNewGame` request from a client. Killed on lobby shutdown or after 30 s without UDP heartbeat. |
| **Notes** | Always loopback because the C++ client's `inet_addr()` only parses dotted-decimal. |

### 4.2 MongoDB — async player mirror

| | |
|---|---|
| **Protocol** | MongoDB wire (TCP+BSON) via `mongo-driver`. Disabled if `MONGO_URL` unset. |
| **Source** | `services/lobby/mongosync.go:55-100` (`SyncPlayer`, `SyncAll`) |
| **Payload** | BSON `{accountId, callsign, banned, agencies[5], lastSeen}`. Password hashes never sent. |
| **Purpose** | Mirror player state into Mongo for the admin dashboard. `mongo.json` on disk is the source of truth; Mongo is a read replica. |
| **Lifecycle** | Fire-and-forget per mutation, on a background goroutine. 5 s timeout per op. Errors logged, not retried. |

### 4.3 RabbitMQ — async event stream

| | |
|---|---|
| **Protocol** | AMQP 0.9.1 via `amqp091-go`. Disabled if `AMQP_URL` unset. |
| **Source** | `services/lobby/events.go:29-91` (connect + Publish) |
| **Exchange** | `silencer.events` (topic, durable). Routing keys like `player.joined`, `player.stats_updated`, `game.created`. |
| **Payload** | JSON event body, transient delivery, ~100–500 bytes. |
| **Purpose** | Live-feed the admin web dashboard. |
| **Lifecycle** | Persistent AMQP connection with reconnect (5 s backoff). Silently drops events while disconnected. |

---

## 5. One-page summary

```
┌────────────────────────────┐
│  Silencer client (C++)     │
└────────────────────────────┘
   │  TCP :517 (lobby protocol)        ───────►  Lobby
   │  UDP ephemeral (peer-to-peer)     ◄──────►  Other clients / dedicated
   │  UDP :517 heartbeat (dedi only)   ───────►  Lobby
   │  HTTPS adminapiurl/api/maps,/actors,/behaviortrees  ───►  Admin API
   │  HTTPS update url                 ───────►  Updater host (CDN/S3)
   ▼  TCP loopback :--control-port      ◄──────  CLI test harness

┌────────────────────────────┐
│  Lobby (Go)                │
└────────────────────────────┘
   ▲  TCP :517                          ◄──────  Clients
   ▲  UDP :517                          ◄──────  Dedicated heartbeats
   ▲  HTTP :8080  (/api/maps)           ◄──────  Clients + admin web
   ▲  HTTP :15171 (/player-auth, /ban)  ◄──────  Admin API
   │  exec silencer -s ...               ─────►  Dedicated child process
   │  MongoDB (BSON)                     ─────►  Mongo
   │  AMQP 0.9.1                         ─────►  RabbitMQ / LavinMQ
```

| | TCP | UDP | HTTP/S | Other |
|---|---|---|---|---|
| Client out | lobby :517 | P2P, dedi heartbeat | maps, actordefs, BT, updater | — |
| Client in | CLI ctrl (loopback) | P2P | — | — |
| Lobby out | — | — | — | exec, Mongo, AMQP |
| Lobby in | :517 | :517 | :8080, :15171 | — |

---

## 6. Audit — is the shape right?

The criteria, restated: are we leaving performance on the table, are
we clean architecturally without over-engineering, and are we boxing
ourselves in?

### What's right and should stay

- **Lobby protocol over a single TCP socket.** Persistent, framed,
  delta-driven. Right shape for a chat/presence/game-list service —
  the same pattern as Discord's gateway, IRC, etc. Doesn't try to be
  REST and shouldn't.
- **UDP for in-game simulation.** Peer-to-peer with one AUTHORITY is
  fine for this game's scale (≤24 players). Snapshot/input split is
  the standard model.
- **Lobby's outbound is async + fire-and-forget.** Mongo and AMQP
  publishes don't block client-facing TCP handlers. Mongo and AMQP
  failures degrade gracefully (lobby keeps working, dashboard goes
  stale). Right call.
- **`fork`+`exec` for dedicated servers.** One process per game is
  simple, isolates crashes, and lets the same binary serve as both
  client and dedicated. Not over-engineered.
- **Admin API on a separate HTTP service.** Uploads, designer,
  community maps don't belong on the lobby's hot path. Map fetches
  by SHA-1 hash are correctly cacheable forever.
- **CLI control socket on loopback only.** Right scope — automation
  hook without exposing a third public listener.

### Where the shape is questionable

The findings, ranked roughly by concern level:

#### A. Synchronous HTTPS on the map-load path (medium-high)

**The thing.** When a map loads, the client serially does:
1. `GET /api/maps` (list)
2. Maybe `GET /api/maps/by-sha1/...` (download missing map)
3. `GET /api/actors` + per-actor `GET /api/actors/{id}` for every actor on the map
4. `GET /api/behaviortrees` + per-tree `GET /api/behaviortrees/{id}`

**Why it matters.** That's potentially dozens of round-trips per map
load, each with a 3 s connect timeout and a 5–10 s total timeout. If
the admin API is slow, the player sees a frozen "Loading…" screen.
At scale (many clients joining at once), there's a thundering herd
on the admin API every time a popular map starts.

**Lower-effort fixes.**

- Bundle the actor list and behavior-tree list into single endpoints
  that return *all* definitions for a given map, server-side joined.
  Cuts N requests to 1.
- Add `ETag` / `If-None-Match` so the client skips downloads when
  nothing changed. Map files keyed by SHA-1 are already
  immutable-by-content; actordefs and BTs aren't.
- Cache aggressively client-side. Today actordefs/BTs are re-fetched
  on every map load even if the same map was just played.

**Higher-effort fix.** Push these definitions over the lobby TCP
socket as part of the post-auth initial sync, the same way games and
presence already are. That removes a whole protocol from the
critical path. Probably overkill for content that rarely changes.

#### B. The `adminapiurl` is a third URL the client has to know (medium)

The client has to discover three endpoints: lobby host (compile-time),
lobby map API URL (compile-time? per-build?), and `adminapiurl`
(config). Each one is a different resolution mechanism. Operationally
this is a lot of knobs to keep aligned across dev, staging, prod, and
they have already drifted (commit `4c5ef22` was a fix where one of
these was wrong).

**Cleanup option.** Have the lobby's `opVersion` reply (or a new
`opConfig` opcode after auth) tell the client where the admin API
lives. Single source of truth: the lobby. The client only needs the
lobby address; everything else is discovered. Costs one new opcode
or a few extra bytes on `opVersion` reply; saves a config knob and a
compile-time constant.

#### C. No reconnect on lobby TCP drop (medium)

If the lobby socket dies mid-session, the client today disconnects
the player entirely. There's no exponential-backoff reconnect, no
session resume. For a player in a chat lobby this is fine; for a
player about to join a game, it's a frustrating round trip back to
the menu.

**Why we haven't hit it.** The lobby is colocated with clients in dev
and inside the same VM in prod, so TCP drops are rare. But this is
exactly the kind of thing that becomes a problem the day the lobby
moves behind a load balancer or restarts during deploy.

**The minimal version.** Reconnect with the same credentials and
rebuild local state from the post-auth fire-hose. The protocol
already redelivers all games + presence on auth, so the *protocol*
supports it cleanly — the client just doesn't try.

#### D. Two listeners on `:517` for two different protocols (low-medium)

TCP for clients, UDP for dedicated heartbeats, both on the same port
number. This works but it's surprising — the protocols are unrelated
and have different audiences (clients vs. lobby-spawned children).
Splitting them onto different ports would make the firewall story
clearer and would let the heartbeat port be loopback-only (it never
needs to be public — the heartbeat is purely intra-host between
lobby and its child processes).

**Effort: trivial.** New flag, default it to a private port, done.
This is a "while you're nearby" cleanup, not a now-now thing.

#### E. SHA-1 password hashing on the wire (low — security, not urgent)

`opAuth` sends `username + sha1(password)` over plaintext TCP. SHA-1
is broken; plaintext TCP means a network-position attacker captures
the hash and replays it (same as the password). The only defense
today is "the lobby is on a trusted network."

**The right answer if/when we care.** TLS-wrap the lobby socket
(stunnel or a Go TLS listener) and switch to a modern KDF
(argon2id) server-side. Until either (a) the lobby is exposed beyond
trusted networks, or (b) we add account-portable identities (Steam
auth, OAuth), this is fine to leave.

#### F. RabbitMQ is the only path for admin events (low)

If RabbitMQ is down and Mongo is down at the same time, the dashboard
sees nothing. Both have independent reconnect logic so this only
matters during a real outage. Worth knowing; not worth designing
around yet.

#### G. No rate limiting anywhere (low)

Lobby happily accepts unlimited auth attempts, chat, game creates per
client. Fine on a private/whitelisted server; would matter the day
this is opened up. Cheap to add when it's actually needed (one
token-bucket struct per `*client`).

### What would *not* be a good change right now

- **Replacing the binary lobby protocol with WebSocket+JSON.** Tempting
  if you come from web, but the binary protocol is small, the spec
  is now machine-readable (`vectors.json`), there are three SDK
  implementations agreeing on it, and the bandwidth profile is
  exactly what you want for a chat/presence service. Switching costs
  a lot and gains you readable-tcpdump-output. Not worth it.
- **HTTP/2 or gRPC for lobby.** Same answer — the existing protocol
  is already as cheap on the wire as gRPC and far simpler.
- **Split lobby into microservices.** It's ~2k lines of Go that does
  one job. Zero reason to fragment it.
- **Build our own peer-to-peer netcode framework.** What's there
  works for the player count we target. The interesting netcode work
  (lag compensation, rollback, etc.) is independent of where the
  bytes flow.

---

## 7. Recommendations, ordered by ROI

| # | Change | Effort | Payoff |
|---|---|---|---|
| 1 | Bundle actor/BT fetches into one map-keyed endpoint, add ETag caching | Low | Removes the per-map-load HTTP storm; biggest user-visible win |
| 2 | Lobby distributes admin-API URL after auth; client stops carrying `adminapiurl` config | Low | One source of truth, kills a class of misconfig bugs |
| 3 | Lobby TCP reconnect with state rebuild on drop | Medium | Required before any deploy that restarts the lobby live |
| 4 | Split UDP heartbeat onto its own (private) port | Trivial | Cleaner ops story; safer firewall |
| 5 | Rate-limit per-client on the lobby | Low | Only needed when exposing publicly |
| 6 | TLS the lobby socket + modern KDF | Medium | Only needed when exposing publicly |

Items 1–4 are work we'd benefit from regardless of whether the lobby
ever goes public. Items 5–6 only matter on the day we open the lobby
beyond trusted networks; track them but don't pre-build.
