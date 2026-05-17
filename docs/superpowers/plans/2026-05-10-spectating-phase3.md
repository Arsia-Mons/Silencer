# Spectating Phase 3 — Joining as Spectator

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let any logged-in lobby user click **Spectate** on a running, spectatable game and start receiving full-world snapshots. Mid-match join is first-class.

**Architecture:** Thin observer overlay on PR #152's rejoin scaffolding. One bit on `MSG_CONNECT`, one `bool observer` on `Peer`, a third admit branch on the AUTHORITY side (after rejoin, before new-player), and a `Game::SpectateGame` client entry that parallels `JoinGame`. Observers never enter `inputqueue`, never park on disconnect, never appear in `ingameusers`, and have any `to=1` chat coerced to `to=0`.

**Tech Stack:** C++14 / SDL3 client; same binary in dedicated mode.

**Spec:** [`docs/superpowers/specs/2026-05-10-spectating-phase3-design.md`](../specs/2026-05-10-spectating-phase3-design.md)

**Path note:** The spec was written against `clients/silencer/src/world.cpp` and `clients/silencer/src/world.h`. Those files now live at `clients/silencer/src/world/world.cpp` and `clients/silencer/src/world/world.h`. The `Peer` struct lives at `clients/silencer/src/net/peer.{h,cpp}` (the spec referenced it as part of `world.h`). The `Connect` function lives in `world/world.cpp` (spec said `world.cpp:1675`; actual location is around `world/world.cpp:1671`).

---

## Task 1: Add `observer` to Peer struct + wire-serialize it

Adds the flag mirroring `disconnected`. Serializing it on the peer-list wire lets every peer know who is an observer, which downstream rendering/UI can use to skip observers from any in-game lists. No behavior change yet.

**Files:**
- Modify: `clients/silencer/src/net/peer.h:24` (add field after `disconnected`)
- Modify: `clients/silencer/src/net/peer.cpp:20` (default in ctor)
- Modify: `clients/silencer/src/net/peer.cpp:24` (serialize)

- [ ] **Step 1: Add `observer` field after `disconnected` in `Peer`**

In `clients/silencer/src/net/peer.h`, after line 24 (`bool disconnected;`):

```cpp
	bool disconnected;
	bool observer;
```

- [ ] **Step 2: Default `observer` to false in the constructor**

In `clients/silencer/src/net/peer.cpp`, after the `disconnected = false;` line at the end of the constructor:

```cpp
	disconnected = false;
	observer = false;
```

- [ ] **Step 3: Serialize `observer` in `Peer::Serialize`**

In `clients/silencer/src/net/peer.cpp`, add a line in `Peer::Serialize` after `data.Serialize(write, accountid);`:

```cpp
	data.Serialize(write, accountid);
	data.Serialize(write, observer);
```

- [ ] **Step 4: Build (Windows fast path) and confirm it compiles cleanly**

Run:

```
cmake --preset win-ninja-unity -S clients/silencer
cmake --build clients/silencer/build-unity
```

Expected: clean build. Existing `MSG_PEERLIST` consumers don't read the new bit yet but the wire size grew by 1 bit per peer; same-version peers tolerate this because both sides serialize it in lockstep.

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/net/peer.h clients/silencer/src/net/peer.cpp
git commit -m "feat(spectating): add Peer::observer flag and serialize on peer-list"
```

---

## Task 2: Wire `observer` bit through `MSG_CONNECT` (outbound + inbound)

Add the trailing 1-bit field to the connect payload, plumbed through `World::Connect`. Default callers (existing `JoinGame`) pass `false`; the new `SpectateGame` (Task 6) will pass `true`.

**Files:**
- Modify: `clients/silencer/src/world/world.h` (signature of `Connect`)
- Modify: `clients/silencer/src/world/world.cpp:1671` (outbound `Connect`)
- Modify: `clients/silencer/src/world/world.cpp:277` (inbound parse only — admit logic stays default-reject until Task 3)

- [ ] **Step 1: Add `observer` parameter to `World::Connect` declaration**

Find the `Connect(...)` declaration in `clients/silencer/src/world/world.h`. Currently:

```cpp
	void Connect(Uint8 agency, Uint32 accountid, const char * password);
```

Change to:

```cpp
	void Connect(Uint8 agency, Uint32 accountid, const char * password, bool observer = false);
```

- [ ] **Step 2: Update `World::Connect` body to serialize the trailing bit**

In `clients/silencer/src/world/world.cpp`, change the signature and append a `PutBit` after the password loop:

```cpp
void World::Connect(Uint8 agency, Uint32 accountid, const char * password, bool observer){
	AllocateMapData(65535);
	sockaddr_in addr;
	addr.sin_addr.s_addr = htonl(GetAuthorityPeer()->ip);
	SwitchToMode(REPLICA);
	state = CONNECTING;
	messagetype = 0;
	authoritypeer = 0;
	GetAuthorityPeer()->lastpacket = SDL_GetTicks();
	Serializer data;
	Uint8 code = MSG_CONNECT;
	data.Put(code);
	data.Put(agency);
	data.Put(accountid);
	Uint8 passwordsize = password ? strlen(password) : 0;
	data.Put(passwordsize);
	for(int i = 0; i < passwordsize; i++){
		data.Put(password[i]);
	}
	data.PutBit(observer);
	SendPacket(GetAuthorityPeer(), data.data, data.BitsToBytes(data.offset));
}
```

- [ ] **Step 3: Parse the trailing bit in the AUTHORITY accept block**

In `clients/silencer/src/world/world.cpp` inside `case MSG_CONNECT:` (around line 277), read the bit right after the password loop. The new local `bool observerRequest` will be wired into admit logic in Task 3.

Find:

```cpp
					Uint8 passwordsize;
					data.Get(passwordsize);
					char temp[256];
					memset(temp, 0, sizeof(temp));
					for(int i = 0; i < passwordsize; i++){
						data.Get(temp[i]);
					}
```

Add immediately after the loop:

```cpp
					for(int i = 0; i < passwordsize; i++){
						data.Get(temp[i]);
					}
					bool observerRequest = data.GetBit();
```

- [ ] **Step 4: Build and confirm it compiles**

```
cmake --build clients/silencer/build-unity
```

Expected: clean build. `observerRequest` is unused (warning suppressed in this codebase, or explicitly `(void)observerRequest;` if a build flag flags it — only add the cast if the compiler complains).

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/world/world.h clients/silencer/src/world/world.cpp
git commit -m "feat(spectating): add observer bit to MSG_CONNECT wire payload"
```

---

## Task 3: Admit observer connections (third branch in AUTHORITY accept block)

Add the third admit branch. The order is: (a) rejoiner first, (b) observer second, (c) new player third. Observer admit ignores `peercount >= maxplayers` (player-cap) but still respects the 25-slot pool ceiling via `AddPeer` returning null. Observer admit also requires `gameinfo.spectatable` (defense-in-depth — Phase 2 gates the button already).

**Files:**
- Modify: `clients/silencer/src/world/world.cpp` (inside `case MSG_CONNECT:`)

- [ ] **Step 1: Rework the canjoin gating and add the observer admit branch**

Locate the block from `if(dedicatedserver.active){` through `if(canjoin && gameplaystate == INGAME && !rejoinpeer){ canjoin = false; }`.

Currently:

```cpp
					if(dedicatedserver.active){
						if(dedicatedserver.IsBanned(accountid)){
							canjoin = false;
						}
						if(!rejoinpeer && peercount >= gameinfo.maxplayers){
							canjoin = false;
						}
					}
					if(canjoin && gameplaystate == INGAME && !rejoinpeer){
						// mid-game connects are only for rejoiners
						canjoin = false;
					}
```

Replace with:

```cpp
					if(dedicatedserver.active){
						if(dedicatedserver.IsBanned(accountid)){
							canjoin = false;
						}
						if(!rejoinpeer && !observerRequest && peercount >= gameinfo.maxplayers){
							canjoin = false;
						}
					}
					if(canjoin && observerRequest && !gameinfo.spectatable){
						// defense-in-depth: button gating already prevents this in normal flow
						canjoin = false;
					}
					if(canjoin && gameplaystate == INGAME && !rejoinpeer && !observerRequest){
						// mid-game connects are only for rejoiners or observers
						canjoin = false;
					}
```

- [ ] **Step 2: Add the observer admit branch between rejoin and new-player**

The current structure is `if(canjoin && rejoinpeer)` → `else if(canjoin)` (new player) → `else` (reject).

Find:

```cpp
					}else if(canjoin){
						Peer * newpeer = AddPeer(host, port, agency, accountid);
						if(newpeer){
```

Change to (inserting an observer branch before the new-player branch):

```cpp
					}else if(canjoin && observerRequest){
						Peer * newpeer = AddPeer(host, port, agency, accountid);
						if(newpeer){
							newpeer->observer = true;
							response.PutBit(true);
							response.Put(newpeer->id);
							SendGameInfo(newpeer->id);
							SendPeerList();
						}else{
							response.PutBit(false);
						}
					}else if(canjoin){
						Peer * newpeer = AddPeer(host, port, agency, accountid);
						if(newpeer){
```

- [ ] **Step 3: Build and confirm clean**

```
cmake --build clients/silencer/build-unity
```

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/world/world.cpp
git commit -m "feat(spectating): admit observers in AUTHORITY MSG_CONNECT handler"
```

---

## Task 4: Gate inputs and route observer chat

Observers must never enter the input queue (they don't control any object). Their `to=1` (team) chat is coerced to `to=0` (all) since they aren't on a team — preserves Q5's "no special affordance" behavior without crashing the team-lookup path.

**Files:**
- Modify: `clients/silencer/src/world/world.cpp:386` (MSG_INPUT gate)
- Modify: `clients/silencer/src/world/world.cpp:464` (MSG_CHAT coercion)

- [ ] **Step 1: Gate MSG_INPUT on `!peer->observer`**

Find:

```cpp
			case MSG_INPUT:{ // client sending input
				if(peer && gameplaystate == INGAME){
```

Change to:

```cpp
			case MSG_INPUT:{ // client sending input
				if(peer && !peer->observer && gameplaystate == INGAME){
```

- [ ] **Step 2: Coerce observer chat `to=1` → `to=0`**

Inside `case MSG_CHAT:`, find:

```cpp
				if(peer){
					Player * player = GetPeerPlayer(peer->id);
					Uint8 to;
					data.Get(to);
```

Insert a coercion immediately after reading `to`:

```cpp
				if(peer){
					Player * player = GetPeerPlayer(peer->id);
					Uint8 to;
					data.Get(to);
					if(peer->observer && to == 1){
						to = 0;
					}
```

- [ ] **Step 3: Build and confirm clean**

```
cmake --build clients/silencer/build-unity
```

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/world/world.cpp
git commit -m "feat(spectating): drop observer inputs and route observer chat to all"
```

---

## Task 5: Free observer slot immediately on disconnect

Observers should not park. The rejoin scaffold parks any non-bot in-game peer with an accountid; observers fit that pattern but should be freed immediately so the 25-slot pool is reclaimed.

**Files:**
- Modify: `clients/silencer/src/world/world.cpp:1263` (the `park` condition in `HandleDisconnect`)

- [ ] **Step 1: Exclude observers from the parking condition**

Find:

```cpp
	bool park = (!permanent && mode == AUTHORITY && gameplaystate == INGAME && peerlist[peerid] && peerlist[peerid]->accountid != 0 && !peerlist[peerid]->isbot);
```

Change to:

```cpp
	bool park = (!permanent && mode == AUTHORITY && gameplaystate == INGAME && peerlist[peerid] && peerlist[peerid]->accountid != 0 && !peerlist[peerid]->isbot && !peerlist[peerid]->observer);
```

- [ ] **Step 2: Build and confirm clean**

```
cmake --build clients/silencer/build-unity
```

- [ ] **Step 3: Commit**

```bash
git add clients/silencer/src/world/world.cpp
git commit -m "feat(spectating): free observer slot immediately on disconnect"
```

---

## Task 6: Add `Game::SpectateGame` and wire the lobby button

Replaces the Phase 2 stub. `SpectateGame` mirrors `JoinGame` (Phase 2's panel branch) but skips agency/team selection and passes `observer=true` to `World::Connect`. The default agency is still passed in the connect payload — the AUTHORITY side ignores it for observer admits since no team membership is established.

**Files:**
- Modify: `clients/silencer/src/game/game.h:97` (add `SpectateGame` declaration)
- Modify: `clients/silencer/src/game/ingame.cpp:259` (add `SpectateGame` body next to `JoinGame`)
- Modify: `clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp:348` (replace stub)

- [ ] **Step 1: Declare `SpectateGame` next to `JoinGame`**

In `clients/silencer/src/game/game.h`, after the `JoinGame` declaration at line 97:

```cpp
	void JoinGame(LobbyGame & lobbygame, char * password = 0);
	void SpectateGame(LobbyGame & lobbygame, char * password = 0);
```

- [ ] **Step 2: Implement `SpectateGame` next to `JoinGame`**

In `clients/silencer/src/game/ingame.cpp`, after `Game::JoinGame` (around line 269), add:

```cpp
void Game::SpectateGame(LobbyGame & lobbygame, char * password){
	strcpy(world.mapname, lobbygame.mapname);
	Peer * peer = world.GetAuthorityPeer();
	peer->ip = ntohl(inet_addr(lobbygame.hostname));
	peer->port = lobbygame.port;
	sharedstate = 0;
	world.mode = World::REPLICA;
	world.Connect(Config::GetInstance().defaultagency, world.lobby.accountid, password, true);
	joininggame = true;
}
```

- [ ] **Step 3: Replace the GSEL_BTN_SPECTATE stub**

In `clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp`, find the `GSEL_BTN_SPECTATE` case (around line 344). Currently:

```cpp
				case GSEL_BTN_SPECTATE:{
					button->clicked = false;
					// Phase 3 wires this to the dedicated-server spectator connect
					// path. For now Phase 2 only delivers the affordance.
					ctx.ShowMessage("Spectating coming soon");
				}break;
```

Replace with the real call. Mirror the `GSEL_BTN_JOIN` password-modal path so private games still prompt:

```cpp
				case GSEL_BTN_SPECTATE:{
					button->clicked = false;
					ctx.game.currentlobbygameid = lobbygame->id;
					if(strlen(lobbygame->password) > 0 && lobbygame->accountid != world.lobby.accountid){
						Uint32 gameId = lobbygame->id;
						ctx.PushScreen(std::make_unique<PasswordModal>(
							[&ctx, gameId](const char * password){
								LobbyGame * lg = ctx.world.lobby.GetGameById(gameId);
								if(lg){
									char buf[64];
									std::strncpy(buf, password ? password : "", sizeof(buf) - 1);
									buf[sizeof(buf) - 1] = '\0';
									ctx.game.SpectateGame(*lg, buf);
								}
							}));
					}else{
						ctx.game.SpectateGame(*lobbygame);
					}
				}break;
```

- [ ] **Step 4: Build and confirm clean**

```
cmake --build clients/silencer/build-unity
```

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/game/game.h clients/silencer/src/game/ingame.cpp clients/silencer/src/ui/screens/lobby/panels/game_select_panel.cpp
git commit -m "feat(spectating): wire Spectate button to SpectateGame entry"
```

---

## Task 7: Manual end-to-end smoke

Per the spec (Section 4) and user feedback "Smoke test before done", actually drive a 3-client smoke. CLI-agent automation is deferred (no `GameCreate` op exists). This is the primary verification path.

- [ ] **Step 1: Build fresh from the unity preset on Windows**

```
cmake --preset win-ninja-unity -S clients/silencer
cmake --build clients/silencer/build-unity
```

- [ ] **Step 2: Three-client smoke**

  1. Launch client A. Log in. Create a **spectatable** game.
  2. Launch client B. Log in. Click **Join** on the same lobby row. Both clients transition through INLOBBY → INGAME.
  3. Launch client C. Log in. The game-select panel row shows **Join** and **Spectate**; click **Spectate**.
  4. Verify on client C: world renders, snapshots arrive, player A is visible.
  5. Type a chat message from client C. Verify it appears on clients A and B with C's account name. Try `to=1`-equivalent (team chat key) from C and verify it still reaches A and B (server coerced `to=1` → `to=0`).
  6. Verify on clients A and B: spectator C is not in any team list / in-game scoreboard. (Spectator does not appear because they aren't on a team and have no Player object — confirm visually.)
  7. Disconnect client C. Verify dedicated-server peer count drops by one immediately; spectator slot is not parked.
  8. Win the game from A or B. Verify all three clients see the 3-second victory message, then return to lobby together.

- [ ] **Step 3: Non-spectatable rejection (defense-in-depth)**

  1. Restart from the lobby. Client A creates a **non-spectatable** game.
  2. Client B joins. Client C sees no **Spectate** button on the row (Phase 2 gating).
  3. Optional: temporarily flip the panel to force-render the Spectate button, click it on the non-spectatable game, and verify the dedicated server rejects with `accept=0` (client surfaces the existing "could not join" path).

- [ ] **Step 4: If anything fails, debug and re-run smoke**

No automated tests cover this flow yet. Treat the smoke as the primary verification gate.

- [ ] **Step 5: No commit; this task is verification only**

---

## Task 8: Update the progress tracker and push the PR

Per user feedback: "PR before in-session review — push and open PR first, then run subagents."

**Files:**
- Modify: `docs/plans/2026-05-09-spectating-progress.md` (check off Phase 3)

- [ ] **Step 1: Check off Phase 3 in the tracker**

Change line 23 from:

```
- [ ] **Phase 3 — Joining as spectator (any time).** Aligned with
```

to:

```
- [x] **Phase 3 — Joining as spectator (any time).** Done on branch.
```

Trim the body to match the Phase 2 voice (terse, past tense, references the design spec).

- [ ] **Step 2: Commit tracker update**

```bash
git add docs/plans/2026-05-09-spectating-progress.md
git commit -m "docs(spectating): phase 3 done — joining as spectator"
```

- [ ] **Step 3: Push branch and refresh PR #148**

```bash
git push
```

PR #148 (`hv/spectator`) already exists — no `gh pr create` needed. Update the PR description if it lists pending phases.

---

## Self-review notes

- **Spec coverage:** Tasks 1–6 cover Section 1's full "Where things change" table. The "Snapshot send: no change" row needs no task. The "Match end: no code change" row needs no task (verified by re-reading `tick_ingame.cpp:61` — `ingameusers` is populated only from team members, and observers join mid-INGAME and never join a team).
- **Edge cases (Section 3):**
  - Spectatable=false: covered in Task 3 (defense-in-depth).
  - 25-slot pool full: `AddPeer` returns null → `response.PutBit(false)` — covered in Task 3.
  - Password mismatch: shared canjoin path — covered.
  - Spectator timeout: standard peer-timeout drops to `HandleDisconnect` which checks `observer` first (Task 5).
  - Spectator sends MSG_INPUT: gated in Task 4.
  - Observer + parked-rejoiner same accountid: rejoin branch comes first in the AUTHORITY accept block, so the rejoiner wins. Confirmed by branch order in Task 3.
- **No placeholders:** All code blocks contain real code with real signatures. No TBDs.
- **Type consistency:** `bool observer` on Peer is used consistently. `World::Connect(..., bool observer = false)` matches across header and body. `Game::SpectateGame` signature mirrors `Game::JoinGame`.
