# Silencer Beta 0110 — Original Game Reference

Extracted from `sil0110.exe` (WinAce SFX → InstallShield CAB → `Silencer.exe`).
Build: **Open Beta Build 110**, May 21, 2000. Developer: Mind Control Software.

---

## Player Profile Fields

```
AGENT:    <callsign>
OF:       <agency>
LEVEL:    <int>
CREDITS:  <int>
MISSIONS: <int>
VICTORIES:<int>
FORFEITS: <int>
REPUTE:   <string>
```

---

## Repute Tiers (index 0–10)

| Index | Label |
|-------|-------|
| 0 | Just Kill Me |
| 1 | And you are? |
| 2 | Ignored |
| 3 | Still Ignored |
| 4 | Misunderstood |
| 5 | Noticed |
| 6 | Respected |
| 7 | Very Favorable |
| 8 | Well Respected |
| 9 | Feared |
| 10 | God-like |

---

## Agencies (AGENCY.I)

| ID | Name | Tech | Advantages |
|----|------|------|------------|
| 0 | **Noxis** | Health Pack (3) | Jump +5 (0,3), Endurance +3 (9,5) |
| 1 | **Lazarus** | Laz Tract (4) | Resurrection (5,6) |
| 2 | **Caliber** | Security Badge (5) | Contacts +3 (4,3) |
| 3 | **Static** | Virus (6) | Periodic enemy blips (7,6), Hacking +3 (3,3) |
| 4 | **Black Rose** | Poison (7), Poison Flare (15) | Stealth (6,6), Shield +2 (1,2) |

### Agency Lore

**Noxis** — Terraforming corp that supplies 70% of Mars breathable oxygen. Agents have superior physical abilities: higher jumps, more stamina, enhanced durability from bio-sporria training.

**Lazarus** — Mysterious organization resurfaces every century causing chaos. Agents have near-miraculous recovery from fatal wounds.

**Caliber** — Upper-class info brokers who pay well for even common files. Security access specialists.

**Static** — Anti-government agency of young talented hackers. Turbulent, reputation-driven. Technologically superior hacking and enemy detection.

**Black Rose** — Dark and misanthropic. Masters of poison and stealth. Work alone. Use Hollowhead injections to cause extreme pain.

---

## Technologies / Inventory (TECH.I)

| ID | Name | Type | Buy | Sell | Stack |
|----|------|------|-----|------|-------|
| 0 | Laser | weapon ammo | 100 | 300 | 1 |
| 1 | Rockets | weapon ammo | 100 | 400 | 1 |
| 2 | Flamer Ammo | weapon ammo | 200 | 500 | 1 |
| 3 | Health Pack | inventory (Noxis) | 150 | 200 | 1 |
| 4 | Lazarus Tract | inventory (Lazarus) | 250 | 1000 | 1 |
| 5 | Security Pass | inventory (Caliber) | 1000 | 1000 | 1 |
| 6 | Virus | inventory (Static) | 400 | 1000 | 1 |
| 7 | Poison | inventory (Black Rose) | 100 | 500 | 1 |
| 8 | Neutron Bomb | inventory | 4000 | 2000 | 8 |
| 9 | E.M.P. Bomb | inventory | 1000 | 1000 | 4 |
| 10 | Shaped Bomb | inventory | 100 | 200 | 1 |
| 11 | Plasma Bomb | inventory | 200 | 500 | 2 |
| 12 | Plasma Detonator | inventory | 150 | 500 | 2 |
| 13 | Fixed Cannon | inventory | 300 | 700 | 2 |
| 14 | Flare | inventory | 200 | 500 | 1 |
| 15 | Poison Flare | inventory (Black Rose) | 200 | 600 | 1 |
| 16 | Camera | inventory | 200 | 200 | 1 |
| 17 | Base Door | inventory | 300 | 600 | 1 |
| 18 | Base Defense | inventory | 100 | 500 | 1 |
| 19 | Insider Info | inventory | 500 | 500 | 1 |
| 20-22 | Give to (credit transfer) | inventory | 100 | 100 | 0 |

---

## Weapons (WEAPON.I)

| ID | Name | Damage | Ammo | Range | Notes |
|----|------|--------|------|-------|-------|
| 1 | Blaster | 40 | unlimited | 9 | Main weapon, unlimited ammo from suit |
| 2 | Laser | 60 | 30 shots | 12 | Removes shields, no flesh damage |
| 3 | Rocket Gun | 75 | 30 shots | 12 | Long range, high yield |
| 4 | Flamer | 3/tick | 75 shots | 3 | Ignores shields, limited range |
| 6 | Force Blade | — | melee | 0 | Melee weapon |
| 7 | Poison | — | — | — | Status effect delivery |

Grenades/bombs as projectile weapons: Shaped (10), Plasma (11), EMP (9), Neutron (12), Robot (13), Cannon (14), Detonator (15), Civilian (16), Flare (18), Poison Flare (19), Robot Electric (20).

---

## Game Lobby / Conflict Settings

| Field | Description |
|-------|-------------|
| Host | Server hostname |
| Level | Map name |
| Security | Password protected |
| Max Players | Player cap |
| Max Teams | Team cap |
| Min Lvl | Minimum agent level to join |
| Max Lvl | Maximum agent level to join |
| Magistrate | Magistrate NPC active (yes/no) |
| Fatigue | Fatigue system active (yes/no) |

---

## Error Codes (join/session)

```
ERR_ok
ERR_wrong_session
ERR_no_player_record
ERR_server_full
ERR_game_full
ERR_no_teams
ERR_kicked
ERR_banned
ERR_invalid_character
ERR_bad_checksums
ERR_failed_level_requirement
ERR_bad_password
ERR_lost_communications
ERR_missing_keep_alive
ERR_bad_packet_format
ERR_already_processed_create
ERR_no_free_games
ERR_activation_failed
ERR_invalid_tech
ERR_server_disconnected
ERR_old_version
ERR_server_not_responding
```

---

## Network Messages (MSG_ protocol)

```
MSG_ack, MSG_failure, MSG_query, MSG_query_reply
MSG_create, MSG_create_reply
MSG_ping, MSG_pong, MSG_keep_alive
MSG_chat, MSG_exit
MSG_first_contact
MSG_join, MSG_join_reply
MSG_kick
MSG_game_status, MSG_flags
MSG_req_level, MSG_level
MSG_req_team_change, MSG_team_change, MSG_lock_team
MSG_tech_change
MSG_fragment_header
MSG_ready, MSG_synch
MSG_close_game, MSG_begin_game
MSG_checksum
MSG_player_bundle, MSG_server_packet, MSG_req_server_packet
MSG_end_game, MSG_req_synchronization
```

---

## Chat Commands (in-game / lobby)

| Command | Context | Description |
|---------|---------|-------------|
| `/w [user] [msg]` or `/whisper` | Lobby | Whisper to user (persists until null whisper) |
| `/i [user]` or `/ignore` | Lobby | Ignore user |
| `/l [user]` or `/listen` | Lobby | Stop ignoring user |
| `/notify` | Lobby | Toggle join/exit system messages |
| `/list` | Lobby | Sorted user list |
| `/me [msg]` or `/emote` | Both | Emote format |
| `/stats [agent]` | Game lobby | Show agent stats in chat |

---

## Keys (default)

| Key | Alt | Action |
|-----|-----|--------|
| Numpad 8 | Joy up | Climb ladder / Aim up |
| Numpad 2 | Joy down | Climb down / Duck |
| Numpad 4 | Joy left | Run left |
| Numpad 6 | Joy right | Run right |
| Numpad 7/9/1/3 | Joy diagonals | Diagonal aim |
| Right SHIFT | — | Jetpack (hold) |
| Right CTRL | Joy Button 1 | Fire weapon |
| Space | Joy Button 3 | Activate / Hack / Door |
| Enter | Enter | Use inventory |
| Numpad 5 | Joy Button 6 | Disguise as civilian |
| TAB | — | Chat |
| Numpad 0 | Joy Button 7 | Next weapon |
| ] | Joy Button 8 | Next inventory item |
| < / > | — | Previous/next camera/detonator |
| M | — | Detonate |
| J/K/L/; | Joy 11–14 | Signals (thumbs up / follow / point / halt) |

---

## Missions (MISSION.I)

| ID | Name | Level |
|----|------|-------|
| 1 | I - Player Movement (tutorial) | 20 |
| 2 | II - Agent Base (tutorial) | 20 |
| 3 | III - Secret Recovery (tutorial) | 20 |
| 4 | Area Star | 17 |
| 5 | Area Pyramid | 21 |
| 6 | Area Towers | 24 |
| 7 | Area Bomb Pit | 10 |
| 8 | Area Rocket Alley | 2 |

---

## WON.NET Server Infrastructure (defunct)

```
AuthServer
TitanRoutingServer
TitanFactoryServer
GameServer
SilencerValidVersions

Routing servers:
  silencer.west.won.net
  silencer.east.won.net
  silencer.central.won.net

Update server: silencer.update.won.net
```

WON API calls used: `WONAuthGetNicknameA`, `WONAuthLoginNewAccountA`, `WONProfileSet`, `WONProfileCreate`, `FO_GetPassword`, `FO_SetPassword`, `FO_ChangeUserPassword`.

---

## Gender Options

- Female
- Male

---

## Build History (from Known Issues)

| Build | Date | Key Changes |
|-------|------|-------------|
| 0110 | May 2000 | Lobby chat commands, Change Agent fix, secret beaming fix |
| 0109 | — | Multiple personality kick bug fix, agent selection in main lobby |
| 0108 | — | 3 new levels, forfeit on willful drop, 2-team minimum |
| 0107 | — | Net code packet discard fix |
| 0106 | — | New server architecture, per-player latency |
| 0105 | — | Checksum bug fix, in-game chat responsiveness |
| 0104 | — | Focus loss fix, in-game chat (TAB key) |

---

## Credits

**Mind Control Software**
- Producer: Andrew Leker
- Design Coordinator: William G. Dunn
- Lead Programmer: Jonathan Stone
- Net & Applications Programming: Mark Roberts, Andrew Leker
- WON.NET Client Programming: Mark Roberts
- 3D Art: Tynan Wales, James Graham
- Tile Art: Gene Hirsh
- Level Design: Andrew Leker, Darren Koepp, Mark Roberts, William G. Dunn, Jeff Moser, Damon Berry
- Sound: Jonathan Hoffberg, Jonathan Stone, Mark Roberts, Adam Levinson
- Music: Jonathan Stone
- Agency Scripting: Peter Kelly
- Player's Guide: Noel Wade
