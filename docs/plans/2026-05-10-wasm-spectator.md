# WASM browser spectator

**Status:** Stage 1 done (renders main menu in browser). Render-target
section revised below — SDL3 has no WebGPU backend upstream, so the
browser build uses SDL_Renderer over WebGL2 instead.
**Date:** 2026-05-10

## Goal

Ship a WebAssembly build of the Silencer client that lets anyone with a
browser watch a live multiplayer match. The widget is embedded in the
main website (`web/website/`). Its idle view renders the existing
lobby panel UI; when an in-progress spectatable game is available it
auto-transitions to spectating that game. No login required.

This is **spectator-only forever** by deliberate design choice. Browsers
participating as real players (sending input, occupying slots) is out
of scope and would require a separate, larger project that replaces
the native UDP P2P mesh with a unified WebRTC mesh. See "Alternative
paths considered" below.

## Confirmed design

- **Networking topology — server-side relay.** A new C++ binary mode
  joins each spectatable game as a passive peer using the existing UDP
  code, then fans snapshot bytes out over WebSocket to N browser
  clients. No changes to the on-wire peer protocol. No WebRTC anywhere.
- **Spectator inputs are local-only.** Tab/WASD/follow-cycle/etc. drive
  camera and follow-target state inside the browser; nothing flows back
  to the relay. WebSocket from relay → browser stays read-only.
- **Render target — SDL_Renderer over WebGL2** (revised 2026-05-10).
  The original plan was SDL3 GPU on WebGPU, but SDL3 3.4.x upstream
  has no `gpu/webgpu/` backend (only D3D12, Metal, Vulkan); the
  closed PR #12046 (klukaszek/SDL) targets older SDL3 3.2.1, doesn't
  match our 3.4 baseline, and was never merged. We pivot to the
  SDL_Renderer 2D API for the browser only — it picks GLES2 (WebGL2)
  in the browser and has been stable for years. The native build
  keeps SDL_GPU on D3D12/Metal/Vulkan; SPIRV codegen still lands
  (committed) so the future SDL3-WebGPU path stays open.
- **Browser render uses CPU palette remap** (revised). Indexed
  framebuffer + per-frame palette remap to RGBA in C++ → SDL_Texture
  streaming upload → SDL_RenderTexture stretch to canvas. At 640×480
  the per-frame cost is ~1 ms — negligible. The native fragment-shader
  remap stays for SDL_GPU paths. Lights and particles will need a CPU
  port for the WebGL2 path when Stage 5+ needs them (deferred).
- **Audio — SDL3_mixer's emscripten port → WebAudio.** Both ADPCM
  (`sound.bin`) and MP3 (`CLOSER2.mp3`) work. Standard "click to enable
  sound" first-gesture UX for browser autoplay policy.
- **Asset delivery — hybrid.** Preload palette, fonts, UI sprites, and
  common gameplay sprites via Emscripten MEMFS bundle. Async-fetch
  maps, actordefs, and per-mission sprites via `emscripten_fetch` —
  the actordef and map paths already do this on native.
- **Hosting — embedded in the main public website** (`web/website/`).
  Both `/spectate` (lobby browser) and `/spectate/<gameid>` (deep link)
  routes are supported.
- **Auth — anonymous v1.** Anyone with the URL can watch. WebSocket
  endpoints accept any client. Layer gating on later without breaking
  the browser surface if needed.
- **Browser baseline.** WebGPU-capable browsers only: Chrome/Edge
  stable, Firefox stable, Safari 26+. Users on older browsers see a
  graceful "browser not supported" message rather than a broken page.
  WebGL2 fallback is out of scope for v1 — adding it would mean a
  second rendering backend without compute-shader support.

## Architecture

Three new components, two existing components changed, no existing
component's runtime behavior changes.

**New components:**

1. **WASM widget** — the existing C++ client built with Emscripten.
   Networking is replaced (raw sockets → WebSocket), input is
   restricted to the spectator camera controls, shaders are served as
   SPIRV. Rendered into a `<canvas>` on the marketing site.
2. **`silencer --relay`** — a new mode of the existing C++ client
   binary. Headless (no SDL video/audio/input). Joins a single game as
   a passive spectator peer using the existing UDP and lobby code in
   `clients/silencer/src/world/world.cpp` and
   `clients/silencer/src/net/lobby.cpp`. Embeds a small WebSocket
   server library; fans incoming snapshot bytes out to all connected
   WS clients without interpreting them. One process per spectatable
   game.
3. **Lobby WebSocket facade** — a new WebSocket endpoint on the Go
   lobby (`services/lobby/`) that re-emits the lobby protocol over
   WebSocket for browsers (which cannot speak the lobby's raw TCP):
   game list with `spectatable` + `can_rejoin` bits, push updates, a
   `spectate <gameid>` command returning the relay URL.

**Existing components changed:**

- **`services/lobby/`** — gains the WebSocket facade above; spawns one
  relay process per spectatable game alongside the dedicated server
  (mirrors the existing pattern in `proc.go` that spawns dedicated
  servers per `MSG_NEWGAME`). Native TCP lobby path is unchanged.
- **`clients/silencer/`** — gains an Emscripten build target, the
  `--relay` binary mode, an HLSL→SPIRV shader codegen path in
  `cmake/CompileShaders.cmake`, and a thin transport abstraction in
  `world.cpp` and `lobby.cpp` so the same code can sit on raw UDP/TCP
  (native) or WebSocket (Emscripten). Native binary runtime behavior
  is unchanged.

**Existing components NOT changed:**

- Native peer protocol on the wire (UDP, `MSG_*` constants, snapshot
  format, `Serializer` framing). Relay reuses it as-is by being a real
  peer.
- Native client build outputs (Windows/macOS/Linux binaries).
- Dedicated server (`-s` mode) behavior.

## Data flow — happy path

1. Visitor loads the website page that embeds the widget. The page
   serves the WASM binary, the preload `.data` bundle (palette, fonts,
   UI, common sprites), and a small JS bootloader.
2. WASM `main()` opens a WebSocket to the lobby facade.
3. Lobby facade pushes initial lobby state: running games with their
   `spectatable` and `can_rejoin` bits, and continues pushing deltas as
   they happen.
4. Widget renders the existing lobby panel UI from that state.
5. **Auto-spectate:** if any INGAME spectatable game exists, after a
   short idle delay the widget picks one (e.g. highest player count)
   and asks the lobby facade to spectate it. The user can override by
   clicking another row.
6. Lobby facade ensures a relay exists for that game (spawns one if
   missing), returns the relay's WebSocket URL.
7. Widget opens a second WebSocket to the relay. Relay sends a fresh
   keyframe + ongoing snapshot stream.
8. Widget decodes snapshots via the existing `Serializer` and runs the
   normal world tick + render loop. Camera follows a default target;
   keyboard input drives camera/follow locally.
9. As snapshots reference unfamiliar maps or actordefs, the widget
   `emscripten_fetch`es them from the admin API on demand — same
   pattern the native client uses.

**Match end:** relay forwards match-end signal → widget transitions
back to the lobby view → lobby may auto-pick a new live game.

**Mid-match join:** identical to steps 6-9. Relay sends a fresh
keyframe so the late joiner has full state; this piggybacks on the
`SendGameInfo` + `SendPeerList` resync handshake that PR #148 Phase 3
adds for native spectator joins.

**Multiple spectators per relay:** the relay broadcasts the same
snapshot stream to all WS clients. Per-spectator state (camera, follow
target) lives entirely in the browser.

**Backpressure:** if a WS client falls behind, the relay drops frames
for that client only and sends the next keyframe on resumption.

## Stages

Seven stages. Stages 1, 2, and 3 are independent and can run in
parallel. Stage 4 needs all three. Stages 5 and 6 follow Stage 4.
Stage 7 closes out the project.

### Stage 1 — Emscripten foundation ✅ DONE

Add an Emscripten build path to the client. Produce a WASM bundle
that loads in a browser and renders the main menu. No networking, no
gameplay.

**Outcome (2026-05-10):** Menu renders in a browser via SDL_Renderer
+ WebGL2. Three commits landed on `hv/wasm`:

- `dc89f45` — HLSL→SPIRV codegen + ShaderBundle SPIRV field
  (groundwork; not consumed in Stage 1 due to the pivot, but
  preserved for a future SDL3-WebGPU backend).
- `9365baa` — Emscripten build target: SDL3+zlib emcc ports,
  FetchContent SDL_mixer 3.2.0, --preload-file glue, main-loop
  adapter, curl/minizip-using files stubbed.
- *(this commit)* — Pivot to SDL_Renderer. New `SDLRendererBackend`
  (CPU palette remap + SDL_Texture streaming + SDL_RenderTexture).
  `Game::SetupRenderDevice` picks it when `__EMSCRIPTEN__`. 8MB
  emscripten stack (default 64KB overflows on `LoadSprites`'s
  88KB header buffer). `Game::LoadProgressCallback` no-ops on
  browser (calling SDL_RenderPresent during loading aborts because
  the main loop isn't established yet).

**De-risked finding:** SDL3 GPU on WebGPU doesn't ship upstream. The
SDL3 3.4 emcc port has `gpu/*.c` (the dispatcher) but no backend
under `gpu/{vulkan,d3d12,metal}`. We pivoted to the well-supported
SDL_Renderer + WebGL2 path. If/when an SDL3 WebGPU backend lands,
the committed SPIRV codegen and `ShaderBundle::spirv` plumbing slot
straight in.

### Stage 2 — Relay binary mode 🟡 SCAFFOLD LANDED (ingest unblocked by #156)

New `silencer --relay <lobbyaddr> <lobbyport> <gameid> [--ws-port=N]`
mode lands as commit `92b4490`. Scaffold includes:

- Binary mode parser in `main.cpp` — gated on POSIX && !EMSCRIPTEN
  (browsers can't host a relay).
- Hand-rolled HTTP→WebSocket upgrade + binary-frame writer in
  `src/net/relay.{h,cpp}` (~270 LOC, RFC 6455 subset, no third-party
  dep — mirrors the Go `services/lobby/wsutil.go` we wrote for
  Stage 3). The WS-library question in the original design is now
  answered by precedent: hand-roll, same shape both sides.
- Per-client outbox with 8 MB cap. A slow spectator drops frames
  rather than stall the broadcast (the design's stated
  backpressure policy).

**What's stubbed:** the relay currently emits a synthetic 4-byte
`BEAT<seq u32>` keepalive every 250 ms so end-to-end WS wiring can
be smoke-tested. The real game-side ingest — joining a peer mesh
as a passive spectator, decoding snapshots, forwarding them — is
the next concrete work item. With #156 merged into main, the
spectator-peer handshake (`Peer::observer` bit, `MSG_CONNECT`
observer admit, `viewedpeerid`, observer slot lifecycle) is
available natively. The relay's Run loop should:

1. Open lobby TCP + UDP sockets via existing `Lobby` + `World`
   code paths (the relay binary is POSIX-only — no SDL).
2. `LOGIN` → `SpectateGame <gameid>` against the lobby.
3. Send `MSG_CONNECT` with the observer bit set; AUTHORITY admits.
4. Read snapshot bytes off the peer mesh and forward verbatim to
   all connected WS clients via the existing 8MB outbox.

**Verify (today):**
  silencer --relay 127.0.0.1 25170 99 --ws-port=25174
  # bun WS client at ws://localhost:25174/ receives BEAT @ 4 Hz

### Stage 3 — Lobby WebSocket facade ✅ DONE

Commit `56b3319`. New WS endpoint on `services/lobby/` (separate port,
default `:15173`) speaks JSON envelopes the browser can consume:

- `hello` with MOTD + initial game-list snapshot
- `newgame` / `delgame` deltas pushed in real time via a new
  `Hub.Subscriber` interface
- `spectate <gameid>` command → `spectate_url` (the relay base from
  `-ws-relay-base`) or `NO_RELAY` / `NOT_SPECTATABLE` / `NO_SUCH_GAME`
  error envelope

Hand-rolled RFC 6455 in `wsutil.go`, no new Go dependency. Native
lobby TCP path is untouched.

**Verify (today):** smoke test in commit message.

### Stage 4 — WASM networking substitution 🟢 READY (unblocked by #156)

Abstract the raw socket calls in `world.cpp` and `lobby.cpp` behind a
thin transport interface. Native builds keep `sendto`/`recvfrom` and
raw TCP; Emscripten builds use WebSocket to the relay and to the lobby
facade respectively. Wire the WASM widget through to render gameplay
from relay snapshots.

With #156 merged, the spectator-peer contract is fixed. The world.cpp
side knows what frames the relay will emit (Stage 2's real ingest
defines the wire). Do `lobby.cpp` and `world.cpp` together in one
sweep — the refactor cost is similar and a partial split leaves the
code in two states.

**Verify:** native dedicated server → relay → WASM widget → renders
the live game. End-to-end smoke. Native E2E tests stay green
(non-negotiable: the transport refactor must not regress native).

### Stage 5 — Lobby UI in browser + auto-spectate 🟡 GATED on Stage 4

WASM widget renders the existing lobby panel UI from the WS facade's
state stream. Auto-spectate logic: pick a live spectatable game after
idle delay; transition back to lobby view on match end. Finalize the
asset pipeline (preload bundle contents, async-fetch paths).

**Alternative path that bypasses the transport refactor:** a JS-side
WebSocket bridge could populate `Lobby::games` from the facade's JSON
envelopes via Emscripten `EM_JS` / `cwrap`. With Stage 4 now ready,
this is mostly historical — fold it in only if Stage 4 hits an
unexpected snag and a near-term demo matters. See "Stage 5
alternative path" at the bottom of this doc.

### Stage 6 — Spectator inputs + camera 🟢 READY (unblocked by #156)

SDL3 emscripten keyboard/mouse input wired through. Camera follow,
cycle (Tab / Shift-Tab), free-cam (WASD/arrows), hold-for-names.
Inherits the Phase 4 controls that landed in #156 (`viewedpeerid`,
free-cam, ESC out, rebound Move L/R cycle + Activate names).

### Stage 7 — Production deploy + website embed 🚫 NOT STARTED

Containerize the relay binary. Lobby spawns relays in production the
same way it spawns dedicated servers (see `services/lobby/proc.go` —
add a `RelayStart` path mirroring `Start`). Static-host the WASM
bundle and preload assets (CDN). Embed the widget in `web/website/`.

The lobby's relay-spawn entrypoint should be triggered by the
WebSocket facade's `spectate <gameid>` handler: ensure a relay is
running for that gameid (start one if not), return its URL. Today
the handler returns the configured `-ws-relay-base` plus
`?gameid=N` as a hint — once spawn-on-demand is wired, it should
also kick off the relay process.

## Stage 5 alternative path (JS bridge)

If we want a browser preview of the lobby browser before Stage 4
unblocks, a small JS module can bridge the existing JSON facade to
the C++ `Lobby` state. Sketch:

1. Custom HTML shell loads `spectator-bridge.js` before `silencer.js`.
2. Bridge opens `ws://<lobbyhost>:15173/spectate`, parses
   envelopes, and calls `Module.ccall("LobbyBrowserAddGame", ...)`
   per `newgame` envelope. New `extern "C" EMSCRIPTEN_KEEPALIVE`
   functions in `lobby.cpp` push games into `Lobby::games` directly
   (skipping the auth state machine).
3. Force `Lobby::state = AUTHENTICATED` and `Lobby::accountid = 1`
   so the lobby browser UI renders without a login flow.
4. Disable the LobbyConnect screen on emscripten — auto-advance
   from main menu → lobby browser.

This sidesteps the lobby.cpp transport refactor entirely. The cost
is a parallel ingress path the C++ code doesn't know about; later
Stage 4 work would supersede it. Worth it if a near-term demo
matters; skip if Stage 4 is imminent.

## Risks and de-risking moves

1. **SDL3 GPU emscripten / WebGPU compatibility with our shader
   pipeline.** De-risked by Stage 1 going first — and the answer was
   *it doesn't work yet*: SDL3 upstream has no WebGPU backend. We
   pivoted to SDL_Renderer + WebGL2 for the browser only; native
   keeps SDL_GPU. See Stage 1 outcome above.
2. **Embedded WebSocket library choice.** Decided early in Stage 2;
   mitigated by sticking to well-known libraries with cross-platform
   build support.
3. **Transport-interface refactor of `world.cpp` regressing native
   networking.** Native E2E smoke and the existing peer-protocol
   golden vectors guard against this. Stage 4 acceptance criterion:
   all existing native tests stay green.
4. **Preload bundle size.** Unknown until measured. Mitigation: be
   prepared to push more assets out to async-fetch if the preload
   bundle grows above ~10-20 MB.
5. **PR #156 spectator landed 2026-05-11.** Stages 2-finish, 4, and 6
   are now unblocked. Risk closed.

## Alternative paths considered

**Path B — unified WebRTC mesh.** Replace the native UDP P2P with
WebRTC DataChannels for every peer (native and browser). Lobby grows a
WebSocket signaling endpoint. Single protocol across all clients;
browser becomes a true first-class peer. **Rejected for now** because
the user goal is spectator-only and Path B's costs (libdatachannel dep
in all native builds, higher join latency from DTLS handshakes,
per-packet DTLS+SCTP overhead, encrypted traffic harder to debug, full
rewrite of `world.cpp` socket layer + lobby signaling) aren't
justified by the goal. Path A (this design) doesn't close the door on
Path B later — if browsers ever need to *play*, that's a separate
project; the relay built here remains useful in either future (e.g.
for low-bandwidth observers, recordings).

**Gateway translation (browser WebRTC ↔ native UDP).** Discarded; same
ops cost as Path A plus a WebRTC stack.

**Native WebRTC for the spectator only, hybrid mesh.** Discarded;
couples browser participation to authority migration and doesn't scale
past a handful of browser peers per game.

## Downstream features (explicitly later)

**Replay / recording.** Capturing the snapshot stream for later
playback is a planned downstream feature. The relay is the natural
place to capture (it has the byte stream in hand) but whether
recording piggybacks on the relay or is built as a separate component
is deferred — that decision belongs to the replay project, not this
one. Do not design for recording in any stage above; do not let
recording requirements influence relay design.

**Other deliberate non-goals for WASM v1:**

- Browser users *playing* the game. Path B territory.
- Spectator chat. Excluded by user.
- Per-spectator filtering at the relay (fog of war, etc.). All
  spectators get the same byte stream.
- Mobile-browser layout and touch UX tuning. The WebGPU+WASM build
  should run on modern mobile Chrome/Safari but layout is its own
  follow-up concern.
- Spectator authentication, rate limiting, abuse prevention. Anonymous
  v1; layer on later.
- Spectator count display ("12 watching"). Deferred.

## Dependencies on other in-flight work

- **#156 spectator (merged 2026-05-11)** — `docs/plans/2026-05-09-spectating.md`.
  Lands Phases 1-4: spectatable flag, can_rejoin bit, `Peer::observer`
  + `MSG_CONNECT` observer admit, `viewedpeerid`, spectator controls,
  ESC out, observer slot lifecycle. Unblocks WASM Stages 2-finish,
  4, and 6.
- **Monorepo restructure** — `docs/plans/2026-04-25-monorepo-restructure.md`.
  May shift directory paths for `services/lobby/`,
  `clients/silencer/`, and `web/website/`. This document is
  intentionally written at the level of architectural intent rather
  than file paths so it doesn't go stale during the restructure.

## Adjacent prior art

- Replay system (`clients/silencer/src/game/replay.{h,cpp}`) — closest
  existing UX for spectator-like input/render. The same input/render
  layer should serve both pre-recorded and live (spectator) streams.
- Dedicated server `-s` mode (`clients/silencer/src/net/dedicatedserver.cpp`,
  `services/lobby/proc.go`) — the relay reuses the exact same pattern:
  a headless C++ binary mode that joins the game using existing
  networking code, spawned by the lobby per-game.
- Admin live-sessions dashboard — list-only via Socket.IO + AMQP.
  Unrelated to in-game spectating; noted because someone may confuse
  the two.

## Where to pick up

Stages 1, 3 are done. Stage 2 scaffold is in (BEAT keepalive
placeholder). #156 (spectator phases 1-4) is now in main, so the
next concrete work is **Stage 2 finish**: swap the BEAT generator
for real spectator-peer ingest in `clients/silencer/src/net/relay.cpp`.
After that, Stage 4 (transport refactor) is the natural follow-up.
Stages 5, 6, 7 are also unblocked and can land in any order.

Concretely: a fresh agent session continuing this work should:

1. Read this document end to end (status board in Stages section).
2. Read `docs/plans/2026-05-09-spectating.md` for native spectator
   context — the `Peer::observer` + `MSG_CONNECT` admit path is what
   the relay needs to consume.
3. Read `clients/silencer/CLAUDE.md` for the C++ client conventions.
4. Read `clients/silencer/src/net/relay.{h,cpp}` end to end — the
   scaffold's Run loop is where ingest replaces the BEAT generator.
5. Read `clients/silencer/src/net/dedicatedserver.cpp` and adjacent
   peer-mesh code — the relay mirrors the dedicated server pattern
   (headless binary mode that joins via existing networking code).
6. Build native + WASM and confirm both still build clean before
   touching anything. WASM bundle: `cd clients/silencer && emcmake
   cmake -S . -B build-wasm && emmake make -j8 -C build-wasm`,
   serve via `python3 -m http.server 8765` from `build-wasm/`,
   check `http://localhost:8765/silencer.html`. Needs
   `~/emsdk/emsdk_env.sh` sourced.
7. Implement Stage 2 finish: relay logs in to the lobby, asks to
   `SpectateGame <gameid>`, joins via `MSG_CONNECT` with the
   observer bit set, reads snapshots, forwards them through the
   existing outbox. End-to-end smoke: native dedicated server →
   relay → WS client receives real snapshot bytes (not BEAT).
8. Then Stage 4 transport refactor in one sweep — both `lobby.cpp`
   and `world.cpp` together. Native E2E stays green.

Resolved implementation questions:

- **Embedded WebSocket library:** hand-rolled, no third-party dep
  (both Go `services/lobby/wsutil.go` and C++ `src/net/relay.cpp`).
- **Lobby WS wire format:** JSON envelopes for spectator facade,
  binary frames for relay snapshots. Both implemented.
- **Lazy vs eager relay spawning:** still open — Stage 7. The
  `/spectate <gameid>` handler today just returns the configured
  base URL; spawn-on-demand or pre-spawn is a deploy-time decision.

## Handoff prompt

> WASM browser-spectator build. Design at
> `docs/plans/2026-05-10-wasm-spectator.md` — read it end to end,
> especially the Stages section status board and "Where to pick up".
>
> Stages 1 (Emscripten foundation + SDL_Renderer/WebGL2) and 3 (lobby
> WS facade) are done. Stage 2 scaffold (`silencer --relay` binary
> mode in `clients/silencer/src/net/relay.{h,cpp}`) is in but emits
> a synthetic `BEAT<seq>` keepalive instead of real snapshots.
>
> #156 just merged the native spectator work (`Peer::observer`,
> `MSG_CONNECT` observer admit, `viewedpeerid`, spectator controls),
> which unblocks Stage 2 finish and Stages 4-6. Your job: swap the
> relay's BEAT generator for real spectator-peer ingest — relay
> logs into the lobby, `SpectateGame <gameid>`, sends `MSG_CONNECT`
> with the observer bit set, reads snapshots off the peer mesh,
> forwards them verbatim to WS clients via the existing 8MB outbox.
> Mirror the headless `dedicatedserver.cpp` pattern. End-to-end smoke:
> native dedicated server → relay → WS client receives real bytes.
>
> Branch is `hv/wasm` (PR #157). Spectator-only forever; do not
> design for browser-plays-the-game or replay/recording.
