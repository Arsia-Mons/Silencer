# Network audit — silencer client + Go lobby

A complete, line-by-line list of every network call made by the
Silencer C++ client and the Go lobby server. This is a reference
document: each entry says what protocol it speaks, what's in the
payload, what the call accomplishes, and when in the program's life
it fires.

---

## Reading this doc — vocabulary

A few terms come up repeatedly. Quick definitions in web-developer
language:

- **Client / silencer binary.** The C++ game executable (`silencer`).
  The same binary runs in two modes:
  - **Player mode** (no `-s` flag) — the GUI game.
  - **Dedicated-server mode** (started with `silencer -s ...`) — a
    headless game host. The lobby server spawns one of these per
    created game.
- **Lobby (Go).** `services/lobby/`. Acts as the matchmaking /
  presence / chat / accounts service. Speaks several protocols on
  several ports — TCP for the game's metagame protocol, UDP for
  dedicated-server liveness, plus two HTTP servers (community map API,
  internal player auth).
- **Authority vs replica.** Once a match starts, the players form a
  small mesh over UDP. One peer is the **authority** — the one
  running the simulation (think "host"). All other peers are
  **replicas** — they send their inputs to the authority and receive
  world-state snapshots back. In standard Silencer setup the
  authority is the dedicated server the lobby spawned; players are
  always replicas. Replicas display the world; the authority decides
  what's true.
- **"Message type" in a binary protocol.** Like a URL path on a
  REST API, but a single byte. The lobby's TCP protocol assigns 14
  message-type bytes (e.g. byte 0 = "auth attempt", byte 2 = "chat").
  The game's UDP protocol assigns 23. The C++ source calls these
  "opcodes" and `MSG_*` constants; this doc just calls them message
  types and gives each a short name.
- **Frame.** Term overloaded between rendering and networking. In
  this doc, **frame = one length-prefixed message on the lobby TCP
  socket**: `[1-byte length N][N bytes of payload, first byte is the
  message type]`. Max payload is 255 bytes. Multiple message types
  fit per TCP packet; one TCP read may give you several frames or
  half a frame.
- **cstring / lenstr.** Two ways the binary protocols pack a string.
  *cstring* = "bytes until you hit `0x00`", like C strings. *lenstr*
  = `[1-byte length N][N bytes]`, no terminator.
- **u8 / u16 / u32.** Unsigned integers, 1 / 2 / 4 bytes,
  little-endian on the wire.

When a row says "C → S" it means the silencer client sent it to the
Go lobby; "S → C" the other way. For the in-game UDP traffic in section 2
the directions are "R → A" (replica to authority) or "A → R"
(authority to replica) — see the topology note in section 2.

---

## Where each network call lives in the codebase

The cited paths are the source of every claim below. If something
here disagrees with the source, the source is right.

| File | Role |
|---|---|
| `services/lobby/main.go` | Listen-port wiring (TCP, UDP, two HTTP listeners, AMQP startup) |
| `services/lobby/protocol.go` | The 14 lobby TCP message-type numbers + frame codec |
| `services/lobby/client.go` | Server-side handlers + sends per TCP connection |
| `services/lobby/hub.go` | Fan-out, presence, game lifecycle, RabbitMQ publishes, `KickAccountID` UDP send |
| `services/lobby/udp.go` | UDP listener for dedicated-server heartbeats |
| `services/lobby/maps.go` | Public HTTP map API |
| `services/lobby/playerauth.go` | Internal HTTP API used by admin-api |
| `services/lobby/events.go` | RabbitMQ exchange + typed event payloads |
| `services/lobby/mongosync.go` | MongoDB mirror writes |
| `services/lobby/proc.go` | `fork`+`exec` for dedicated-server processes |
| `services/lobby/update.go` | Reads `update.json` from disk (gates, but does not perform, network IO) |
| `clients/silencer/src/net/lobby.h` | C++ side of the lobby TCP message-type enum |
| `clients/silencer/src/net/lobby.cpp` | Connect, parse, send for the lobby TCP protocol |
| `clients/silencer/src/world/world.h` | UDP message-type enum (the in-game P2P protocol) |
| `clients/silencer/src/world/world.cpp` | UDP send/recv. Two dispatch blocks: authority path (`DoNetwork_Authority`) and replica path (`DoNetwork_Replica`) |
| `clients/silencer/src/net/dedicatedserver.cpp` | UDP heartbeat sender (dedicated mode only) |
| `clients/silencer/src/net/mapfetch.cpp` | HTTPS map list / download |
| `clients/silencer/src/actors/actordef.cpp` | HTTPS actor-definition fetch |
| `clients/silencer/src/actors/behaviortree.cpp` | HTTPS behavior-tree fetch |
| `clients/silencer/src/updater/updaterdownload.cpp` | HTTPS auto-update installer download |
| `clients/silencer/src/net/controlserver.cpp` | TCP loopback control socket (test/CLI automation) |
| `clients/silencer/src/net/controldispatch.cpp` | The op→handler table for the control socket |
| `clients/silencer/src/platform/config.h` | Two distinct URL config keys: `mapapiurl`, `adminapiurl` |
| `clients/cli/index.ts` | Bun TS consumer of the control socket |

The two URL config keys are different by design. `mapapiurl` points
at the lobby's built-in map HTTP server. `adminapiurl` points at the
separate admin-api service (out of scope for this doc; the silencer
client just GETs JSON from it). The default values come from compile
flags `SILENCER_MAP_API_URL` and `SILENCER_ADMIN_API_URL`; both can
be overridden in the user's config file.

---

## Listen ports the lobby opens at startup

| Port (default) | Protocol | Purpose | Source |
|---|---|---|---|
| `:517` | **TCP** | Game-client lobby connections | `main.go:96` (`net.ListenTCP`) |
| `:517` | **UDP** | Dedicated-server heartbeats from spawned children | `main.go:106` (`net.ListenUDP`) |
| `:8080` | **HTTP** | Public map API (`-map-api-addr` flag, prod uses `:15172`) | `main.go:118` (`go StartMapAPIServer`) |
| `:15171` | **HTTP** | Internal player-auth API (`-player-auth-addr` flag) | `main.go:112` (`go StartPlayerAuthServer`) |

The same port number `:517` carries two unrelated protocols: TCP for
clients, UDP for the lobby's own child processes phoning home. They
share the port number but are independent listeners on different IP
protocols.

The lobby also opens an outbound AMQP connection to RabbitMQ /
LavinMQ at startup if `-amqp-url` or `$AMQP_URL` is set
(`events.go:31`, `amqp.Dial`), and an outbound MongoDB connection if
`$MONGO_URL` is set (`mongosync.go:40`, `mongo.Connect` followed by
`client.Ping`). Both reconnect on drop. Neither is required.

The silencer client opens one listen port of its own:

| Port | Protocol | Purpose | Source |
|---|---|---|---|
| OS-assigned ephemeral | UDP | The peer-to-peer game socket. Same socket sends *and* receives every UDP message in section 2 below. Advertised to other peers via `LobbyGame.hostname` ("ip,port" string). | `world.cpp:1555` (`Bind`) |
| `--control-port <N>` | TCP loopback (`127.0.0.1` only) | JSON-line control socket for test automation. Only opens if `--control-port` is passed. | `controlserver.cpp:35-46` |

---

## 1. Lobby TCP protocol — the metagame

Persistent TCP connection, plaintext, framed as `[1-byte
length][payload]`, payload max 255 bytes. The first payload byte is
the message-type number; everything after that is the body for that
message type.

**Lifecycle of the connection itself:**
1. Client `connect()`s on launch (`lobby.cpp:55`, `Lobby::Connect`).
2. First thing the client sends is a "version" message (#1.1 below).
3. If the version check passes, the user is shown the login screen;
   typing credentials sends "auth" (#1.5).
4. If auth passes, the server immediately fan-outs MOTD chunks
   (#1.8/#1.9), the current channel name (#1.10), every known game
   (#1.12), and presence entries for every other logged-in user
   (#1.25). After this initial burst the connection stays open for
   the rest of the session — chat, presence, game create/join, etc.
   all flow on it.
5. The lobby sends a "ping" (#1.20) every 10 s and disconnects the
   client after 30 s of read silence (`client.go:52`). The client
   replies with its own ping (#1.21).
6. Either side closes the TCP socket to disconnect (`client.go:65`
   triggers `hub.Leave`).

There are 14 message-type numbers (`protocol.go:9-23`,
`lobby.h:24`). Several have multiple shapes depending on direction
(client→server vs server→client) or status flag, so the table below
has 26 rows. One number (`opConnect = 6`) is reserved but never used
on the wire — both sides have the constant defined, but the
client-side handler is commented out (`lobby.cpp:262-326`) and the
server has no case for it.

### TCP message types

| # | Name (number) | Direction | Payload bytes (after the message-type byte) | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 1.1 | `Version` (7) — request | C → S | cstring `version` (≤64 chars), then 1 byte `platform` (0=unknown, 1=mac arm64, 2=win x64) | Tell the lobby what version+platform the client is, before anything else. | First frame after TCP connect. Sent from `Lobby::SendVersion` (`lobby.cpp:477`). |
| 1.2 | `Version` (7) — accept | S → C | u8 `success=1` | Server says "you're current, proceed". | Reply to 1.1 when version matches `-version` flag. |
| 1.3 | `Version` (7) — reject + update info | S → C | u8 `success=0`, u16 `urlLen`, `urlLen` bytes URL, 32 bytes SHA-256 of installer | Tell the client "you're outdated, here's where to download the new build". | Reply to 1.1 when version mismatches AND the lobby has an `update.json` manifest with a URL for this platform. URL+SHA come from `update.go::LoadManifest` reading `update.json` off disk; flag `-update-manifest`. |
| 1.4 | `Version` (7) — reject bare | S → C | u8 `success=0` | "Outdated, but I don't have an updater URL for your platform." Client shows "version mismatch" without an auto-update offer. | Reply to 1.1 when version mismatches and no manifest entry for the client's platform. |
| 1.5 | `Auth` (0) — request | C → S | cstring `username` (≤17 incl. null), 20 bytes SHA-1 of the password | Log in. SHA-1 of plaintext password — both username and hash go over plaintext TCP. | Sent from `Lobby::SendCredentials` (`lobby.cpp:490`) when the user submits the login form. |
| 1.6 | `Auth` (0) — success | S → C | u8 `success=1`, u32 `accountId` | "You're in. Here's your numeric account id." | Reply to 1.5 on match. Client transitions into the "logged in" state and the post-auth fan-out (1.8–1.25) follows immediately. |
| 1.7 | `Auth` (0) — failure | S → C | u8 `success=0`, cstring error message ("Incorrect password for X" or "Account suspended: X") | Login failed. | Reply to 1.5 on bad password / banned account. The client shows the error, stays disconnected. |
| 1.8 | `MOTD` (1) — chunk | S → C | cstring `text` (≤200 chars per chunk) | One slice of the message-of-the-day. The client appends each chunk into its `motd` buffer. | Right after 1.6. Server walks the MOTD file in 200-char chunks (`client.go:200-217`), one chunk per frame. |
| 1.9 | `MOTD` (1) — terminator | S → C | u8 `0x00` | "End of MOTD." Client stops buffering. | Sent right after the last 1.8 chunk. |
| 1.10 | `Channel` (5) | S → C | cstring `channelName` | "You are now in chat channel X." Client updates its current-channel display. | After auth (default channel "Lobby"), and again whenever the user `/join`s a channel. |
| 1.11 | `NewGame` (3) — create request | C → S | Full `LobbyGame` struct: u32 id (ignored, server assigns), u32 ownerAccountId (ignored, server fills), lenstr name, lenstr password, lenstr hostname, lenstr mapName, 20 bytes mapHash, u8 players, u8 state, u8 securityLevel, u8 minLevel, u8 maxLevel, u8 maxPlayers, u8 maxTeams, u8 extra, u16 port | "Create a new game with these settings." | User clicks Create Game. Sent from `Lobby::CreateGame` (`lobby.cpp:536`). The lobby reserves a game id, spawns a dedicated-server child, and waits for that child's first UDP heartbeat (#3.1) before replying. |
| 1.12 | `NewGame` (3) — existing-games push | S → C | u8 `status=1`, then full `LobbyGame` struct | "Here's a game in the list." Sent once per existing game. | Initial fan-out right after 1.6 (`hub.go:71`). The client populates its game list. |
| 1.13 | `NewGame` (3) — game ready / updated push | S → C | u8 `status=1`, full `LobbyGame` | A game just became ready (its dedicated server reported in) or its state changed. | Triggered by an incoming UDP heartbeat (#3.1) into `hub.OnHeartbeat` — broadcast to everyone (`hub.go:319-321`, `hub.go:344-352`). Also the success reply to the requester of 1.11. |
| 1.14 | `NewGame` (3) — create rejected | S → C | u8 `status=2`, full `LobbyGame` | "Your game-create attempt failed." Client shows "Could not create game" dialog. | Sent only to the requester of 1.11, when the dedicated-server child does not heartbeat back within 30 s (`hub.go:269` → `failPending` → `hub.go:291`). |
| 1.15 | `DelGame` (4) | S → C | u32 `gameId` | "Remove this game from your list." | Fired when a game's owner disconnects (`hub.go:139`) or when an owner creates a new game so the previous one is dropped (`hub.go:248`). |
| 1.16 | `Chat` (2) — request | C → S | cstring `channel` (≤64), cstring `message` (≤255) | Send a chat line. If `message` starts with "/join ", the lobby treats it as a channel switch instead of broadcasting it (and replies with #1.10, not a chat broadcast). | Sent from `Lobby::SendChat` and `Lobby::JoinChannel` (`lobby.cpp:506`, `:524`). |
| 1.17 | `Chat` (2) — broadcast | S → C | cstring `channel`, cstring `text` (already prefixed with "displayName: "), u8 `color` (palette index; 0 = white), u8 `brightness` (128) | Display a chat line. | Sent by `hub.Chat` (`hub.go:356`) to every other client in the same channel. |
| 1.18 | `UserInfo` (8) — request | C → S | u32 `accountId` | Client wants the profile for this account — display name + agencies + stats. | Sent from `Lobby::GetUserInfo` (`lobby.cpp:614`) lazily, the first time the UI needs to render someone's name (e.g. presence list, peer in a game). |
| 1.19 | `UserInfo` (8) — reply | S → C | u32 `accountId`, then 5 × `Agency` struct (each: u16 wins, u16 losses, u16 xpToNextLevel, u8 level, u8 endurance, u8 shield, u8 jetpack, u8 techSlots, u8 hacking, u8 contacts), then lenstr `name` | Profile data. | Reply to 1.18 (`client.go:303-322`). For unknown accountIds the server returns a stub `User{Name: "Unknown"}` so the client doesn't hang. |
| 1.20 | `Ping` (9) — server keepalive | S → C | empty | Server's keepalive. | Every 10 s from `Client.pingLoop` (`client.go:69-80`). Resets the client's "are we still connected" timer. |
| 1.21 | `Ping` (9) — client reply | C → S | u8 `0x01` (one byte after the message-type byte; the C++ code writes `msg[0]=MSG_PING; msg[1]=1`) | Client's ack of the server ping. | Sent from `Lobby::DoNetwork` MSG_PING case (`lobby.cpp:392-398`). The server's `handleFrame` accepts the opcode and ignores the body (`client.go:117`). |
| 1.22 | `UpgradeStat` (10) — request | C → S | u8 `agency` (0–4), u8 `stat` (which stat to upgrade) | "Spend XP to bump one stat for one of my agencies." | Sent from `Lobby::UpgradeStat` (`lobby.cpp:642`) when the user clicks an upgrade arrow on the stats screen. |
| 1.23 | `UpgradeStat` (10) — ack | S → C | empty (just the message-type byte; no body) | "I applied the upgrade." Client invalidates its cached `User` for that account so the next render fetches fresh values via 1.18/1.19. | Reply to 1.22 (`client.go:337` — `c.send([]byte{opUpgradeStat})`). Only sent on success; on insufficient XP / invalid input nothing comes back. |
| 1.24 | `RegisterStats` (11) | C → S | u32 gameId, u8 teamNumber, u32 accountId, u8 statsAgency, u8 won, u32 xp, then 34 × u32 stats (= 136 bytes): per-weapon `[fires, hits, playerKills]` × 4 weapon slots, then 22 scalar counters in this order: civiliansKilled, guardsKilled, robotsKilled, defenseKilled, secretsPickedUp, secretsReturned, secretsStolen, secretsDropped, powerupsPickedUp, deaths, kills, suicides, poisons, tractsPlanted, grenadesThrown, neutronsThrown, empsThrown, shapedThrown, plasmasThrown, flaresThrown, poisonFlaresThrown, healthPacksUsed, fixedCannonsPlaced, fixedCannonsDestroyed, detsPlanted, camerasPlanted, virusesUsed, filesHacked, filesReturned, creditsEarned, creditsSpent, healsDone | "Match ended; here are this player's match stats." Server stores them and republishes as RabbitMQ events #5.5 and #5.6. | Sent at end-of-match from `Lobby::RegisterStats` (`lobby.cpp:650`). One frame per player whose stats are being recorded. No reply. |
| 1.25 | `Presence` (12) | S → C | u8 `action` (0=upsert, 1=remove), u32 `accountId`, u32 `gameId` (0 if in main lobby), u8 `status` (0=lobby, 1=pregame, 2=playing), lenstr `name` | "This player just appeared / changed game / disappeared." | Sent on auth (full backfill of every other client + self), on `SetClientGame` (#1.26), and on disconnect (`hub.go:144`). Drives the in-lobby presence list. |
| 1.26 | `SetGame` (13) | C → S | u32 `gameId` (0 = back to main lobby), u8 `status` (0=lobby, 1=pregame, 2=playing) | "I'm now in game N's pregame screen / I just started playing N / I'm back in the main lobby." Server validates and rebroadcasts as #1.25. | Sent when the user joins/leaves a game lobby or transitions into gameplay. From `Lobby::SendSetGame` (`lobby.cpp:575`). |

---

## 2. UDP peer-to-peer game traffic

Once a player is in a game, all gameplay flows over UDP between
peers. The dedicated server (running `silencer -s ...`) is the
authority. Player clients are replicas. Each peer has a single UDP
socket — it's the same OS handle for sending and receiving every
message in this section.

The C++ client has **two separate UDP dispatch loops** depending on
the local mode:

- **Authority path** — `world.cpp:228-595`, `World::DoNetwork_Authority`.
  Handles inbound from replicas. Sends snapshots/peer-list/etc.
- **Replica path** — `world.cpp:614-820`, `World::DoNetwork_Replica`.
  Handles inbound from the authority. Sends inputs / chat / etc.

**Topology is hub-and-spoke, not a free mesh.** Every UDP packet is
between the authority and one or more replicas. Replicas *never* send
UDP directly to other replicas — when a replica needs to broadcast
something (e.g. chat), it sends to the authority, which fans out.
Confirmed by walking every `SendPacket()` call site in `world.cpp`:
replica-originated sends always target `GetAuthorityPeer()`;
multi-recipient sends only happen inside `if(mode == AUTHORITY)`
branches.

Direction in the table below is therefore either "R → A" (replica →
authority) or "A → R" (authority → replica). A few message types
flow both ways depending on who initiates — those are noted per row.

23 message types in total (`world.h:200-202`). Their names below
match the C++ enum (`MSG_*`). Two more, `MSG_VIRUS` and `MSG_REPAIR`,
exist as commented-out blocks (`world.cpp:470-495`); their semantics
have been folded into `MSG_STATION` subcodes 1 and 2 — no traffic
uses those names today.

### UDP message types

| # | Name (ordinal) | Direction | Payload | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 2.1 | `MSG_CONNECT` (0) — join request | R → A | u8 agency, u32 accountId, u8 passwordSize, then `passwordSize` bytes of password | "I want to join your game with this agency and creds." | Sent by `World::Connect` (`world.cpp:1581`) when a player clicks Join Game. |
| 2.2 | `MSG_CONNECT` (0) — join response | A → R | 1 bit `accepted`. If accepted: u8 `peerId` (your slot in the game's peer table). | "Yes/no, and if yes here's your peer id." Replica needs the peerId for everything it later sends. | Reply to 2.1 (`world.cpp:303`). |
| 2.3 | `MSG_SNAPSHOT` (1) | A → R | u32 `tickCount`, u32 `theirLastTick`, then a delta-encoded world snapshot (variable size, can be ~0.5–2 KB) | The world. Authority's view of every game object. | Every simulation tick (~30 Hz) per replica peer. From `World::SendSnapshots` (`world.cpp:1755`). |
| 2.4 | `MSG_INPUT` (2) | R → A | u32 tickCount, u32 lastTick, then serialized `Input` struct (movement bits, mouse coords, action flags) | "Here's what I'm pressing this tick." Authority feeds this into the simulation. | Per replica tick during gameplay, from `World::SendInput` (`world.cpp:1623`). |
| 2.5 | `MSG_PEERLIST` (3) — request | R → A | empty | "Send me the current peer list." | Sent right after a successful 2.2 by `World::RequestPeerList` (`world.cpp:1837`). |
| 2.6 | `MSG_PEERLIST` (3) — push | A → R | A serialized list of all `Peer` structs (id, accountId, agency, ready flag, host flag, gameInfoLoaded flag, mapDownloaded flag, controlled-object list). Length-self-delimited. | The full set of who-is-in-the-game and their state. | Reply to 2.5, plus rebroadcast every time a peer's flags change (joins, ready toggles, agency changes, team changes, map-downloaded transitions). From `World::SendPeerList` (`world.cpp:1846`). |
| 2.7 | `MSG_DISCONNECT` (4) — replica leaving | R → A | empty | "I'm leaving the game." | Sent by `World::Disconnect` (`world.cpp:1609`) when the replica leaves. |
| 2.8 | `MSG_DISCONNECT` (4) — authority shutting down | A → R | empty | "Game is over / authority is going away. Tear down." | Sent by authority's `World::Disconnect` to every replica (`world.cpp:1604`). |
| 2.9 | `MSG_PING` (5) | R → A | u32 `pingId` | Round-trip latency probe. | Every 1 s from a replica (`world.cpp:602` triggers `SendPing`, which puts MSG_PING + pingId; `world.cpp:2174-2181`). |
| 2.10 | `MSG_PONG` (6) | A → R | u32 `pingId` (echoed) | Echo back the ping id so the replica can compute RTT. | Reply to 2.9 (`world.cpp:336-339`). The replica records the round-trip into a 10-slot ping history. |
| 2.11 | `MSG_GAMEINFO` (7) — host → authority | R(host) → A | A serialized `GameInfo` struct (map name, password, security level, level range, max players, max teams, …) | The human host of a game tells the dedicated-server authority the rules of the match. (Only the player marked `ishost` does this.) | Sent by the host replica when the authority requests it (`world.cpp:1773-1777`). |
| 2.12 | `MSG_GAMEINFO` (7) — authority broadcast | A → R | Serialized `GameInfo` | "Here are the match rules." Replica caches them. | Authority forwards 2.11 to every other replica (`world.cpp:358-363`). |
| 2.13 | `MSG_GAMEINFO` (7) — replica ack | R → A | empty (just the message-type byte) | "I have the gameinfo loaded." | Reply to 2.12 (`world.cpp:1781`). Authority uses this flag in 2.6. |
| 2.14 | `MSG_READY` (8) | R → A | empty | Toggle the replica's "ready" flag (in the pregame screen). | Sent on the player checking the Ready box (`world.cpp:1788`). Authority flips the flag and rebroadcasts the peer list (2.6). |
| 2.15 | `MSG_CHAT` (9) — replica → authority | R → A | u8 `to` (0=all, 1=team), then a null-terminated chat string (max ~256 bytes) | In-game chat, sent up to be relayed. | Player typed in the in-game chat box (`world.cpp:1951-1958`). |
| 2.16 | `MSG_CHAT` (9) — authority fan-out | A → R | u32 `senderAccountId`, then null-terminated text | Distribute chat. Authority filters by team if `to==1`. | Authority fans out to peers (`world.cpp:407-419`). Replicas display via `DisplayChatMessage` (`world.cpp:704`). |
| 2.17 | `MSG_STATION` (10) | R → A | u8 `subcode` (0=BUY, 1=REPAIR, 2=VIRUS), u8 `id` | "Player at the station performed action X on item id Y." Authority calls Player::BuyItem / RepairItem / VirusItem. | Sent on UI clicks at a buy/repair station (`world.cpp:1408`, `:1416`, `:1424`). |
| 2.18 | `MSG_CHANGETEAM` (11) | R → A | empty | "Move me to the next team in my current agency." | Sent on the player clicking Change Team in the pregame screen (`world.cpp:2002`). Authority updates teams and rebroadcasts the peer list (2.6). |
| 2.19 | `MSG_STATUS` (12) | A → R | A null-terminated status string (HUD-style overlay text) followed by 1 byte duration and 1 byte color | "Show this status line in the HUD." | Pushed by authority via `World::ShowStatus` when a gameplay event needs displaying (`world.cpp:1924`). |
| 2.20 | `MSG_MESSAGE` (13) | A → R | A null-terminated message string, then u8 `time`, u8 `type` | Big centered message (e.g. "MISSION COMPLETE"). Differs from 2.19 in size + lifecycle. | Sent by `World::ShowMessage` (`world.cpp:1891-1899`). |
| 2.21 | `MSG_GOVTKILL` (14) | A → R | u8 `peerId` | "This peer's player has been killed by the in-game government NPCs." | Sent by `World::KillByGovt` (`world.cpp:2017-2023`). |
| 2.22 | `MSG_SOUND` (15) | A → R | u8 `volume`, then null-terminated sound bank name | "Play this sound effect at this volume." | Sent by `World::SendSound` (`world.cpp:1967`). Replicas look up the name in their local sound bank. |
| 2.23 | `MSG_TECH` (16) | R → A | u32 `techChoices` (bitmask of selected tech upgrades) | "I'm bringing this tech loadout." | Sent in pregame from `World::SetTech` (`world.cpp:2083`). |
| 2.24 | `MSG_STATS` (17) | A → R | Serialized `Stats` blob (the same 34-u32 layout as #1.24 above) | Authoritative end-of-match stats for this replica's player, so the client knows what to register with the lobby (#1.24). | Sent by `World::SendStats` (`world.cpp:1477-1480`). |
| 2.25 | `MSG_EXISTS` (18) | A → R | A run of u16 `objectId`s (variable count). | "Tell me which of these objects you still have — I think they may be stale on your side." (Authority asks replica to confirm presence so it can reissue removes for ones that died while replica was offline.) | Sent periodically by `World::CheckExists` (`world.cpp:1033-1052`) for selected object types: pickups, fixed cannons, detonators. |
| 2.26 | `MSG_REMOVE` (19) | R → A | A run of u16 `objectId`s | "I don't have these — please remove them from your authoritative state too." | Reply to 2.25 (`world.cpp:530-538`). Authority then marks those object ids destroyed (`world.cpp:782`). |
| 2.27 | `MSG_MAP` (20) — replica reports done | R → A | u8 subcode `MAP_DOWNLOADED=0` | "I have the full map; you can mark me ready." | Sent by `World::SendMapDownloaded` (`world.cpp:2107-2109`). |
| 2.28 | `MSG_MAP` (20) — request chunk | usually R → A; A → R when authority lacks the map | u8 subcode `MAP_GETCHUNK=1`, u32 `offset` | "Send me bytes [offset … offset+1024] of the current map." | Common case: a replica that joined without the map asks the authority for chunks. Inverted case: if the dedicated server itself doesn't have a community map locally, it asks the host player (who already downloaded it from the lobby's map HTTP API) — see `world.cpp:2139-2150`. Either way, the authority is always one endpoint; replicas never request from each other. |
| 2.29 | `MSG_MAP` (20) — chunk push | mirror of 2.28 | u8 subcode `MAP_PUTCHUNK=2`, u32 `offset`, u32 `size`, then `size` bytes | "Here are bytes [offset … offset+size] of the map." | Reply to 2.28 — direction is whichever side received the request. A → R when a replica asked, R → A when the authority asked the host player. From `World::PutMapChunk` (`world.cpp:2114-2130`). Max 1024 bytes per chunk. |
| 2.30 | `MSG_SETAGENCY` (21) | R → A | u8 `agency` | "Move me to a team in this agency." | Sent in pregame from `World::SetAgency` (`world.cpp:2010`). |
| 2.31 | `MSG_KICK` (22) — admin → dedicated | special: see **section 4.4** below | u32 accountId | "Kick this player from the game." | The lobby itself injects this packet from a separate UDP socket when an admin bans a player who is currently in a game. The dedicated-server authority receives it on its own UDP game socket and disconnects the matching peer. |

A few sanity guards in the authority loop:

- A peer that hasn't sent any UDP packet for 10 s is auto-disconnected (`world.cpp:586-595`, `peertimeout`).
- The `MSG_KICK` handler only honors packets whose source IP equals `dedicatedserver.lobbyaddress` (`world.cpp:569`) — it's loopback-only because the lobby always spawns the dedicated on `127.0.0.1`.

---

## 3. UDP heartbeat (dedicated-server → lobby)

A dedicated-server process needs to tell the lobby it's alive and
what game it's hosting so the lobby can flip the game's status from
"pending" to "ready".

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Direction** | Dedicated (`silencer -s ...`) → lobby |
| **Endpoint** | The lobby's `:517` UDP listener. Always `127.0.0.1` because the lobby spawns the dedicated process locally and passes the lobby's loopback address to it as a CLI argument (`proc.go:43-49`). |
| **Source (sender)** | `dedicatedserver.cpp:61-74` (`SendHeartBeat`) |
| **Source (receiver)** | `services/lobby/udp.go:11-35` (`serveUDP`) |
| **Payload** | 8 bytes total: `[0x00 (subtype byte)][u32 gameId LE][u16 boundPort LE][u8 state]`. `state`: 0=in-lobby/pregame, 1=in-game. |
| **Purpose** | (a) Confirm to the lobby that the dedicated process started successfully, including which UDP port the players should connect to. (b) Keep updating the game's `state` so the lobby can rebroadcast it to other clients via #1.13. (c) Keep-alive — if no heartbeat arrives within the 30-second pending window, the lobby kills the spawn and tells the requester (#1.14). |
| **Lifecycle** | Sent every ~3.3 s for the lifetime of the dedicated process — `Tick` runs once per simulation tick (~30 Hz) and only sends every 100th tick (`dedicatedserver.cpp:50-58`). First heartbeat triggers `Hub.OnHeartbeat` → fan-out 1.13. Subsequent heartbeats only fan-out if `state` changed. |

---

## 4. Lobby HTTP — public map API (`mapapiurl`)

This is a regular HTTP server that lives **inside the lobby** binary
on a separate port (default `:8080`, prod `:15172` behind Cloudflare
+ admin proxy). It is what the silencer client config key
`mapapiurl` points at — distinct from the unrelated `adminapiurl`.

CORS is enabled (any origin, including `null` for `file://`).
`-map-upload-key` adds an `X-Api-Key` requirement to the two
mutating endpoints (POST and DELETE); GET endpoints are always
unauthenticated.

| # | Method + path | Auth | Request | Response | Purpose | Lifecycle |
|---|---|---|---|---|---|---|
| 4.1 | `GET /api/maps` | none | — | `application/json` array of `{sha1, name, size, author, uploaded_at}` | Browse what community maps the server has. | Called by the silencer client from `FetchServerMapList` and `FetchAndSyncServerMaps` (`mapfetch.cpp:173-249`) when the user opens the Create Game / map picker screen. Also called by the admin web app to list maps. |
| 4.2 | `POST /api/maps` | `X-Api-Key` (if `-map-upload-key` set) | Headers `X-Filename`, `X-Author`. Body: raw `.SIL` bytes, max 65535. | 201 Created with the new `MapMeta` JSON | Upload a new community map. | Called by the admin web app's level designer on Save. The silencer client never calls this endpoint. |
| 4.3 | `GET /api/maps/by-sha1/{sha1hex}` | none | — | `application/octet-stream` (raw `.SIL` bytes), `Content-Disposition` attachment with the original filename | Download a specific map by content hash. | Silencer client calls this from `FetchMapFromServer` (`mapfetch.cpp:86-171`) when it joins a game whose advertised `mapHash` (sent inside the `LobbyGame` struct, see #1.12/#1.13) doesn't match any local file. After download, the client SHA-1-verifies the bytes before saving (`mapfetch.cpp:144-150`). |
| 4.4 | `GET /api/maps/{name}` | none | — | Raw `.SIL` bytes | Download a map by filename instead of hash. | Used by the admin web app's designer when the operator clicks a map name. The silencer client does not call this directly — it always goes through `by-sha1`. |
| 4.5 | `DELETE /api/maps/{name}` | `X-Api-Key` | — | `{deleted: name}` | Remove a community map. | Admin web only. |

`OPTIONS` requests on `/api/maps` and `/api/maps/...` return `204 No
Content` for CORS preflight.

---

## 5. Lobby HTTP — internal player auth (`-player-auth-addr`)

Default `:15171`. Bound to whatever the operator sets — in practice
only reachable inside the Docker network. Used **only** by the
admin-api service (which is out of scope for this doc). The silencer
client never calls these endpoints. 5-second read+write timeout per
request. Bodies capped at 256 bytes.

| # | Method + path | Request body | Response | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 5.1 | `POST /player-auth` | `{name: string, sha1Hex: string}` (40-char hex of SHA-1(password)) | `{ok: true, accountId: u32, name: string}` on match, `{ok: false}` (or `{ok: false, error: "invalid sha1"}`) on miss | Validate a player's lobby credentials so the admin-api can let them log in to the admin web with the same username/password. | Admin web login flow. Once per attempted login. |
| 5.2 | `POST /ban` | `{accountId: u32, banned: bool}` | `{ok: bool}` (true = account existed and was updated) | Set or clear an account's banned flag in the lobby's user store. As a **side effect** when `banned=true`, the lobby calls `hub.KickAccountID` which (a) closes the player's lobby TCP socket if open, and (b) fires the UDP kick packet in section 4.4 below to any in-game dedicated server. | Admin clicks Ban / Unban on the dashboard. |
| 5.3 | `POST /delete-player` | `{accountId: u32}` | `{ok: bool}` | Remove an account from the lobby's user store. Triggers a Mongo delete (#7.2) via `store.DeletePlayer`. | Admin clicks Delete on the dashboard. |

---

## 6. Lobby outbound UDP — admin kick into a running game

Almost-hidden network call: when a `POST /ban` (#5.2) lands and the
banned player is currently inside a running game, the lobby sends a
UDP packet *from the lobby to the dedicated-server child process* to
forcibly disconnect the player from gameplay (without that, the ban
only takes effect at the lobby layer; the player would keep running
around inside the dedicated until the match ended).

| | |
|---|---|
| **Protocol** | UDP, plaintext |
| **Direction** | Lobby → dedicated server (the `silencer -s ...` child) |
| **Endpoint** | `127.0.0.1:<gamePort>` (always loopback; `gamePort` is the dedicated's bound game port that came in on heartbeat #3) |
| **Source (sender)** | `services/lobby/hub.go:374-411` (`KickAccountID`) — opens a fresh UDP socket via `net.DialUDP`, writes 5 bytes, closes |
| **Source (receiver)** | `clients/silencer/src/world/world.cpp:568-583` (`MSG_KICK` case, authority-side dispatch) |
| **Payload** | 5 bytes: `[22][u32 accountId LE]`. The `22` is the C++ `MSG_KICK` enum ordinal (the 23rd entry in the `MSG_*` enum, see `world.h:200-202`). |
| **Purpose** | Tell the dedicated to drop the player from the game right now. The dedicated receives it on its normal UDP game socket, validates the source IP equals its known lobby address (`world.cpp:569`), looks up the matching peer by accountId, kills the player and disconnects them. |
| **Lifecycle** | Fired only on `POST /ban` with `banned=true` for an account currently in a game (`playerauth.go:73`). Once per ban event. |

---

## 7. Lobby outbound RabbitMQ events (`-amqp-url` / `$AMQP_URL`)

The lobby publishes events to a topic exchange named
`silencer.events` (durable). Connection is opened at startup and
auto-reconnects with 5 s backoff (`events.go:29-68`). Disabled
entirely if no AMQP URL is configured. All publishes are
fire-and-forget on a background goroutine (`events.go:82-91`),
transient (not persisted in RMQ), JSON body, ~100–500 bytes.

Connection lifecycle calls (in addition to the publishes):
- `amqp.Dial(url)` — once at startup, plus on every reconnect
  (`events.go:31`)
- `conn.Channel()` — once per successful connect (`events.go:37`)
- `ch.ExchangeDeclare("silencer.events", "topic", durable=true, ...)`
  — once per successful connect (`events.go:44`)
- `ch.Publish(...)` — one per event listed below

| # | Routing key | Body fields | Triggered by | Purpose |
|---|---|---|---|---|
| 7.1 | `player.login` | `accountId, name, ip, agencies[5], ts` | `hub.go:93` — fired in `Hub.Join` on auth success | Notify the admin dashboard of a new login. |
| 7.2 | `player.logout` | `accountId, name, ts` | `hub.go:153` — fired in `Hub.Leave` when the TCP socket drops | Notify the dashboard the player went offline. |
| 7.3 | `player.presence` | `accountId, name, gameId, gameStatus, online, ts` | `hub.go:197` — fired in `SetClientGame` after the lobby applies an `opSetGame` (#1.26) | Live-update which game/state each player is in. |
| 7.4 | `player.upgrade` | `accountId, agencyIdx, statId, agency, ts` | `client.go:339` — after a successful `opUpgradeStat` (#1.22/#1.23) | Reflect the stat change in the dashboard. |
| 7.5 | `player.stats_update` | `accountId, agencyIdx, agency, ts` | `client.go:441` — after `opRegisterStats` (#1.24) succeeds | "This player's totals just changed." |
| 7.6 | `player.match_stats` | `accountId, gameId, agencyIdx, won, xp, stats (full 34-u32 blob), ts` | `client.go:445` — after `opRegisterStats` succeeds | The full per-match breakdown — separate event because it's bigger and the dashboard subscribes to it differently. |
| 7.7 | `game.created` | `gameId, accountId, name, mapName, ts` | `hub.go:264` — when the lobby accepts an `opNewGame` request and starts spawning the dedicated server (before the dedicated has actually heartbeat-ed in) | "A user just created a new game." |
| 7.8 | `game.ready` | `gameId, accountId, name, mapName, hostname, port, ts` | `hub.go:325` — when the dedicated's first UDP heartbeat (#3) arrives and the game flips from pending to live | "Game is now joinable, here's the address." |
| 7.9 | `game.ended` | `gameId, ts` | `hub.go:151, 254` — when the owner disconnects, or when the owner creates a new game so the previous one is dropped | "Game removed from the list." |

---

## 8. Lobby outbound MongoDB (`$MONGO_URL`)

Disabled if `$MONGO_URL` is unset. Targets a database/collection
called `silencer.players`. Mongo is treated as a read-only mirror
for the admin dashboard — the source of truth is `lobby.json` on the
lobby's filesystem. Password hashes are intentionally never sent.
All writes are async; errors logged, not retried.

| # | Operation | Filter / data | Triggered by | Purpose |
|---|---|---|---|---|
| 8.0 | `mongo.Connect` + `client.Ping` | the `$MONGO_URL` connection | Once at lobby startup (`mongosync.go:40-45`). If either fails, sync is permanently disabled for this process — it's not retried. | Open the connection; gate whether 8.1–8.3 will ever do anything. |
| 8.1 | `UpdateOne` (upsert) on `players` | filter `{accountId}`, set `{accountId, callsign, banned, agencies[5], lastSeen}` | `mongosync.go:55-76` (`SyncPlayer`), called on every player mutation in the lobby's user store: login refresh, stat upgrade, match-stats update, ban toggle | Keep the Mongo mirror up-to-date row by row. |
| 8.2 | `DeleteOne` on `players` | filter `{accountId}` | `mongosync.go:79-90` (`DeletePlayer`), called from `POST /delete-player` (#5.3) | Remove a player from the mirror. |
| 8.3 | Bulk upsert (calls 8.1 in a goroutine for every user) | every entry in `lobby.json` | `mongosync.go:94-104` (`SyncAll`), called once at startup right after the connection succeeds | Make sure Mongo reflects current `lobby.json` even if the lobby was offline. |

---

## 9. Silencer client outbound HTTPS (libcurl)

Four distinct call sites in the C++ client, hitting two different
config keys.

### 9.1 `mapapiurl` — community map server (= the lobby's HTTP API in section 4)

| # | Method + URL | Source | Timing | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 9.1.1 | `GET {mapapiurl}/api/maps/by-sha1/{sha1hex}` | `mapfetch.cpp:86-171` (`FetchMapFromServer`) | 3 s connect, 10 s total. Follows redirects. SHA-1 of body verified before saving. Response capped at 65 535 bytes. | Fetch a specific map by hash. | Called when the player opens a game whose `LobbyGame.mapHash` doesn't match a locally-known map. |
| 9.1.2 | `GET {mapapiurl}/api/maps` | `mapfetch.cpp:200-219` (`FetchServerMapList`) — internal `FetchMapListJSON`. Same JSON body returned by section 4.1. | 1 s connect, 2 s total. Response capped at 1 MB. | Browse remote maps. | Called when the user opens the Create Game screen. Gives the user a list of remote maps they could host. |
| 9.1.3 | `GET {mapapiurl}/api/maps` then per-entry `GET {mapapiurl}/api/maps/by-sha1/{hex}` | `mapfetch.cpp:221-250` (`FetchAndSyncServerMaps`) | Each download: 3 s connect, 10 s total. | "Mirror every server map I don't have." Downloads the list, then for every entry whose filename is not already in `level/download/`, calls 9.1.1 internally. | Called at level-picker open time to pre-cache community maps. |

### 9.2 `adminapiurl` — admin-api service (separate from the lobby)

| # | Method + URL | Source | Timing | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 9.2.1 | `GET {adminapiurl}/api/actors` | `actordef.cpp:217-233` (`FetchActorDefs`, list step) | 3 s connect, 5 s total. 4 MB cap. | Get the list of NPC actor IDs that have server-side definitions. | Called once per map load before gameplay starts. |
| 9.2.2 | `GET {adminapiurl}/api/actors/{id}` (one per id from 9.2.1) | `actordef.cpp:240-260` (per-actor loop) | Same shape. | Fetch one actor's animation/audio/hitbox JSON definition. | Called once per actor id returned by 9.2.1, sequentially. |
| 9.2.3 | `GET {adminapiurl}/api/behaviortrees` | `behaviortree.cpp:316-331` (`FetchBehaviorTrees`, list step) | 3 s connect, 5 s total. 4 MB cap. | Get the list of NPC AI tree IDs. | Once per map load. |
| 9.2.4 | `GET {adminapiurl}/api/behaviortrees/{id}` | `behaviortree.cpp:333-352` (per-tree loop) | Same shape. | Fetch one behavior-tree JSON definition. | Once per tree id returned by 9.2.3, sequentially. |

### 9.3 Updater installer download (URL comes from #1.3)

| # | Method + URL | Source | Timing | Purpose | Lifecycle |
|---|---|---|---|---|---|
| 9.3.1 | `GET <url-from-version-reply>` | `updaterdownload.cpp:63-114` (`UpdaterDownload::Fetch`) | 15 s connect timeout. Up to 5 redirects. SHA-256 verified after download. | Download the platform installer (`.dmg` / `.exe`) the lobby pointed the client at via #1.3. Only HTTPS is allowed; HTTP is permitted only when the host resolves to `127.0.0.1`/`localhost`/`[::1]` (`updaterdownload.cpp:6-29`). | Fired once at startup, only on the version-rejected-with-update path, and only after the user clicks Download in the modal. |

The user-agent strings differ per call site:
`silencer/<version>` (mapfetch), `silencer-actordef/1`,
`silencer-behaviortree/1`, `silencer-updater/1.0`.

---

## 10. Silencer client TCP control socket (test/automation)

A loopback-only TCP listener the client opens at boot if
`--control-port <N>` is passed. Used by the Bun TS CLI
(`clients/cli/index.ts`) and other test harnesses to drive the game
from outside.

| | |
|---|---|
| **Protocol** | TCP, loopback only (`INADDR_LOOPBACK`, i.e. `127.0.0.1`), plaintext, **JSON Lines** (`\n`-terminated JSON objects). 1 MiB max per line. |
| **Source (server)** | `clients/silencer/src/net/controlserver.cpp:28-62` (`Start`), `:181-228` (`HandleConnection`) |
| **Source (dispatcher)** | `clients/silencer/src/net/controldispatch.cpp` |
| **Source (consumer)** | `clients/cli/index.ts` |
| **Frame on the wire** | Request: `{"id": int, "op": string, "args": {...}}\n`. Reply: `{"id": int, "ok": true, "result": ...}\n` or `{"id": int, "ok": false, "code": "...", "error": "..."}\n`. One reply per request. |
| **Lifecycle of the connection** | Each TCP connection runs its own request/reply loop. Connection stays open as long as the CLI client wants — the Bun CLI in `clients/cli/index.ts` always sends one request and disconnects, but the server supports many on one connection. Listen backlog 16. Server runs for the silencer process lifetime once started. |

### Ops the control socket understands

Each row is one valid `op` string. These are not network message
types in the binary-protocol sense — they're JSON commands. Phase
controls when in the game's frame loop the op runs (`controldispatch.cpp:22-27`).

| # | `op` | Phase | Args | Purpose |
|---|---|---|---|---|
| 10.1 | `ping` | immediate | — | Liveness probe; returns `{version, build, frame, paused}`. |
| 10.2 | `state` | immediate | — | Returns `{state, current_interface_id, frame, paused}`. |
| 10.3 | `inspect` | immediate | optional `interface_id: u16` (defaults to current) | Returns the widget tree for an interface — buttons, toggles, textboxes, selectboxes, with positions/labels/state. |
| 10.4 | `world_state` | immediate | — | Returns a JSON summary of in-game world state (`Game::GetWorldSummary`). |
| 10.5 | `click` | immediate | `label: string` or `id: int` | Find a button/toggle by label and activate it. |
| 10.6 | `set_text` | immediate | `label: string`, `text: string` | Find a textbox by label and set its content. |
| 10.7 | `select` | immediate | `label: string`, plus `index: int` *or* `text: string` | Pick an item in a selectbox by index or text match. |
| 10.8 | `back` | immediate | — | Go back one screen (returns `{went_back: bool}`). |
| 10.9 | `quit` | immediate | — | Set `quitRequested = true` to exit the program. |
| 10.10 | `pause` | immediate | — | Pause the simulation (rejected with `WRONG_STATE` while in live multiplayer). |
| 10.11 | `resume` | immediate | — | Unpause. |
| 10.12 | `screenshot` | post-render | optional `out: string` (defaults to `/tmp/silencer-<frame>.png` or `%TEMP%\silencer-<frame>.png`) | Save a PNG of the current frame *after* the next render finishes. Returns `{path}`. |
| 10.13 | `wait_frames` | multi-frame | `n: int` | Block the caller's HTTP-style request until `n` simulation frames have elapsed. |
| 10.14 | `wait_ms` | multi-frame | `n: int` | Block until `n` ms of wall-clock have passed. |
| 10.15 | `wait_for_state` | multi-frame | `state: string`, optional `timeout_ms` (default 5000) | Block until the named game state is current. Times out with `TIMEOUT` code. |
| 10.16 | `step` | multi-frame | `frames: int` *or* `ms: int` | Pause, advance the sim by N frames (or N ms of wall-clock catch-up), pause again. |

Unknown ops respond `{ok:false, code:"UNKNOWN_OP"}`. Malformed JSON
responds `{ok:false, code:"BAD_REQUEST"}`. On server shutdown,
in-flight ops are completed with `{ok:false, code:"INTERNAL", error:
"server stopping"}` (`controlserver.cpp:98-107`).

---

## 11. Process spawn (not a network call but it kicks off section 3)

| | |
|---|---|
| **Mechanism** | `os/exec` `fork`+`exec`, not network |
| **Source** | `services/lobby/proc.go:33-78` (`procManager.Start`) |
| **Command line** | `silencer -s 127.0.0.1 <lobbyPort> <gameId> <accountId> [<gamePort>]` |
| **Why this is in a network audit** | The spawned child opens its own UDP socket, sends the heartbeats listed in section 3, and accepts/sends every UDP message in section 2 as the authority. Every one of those calls only exists because of this `exec`. |
| **Lifecycle** | Per accepted `opNewGame` (#1.11). Killed when the lobby decides the game is over (owner disconnects, `Hub.Leave`/`RequestCreateGame` drop), if no heartbeat lands within 30 s (`pendingTimeout`), or on lobby shutdown (`StopAll`). |
