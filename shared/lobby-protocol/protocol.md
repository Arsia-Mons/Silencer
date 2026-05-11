# Silencer lobby wire protocol

Authoritative reference: [`services/lobby/protocol.go`](../../services/lobby/protocol.go).
This document mirrors that file in human-readable form. The C++ and
TS SDKs in [`clients/lobby-sdk/`](../../clients/lobby-sdk/) implement
what's described here, and the golden vectors in
[`vectors.json`](./vectors.json) are consumed by both SDK test suites
(and may be consumed by `services/lobby/` itself in the future).

If the Go server and this document disagree, the **server wins** —
update this doc and bump opcodes / version on both sides per the
invariant in [`services/lobby/CLAUDE.md`](../../services/lobby/CLAUDE.md).

## Transport

- TCP, default port `:517` (configurable server-side via `-addr`).
- Little-endian, byte-aligned binary (no bit-packing despite the
  client's `Serializer` being bit-aligned — every lobby field happens
  to land on a byte boundary).
- All numeric multi-byte fields are LE.

## Framing

Every message on the wire is one frame:

```
[len u8][opcode u8][payload …]
```

- `len` is the count of bytes that follow it (`opcode` + `payload`),
  so the whole frame on the wire is `1 + len`.
- `len` is at minimum `1` (opcode only) and at most `255`. A `len`
  of `0` is a protocol error.
- Implementations MUST NOT split or merge logical frames; one frame
  = one logical message.

## Field encodings

| Type      | Encoding                                                                  |
|-----------|---------------------------------------------------------------------------|
| `u8`      | one byte                                                                  |
| `u16`     | two bytes, little-endian                                                  |
| `u32`     | four bytes, little-endian                                                 |
| `bytes(n)`| `n` raw bytes                                                             |
| `cstr`    | UTF-8/ASCII bytes followed by a single `0x00` terminator                  |
| `lenstr`  | `u8` length followed by exactly that many bytes (no terminator, no UTF-8 validation) |

`cstr` readers SHOULD bound the search by a max length to avoid
runaway reads on malformed input.

## Opcodes

```
opAuth          = 0
opMOTD          = 1
opChat          = 2
opNewGame       = 3
opDelGame       = 4
opChannel       = 5
opConnect       = 6   (reserved, unused on the wire today)
opVersion       = 7
opUserInfo      = 8
opPing          = 9
opUpgradeStat   = 10
opRegisterStats = 11
opPresence      = 12
opSetGame       = 13
```

## Connection lifecycle

1. Client opens TCP connection to `host:port`.
2. Client sends `opVersion` request.
3. Server replies with `opVersion` (OK or reject + optional update info).
4. If OK, client sends `opAuth` with username + SHA-1(password).
5. Server replies with `opAuth` (success → account ID, or failure → error message).
6. On success, server pushes:
   - `opMOTD` chunks then a terminator (`opMOTD` with empty payload),
   - `opChannel` (current channel name),
   - one or more `opNewGame` / `opPresence` frames per existing game/player.
7. Server sends `opPing` every 10 s. Client SHOULD reply with
   `[opPing, 0x01]` (the C++ client does; the server does not check
   the body — any frame on the connection resets the read deadline).
8. Server's read deadline is **30 s**. The C++ client closes after
   **20 s** of silence. SDKs SHOULD pick a value <30 s.
9. Either side may close the socket; the server fans out `opDelGame`
   for any game owned by the leaver and `opPresence` (action=remove)
   to peers.

## Per-opcode payloads

Unless otherwise noted, "request" means client→server and
"reply"/"push" means server→client.

### `opAuth` (0)

**Request** (`C → S`):
```
cstr   username      (max 16 chars + null)
bytes  password_sha1 (20 bytes, raw SHA-1 of password)
```

**Reply** (`S → C`):
```
u8     status        (1 = success, 0 = failure)
if status == 1:
  u32  account_id
if status == 0:
  cstr error_message
```

### `opMOTD` (1)

**Push** (`S → C`), repeated, terminated by an empty MOTD frame:
```
cstr   text_chunk    (server chunks at 200 bytes)
```
Terminator: a single `[opMOTD, 0x00]` frame (zero-length cstr =
just the null terminator). Clients accumulate chunks until terminator.

### `opChat` (2)

**Request** (`C → S`):
```
cstr   channel
cstr   message
```
A message starting with `/join ` is interpreted server-side as a
channel switch (the rest of the message is the new channel name).

**Push** (`S → C`):
```
cstr   channel
cstr   message
u8     color         (palette index, 0 = white)
u8     brightness    (0–255, default 128)
```

### `opNewGame` (3)

**Request** (`C → S`): create a new game. Wire format is the full
`LobbyGame` struct (see below); server overrides `account_id`,
`players=1`, `state=0`.

**Push** (`S → C`):
```
u8           status   (1 = success / advertise existing, 2 = create failed)
LobbyGame    game
```
A push where `game.account_id == self.account_id` AND `status == 2`
means "your CreateGame request failed" (e.g. heartbeat timeout from
the spawned dedicated). Otherwise it's a normal advertisement.

### `opDelGame` (4)

**Push** (`S → C`):
```
u32    game_id
```

### `opChannel` (5)

**Push** (`S → C`):
```
cstr   channel_name
```
Sent right after auth and any time the server changes the client's
channel context.

### `opConnect` (6)

Reserved. The server does not currently send or process this opcode;
historical client code is commented out. SDKs MUST ignore inbound
frames with this opcode rather than erroring.

### `opVersion` (7)

**Request** (`C → S`):
```
cstr   version          (max 64 chars + null)
u8     platform         (optional; absent = pre-updater client)
                        0 = unknown, 1 = macos_arm64, 2 = windows_x64
```

**Reply** (`S → C`):
```
u8     ok               (1 = accepted, 0 = rejected)
if ok == 0 AND server has an update manifest matching the client's platform:
  u16    url_len        (LE)
  bytes  url            (url_len bytes, max 200)
  bytes  sha256         (32 bytes, hex of expected installer SHA-256)
```
Pre-updater servers send only the `ok` byte. Clients MUST detect
"reject + update info" by checking for at least `2 + 32` bytes
remaining after `ok`.

### `opUserInfo` (8)

**Request** (`C → S`):
```
u32    account_id
```
Account IDs in the top 24 (`>= 0xFFFFFFE7`) are bots; the server
does not respond, the client fills them locally.

**Reply** (`S → C`):
```
u32    account_id
struct[5] agency:                     (5 agencies × 13 bytes = 65 bytes)
   u16  wins
   u16  losses
   u16  xp_to_next_level
   u8   level
   u8   endurance
   u8   shield
   u8   jetpack
   u8   tech_slots
   u8   hacking
   u8   contacts
lenstr name                           (≤ 16 bytes; client only reads first 16)
```

### `opPing` (9)

**Push** (`S → C`):
```
(no payload)
```
Sent every 10 s.

**Ack** (`C → S`):
```
u8   value      (1; ignored by server)
```
The server only cares that *some* frame arrived inside the 30 s
window — any read resets the deadline.

### `opUpgradeStat` (10)

**Request** (`C → S`):
```
u8     agency_idx     (0–4)
u8     stat_id        (1=endurance 2=shield 3=jetpack 4=tech_slots 5=hacking 6=contacts)
```

**Reply** (`S → C`): empty payload (just `[opUpgradeStat]`); the
client re-fetches its own user info to see the new value.

### `opRegisterStats` (11)

**Request** (`C → S`), end-of-match stats from the AUTHORITY peer:
```
u32    game_id
u8     team_number
u32    account_id
u8     stats_agency
u8     won            (0 or 1)
u32    xp
struct MatchStats:    (44 × u32 LE = 176 bytes, in declaration order)
   weapons[4]:
     u32 fires
     u32 hits
     u32 player_kills
   u32 civilians_killed
   u32 guards_killed
   u32 robots_killed
   u32 defense_killed
   u32 secrets_picked_up
   u32 secrets_returned
   u32 secrets_stolen
   u32 secrets_dropped
   u32 powerups_picked_up
   u32 deaths
   u32 kills
   u32 suicides
   u32 poisons
   u32 tracts_planted
   u32 grenades_thrown
   u32 neutrons_thrown
   u32 emps_thrown
   u32 shaped_thrown
   u32 plasmas_thrown
   u32 flares_thrown
   u32 poison_flares_thrown
   u32 health_packs_used
   u32 fixed_cannons_placed
   u32 fixed_cannons_destroyed
   u32 dets_planted
   u32 cameras_planted
   u32 viruses_used
   u32 files_hacked
   u32 files_returned
   u32 credits_earned
   u32 credits_spent
   u32 heals_done
```
No reply.

### `opPresence` (12)

**Push** (`S → C`):
```
u8     action         (0 = add/upsert, 1 = remove)
u32    account_id
u32    game_id        (0 = main lobby, else game id)
u8     status         (0 = lobby, 1 = pregame, 2 = playing)
lenstr name           (≤ 16 bytes)
```

### `opSetGame` (13)

**Request** (`C → S`):
```
u32    game_id        (0 = back to main lobby)
u8     status         (0 = lobby, 1 = pregame, 2 = playing)
```
No reply.

## `LobbyGame` wire layout

Used inside `opNewGame` pushes. 16-byte hash is raw SHA-1 of the map
file.

```
u32    id
u32    account_id     (host)
lenstr name           (≤ 63 chars)
lenstr password       (≤ 63 chars; empty = public)
lenstr hostname       ("ip,port" string the client uses to peer-connect)
lenstr map_name       (≤ 63 chars)
bytes  map_hash       (20 bytes, SHA-1)
u8     players
u8     state          (0 = lobby, 1 = pregame, 2 = playing)
u8     security_level (0=NONE 1=LOW 2=MEDIUM 3=HIGH)
u8     min_level
u8     max_level
u8     max_players
u8     max_teams
u8     extra
u16    port
```

## UDP heartbeat (server-side, dedicated → lobby)

Not an SDK concern (only the dedicated game binary speaks this), but
documented here for completeness. Same lobby port, but UDP:

```
u8     0x00           (constant, "heartbeat" tag)
u32    game_id
u16    port
u8     state
```

## Limits & constants

| Constant            | Value | Source                                    |
|---------------------|-------|-------------------------------------------|
| max frame payload   | 255   | `services/lobby/protocol.go:26`           |
| max username        | 16    | `clients/silencer/src/lobby.h` (`maxusername`) |
| max channel name    | 32    | `clients/silencer/src/lobby.cpp` (`SendChat` guard) |
| max update url      | 200   | `services/lobby/protocol.go:27`           |
| server read timeout | 30 s  | `services/lobby/client.go:52`             |
| server ping period  | 10 s  | `services/lobby/client.go:70`             |
| reference client timeout | 20 s | `clients/silencer/src/lobby.cpp:120`  |
