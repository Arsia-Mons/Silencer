# game.cpp refactor — design

**Status:** design complete. See [`2026-05-09-game-cpp-refactor-progress.md`](./2026-05-09-game-cpp-refactor-progress.md) for execution tracking.

**Goal:** break up `clients/silencer/src/game/game.cpp` (6,544 lines, single `Game` god-class) into focused, navigable units without changing runtime behavior.

---

## Section 1 — Architecture overview

The problem: `game.cpp` is 6,544 lines. It's one giant class called `Game` that does ~10 different jobs in one file. Hard to navigate, hard to edit.

The plan: don't rewrite anything. Cut the file into pieces and put each piece in its own folder. The class `Game` stays — same members, same behavior. We spread the code across many small files instead of one huge one.

Two clusters of code already act like little self-contained machines, so we promote them to their own classes:

1. **Map downloads.** `Game` owns 3 background threads + 3 progress counters + a mutex, all just for "fetch a map from the server." That's a whole subsystem hiding inside `Game`. We pull it out into a `MapDownloader` class.
2. **Background music/ambience.** `Game` owns 3 audio channels and the cross-fade logic for ambient sounds. Pull it out into an `AmbienceMixer` class.

Beyond those two, the menu UI gets the full class treatment using a **three-tier model** (Screen / Panel / Modal — see Section 2). The other categories (gameplay-state Tick bodies, input handling, in-game lifecycle) stay as `Game::` member functions, just spread across companion `.cpp` files in topic-organized sub-folders.

### Why menus get the full class treatment (not just file split)

Today, every menu's "build widgets" and "handle clicks" code is a `Game::` member (`CreateMainMenuInterface`, `ProcessMainMenuInterface`, etc.). There's no per-screen class — just a generic `Interface` widget bag and `Game` doing all the screen-specific glue. That glue needs `world` (to register the interface), the state machine vars (`state`, `nextstate`, `GoToState`), `Config`, `lobby`, the keybind map, etc. — so the easiest home was as members of `Game` that already had access to all that.

A clean design has one class per UI surface. The mechanical-split-only option (move menu methods into `menu_*.cpp` files, still as `Game::` members) was rejected in favor of doing it properly now.

---

## Section 2 — The new shape

### The three-tier UI model

The existing code already distinguishes (implicitly) between three kinds of UI surface — we make that explicit.

| Term | Definition | Examples | Lifecycle owner |
|---|---|---|---|
| **Screen** | Top-level UI surface bound to a Game state. One visible at a time (plus modals on top). Owns the root `Interface` widget. | MainMenu, LobbyConnect, **Lobby**, Options, OptionsControls, OptionsDisplay, OptionsAudio, GameSummary, Update | The screen stack (held by Game) |
| **Panel** | A sub-`Interface` composed inside a Screen. Multiple visible at once. Screen decides when to build/swap/destroy them. **Not** on the stack. | Character, Chat, GameSelect, GameCreate, GameJoin, GameTech (all live inside `LobbyScreen`) | Their parent Screen |
| **Modal** | Overlay pushed on top of whatever's active. Blocks input below. Has a callback for "user closed it." | ModalDialog, PasswordDialog, MapPreview | Pushed on the screen stack, but its own type with render-underlay |

**Why three and not one:** the current `LobbyScreen` would otherwise be a 990-line beast. It is genuinely composed of multiple co-visible sub-Interfaces (chat panel, character panel, game-select-or-create panel) — that's how the existing code works (`iface->AddObject(chatinterface)`, etc.). Modeling them as Panels owned by `LobbyScreen` (not as separate stack-managed Screens) matches reality and keeps each file small.

### The base classes

#### `Screen` — top-level UI

```cpp
// clients/silencer/src/game/screens/screen.h
class Screen {
public:
  virtual ~Screen() = default;

  // Build widgets and add the root Interface to world. Called once on push.
  virtual void Build(ScreenContext& ctx) = 0;

  // Called once per Tick while screen is on top. Reads button clicks,
  // updates dynamic content (chat lines, server list, download progress),
  // ticks owned panels.
  virtual void Tick(ScreenContext& ctx) = 0;

  // Tear down widgets. Called on pop/replace.
  virtual void Destroy(ScreenContext& ctx) = 0;

  // Optional: handle a back/cancel request (esc, right-click). Default = pop.
  virtual bool HandleBack(ScreenContext& ctx) { return true; }

  Uint16 interfaceId = 0;  // the root Interface
};
```

#### `Panel` — sub-component owned by a Screen

```cpp
// clients/silencer/src/game/panels/panel.h
class Panel {
public:
  virtual ~Panel() = default;

  // Build widgets and attach to the parent Interface.
  virtual void Build(ScreenContext& ctx, Interface* parent) = 0;

  // Called by the parent Screen's Tick when this panel should update.
  virtual void Tick(ScreenContext& ctx) = 0;

  // Tear down widgets. Called when the parent Screen pops or swaps the panel.
  virtual void Destroy(ScreenContext& ctx) = 0;

  Uint16 interfaceId = 0;  // sub-Interface added to parent
};
```

A Screen owns its panels by composition:

```cpp
class LobbyScreen : public Screen {
  CharacterPanel character;
  ChatPanel chat;
  std::unique_ptr<Panel> mainPanel;  // GameSelectPanel or GameCreatePanel — swaps
  // ...
};
```

Panels are not on the screen stack — the parent Screen builds, ticks, swaps, and destroys them.

#### `Modal` — overlay with callback

```cpp
// clients/silencer/src/game/modals/modal.h
class Modal : public Screen {
public:
  // Modals share Screen's lifecycle (pushed/popped on the stack), but the
  // stack renders the screen underneath them too (rather than hiding it).
  // Each Modal subclass invokes its onClose callback before requesting pop.
  bool isOverlay() const { return true; }
};
```

(Either a flag on `Screen`, or a virtual on a `Modal` subclass — to be decided in implementation. The contract is: when a Modal is on top of the stack, the renderer draws the Screen below it first.)

### `ScreenContext` — curated API screens use to talk to Game

Everything a screen needs — and **only** what they need — comes through this. The contract is the public surface: the named subsystem refs and the named action methods listed below. How `ScreenContext` implements those actions internally (private `Game&` ref, `std::function` callbacks, anything else) is an implementation detail, not part of the contract.

```cpp
// clients/silencer/src/game/screens/screen_context.h
class ScreenContext {
public:
  // Subsystem references — every screen has access to these.
  World&    world;
  Renderer& renderer;
  Lobby&    lobby;       // shorthand for world.lobby
  Config&   config;
  KeyMap&   keymap;
  Updater&  updater;
  AmbienceMixer&  ambience;       // (extracted collaborator)
  MapDownloader&  mapDownloader;  // (extracted collaborator)

  // State-machine + screen-stack actions — the architectural primitives.
  void GoToState(Uint8 newState);
  void GoBack();
  void RequestQuit();
  void PushScreen(std::unique_ptr<Screen> s);
  void PopScreen();
  void ReplaceScreen(std::unique_ptr<Screen> s);
  void ShowModal(std::unique_ptr<Modal> m);            // generic
  void ShowMessage(const char* msg, std::function<void(bool ok)> onClose); // helper for ModalDialog
};
```

Panels receive the same `ScreenContext` from their parent Screen — they have full access to subsystems but typically don't drive the screen stack themselves (that's the parent Screen's job).

**Principle:** `ScreenContext` only holds things every Screen/Panel structurally needs (subsystem refs + state-machine actions). Session-specific data (username, selected agency, "is this live MP") lives on whichever subsystem owns it semantically — **not** as accessors on the context.

Where session-specific data actually goes:

- **`LocalUsername`** — was `char localusername[17]` on `Game`, populated from config/account. Belongs on `Config`. Screens read `ctx.config.localusername`.
- **`SelectedAgency`** — `Game::GetSelectedAgency()` is 43 lines of "look at the agency button states in the character interface and figure out which one is picked." That's character-panel logic. Becomes a method on `CharacterPanel`; the result (chosen agency) gets stored where the rest of the local player's identity lives. Other surfaces read it from there.
- **`IsLiveMultiplayer`** — one-line derived view of game state (`state == INGAME && !replayfile`). Either a free function, a method on `World`, or just inlined where needed.

**Stricter rule for the implementation:** if a Screen/Panel needs something that doesn't fit cleanly into one of the subsystem refs above, that's a signal the data is sitting in the wrong place — fix the data home, don't add a context accessor.

### `ScreenStack` — held by Game, replaces all those `Uint16 *interface` members

```cpp
// inside Game (game.h, much slimmer)
std::vector<std::unique_ptr<Screen>> screenStack;  // top = active; modals stack on top
// The 12 Uint16 *interface members go away.
```

### What `Game::Tick`'s state cases collapse to

Today (LOBBY case is ~187 lines):
```cpp
case LOBBY: {
  // ... 187 lines of inline chat updates, button checks, sub-screen swapping ...
}
```

After:
```cpp
case LOBBY: {
  if (stateisnew) {
    PushScreen(std::make_unique<LobbyScreen>());
    stateisnew = false;
  }
  TickActiveScreen();   // top-of-stack Screen->Tick(ctx); modals tick first
} break;
```

The 187 lines move into `LobbyScreen::Tick()` (orchestration) plus `CharacterPanel::Tick()`, `ChatPanel::Tick()`, etc. (per-panel logic).

### Concrete example: `MainMenuScreen`

The current `Game::ProcessMainMenuInterface` (lines 4322–4346) becomes:

```cpp
// clients/silencer/src/game/screens/main_menu_screen.cpp
void MainMenuScreen::Build(ScreenContext& ctx) {
  Interface* iface = /* construct buttons (was CreateMainMenuInterface) */;
  interfaceId = iface->id;
}

void MainMenuScreen::Tick(ScreenContext& ctx) {
  Interface* iface = ctx.world.GetObjectFromId<Interface>(interfaceId);
  for (Uint16 oid : iface->objects) {
    Button* b = ctx.world.GetObjectFromId<Button>(oid);
    if (!b || !b->clicked) continue;
    switch (b->uid) {
      case 0: ctx.GoToState(SINGLEPLAYERGAME); break;
      case 1: ctx.GoToState(LOBBYCONNECT); break;
      case 2: ctx.GoToState(OPTIONS); break;
      case 3: ctx.RequestQuit(); break;
    }
  }
}

void MainMenuScreen::Destroy(ScreenContext& ctx) {
  ctx.world.GetObjectFromId<Interface>(interfaceId)->DestroyInterface(ctx.world, ...);
}
```

---

## Section 3 — Inventory + folder layout

### 9 Screens (top-level, stack-managed)

| Screen | Replaces | Notes |
|---|---|---|
| `MainMenuScreen` | `CreateMainMenuInterface` + `ProcessMainMenuInterface` | ~73 LoC |
| `LobbyConnectScreen` | `CreateLobbyConnectInterface` + `ProcessLobbyConnectInterface` | ~220 LoC |
| `LobbyScreen` | `CreateLobbyInterface` + parts of `ProcessLobbyInterface` (orchestration only) + `UpdateLobbyMapName` + `LOBBY` tick | ~300 LoC after panels are extracted (down from 990) |
| `OptionsScreen` | `CreateOptionsInterface` + `OPTIONS` tick | ~70 LoC |
| `OptionsControlsScreen` | `CreateOptionsControlsInterface` + `OPTIONSCONTROLS` tick + keybind UI helpers (`LegacyView`, `WriteLegacy`, `GetActionKeyDisplayName`, `GetKeyName`, `CycleKeybindPreset`, `ForkActiveProfileIfBuiltin`) | ~370 LoC |
| `OptionsDisplayScreen` | `CreateOptionsDisplayInterface` + `OPTIONSDISPLAY` tick | ~150 LoC |
| `OptionsAudioScreen` | `CreateOptionsAudioInterface` + `OPTIONSAUDIO` tick | ~130 LoC |
| `GameSummaryScreen` | `CreateGameSummaryInterface` + `ProcessGameSummaryInterface` + `UpdateGameSummaryInterface` + `AddSummaryLine` + `MISSIONSUMMARY` tick | ~325 LoC |
| `UpdateScreen` | `CreateUpdateInterface` + `ProcessUpdateInterface` + `LaunchStage2` + `UPDATING` tick | ~240 LoC |

### 6 Panels (owned by `LobbyScreen`)

| Panel | Replaces | Notes |
|---|---|---|
| `CharacterPanel` | `CreateCharacterInterface` + `Game::GetSelectedAgency` (now a method here) | ~160 LoC |
| `ChatPanel` | `CreateChatInterface` | ~70 LoC |
| `GameSelectPanel` | `CreateGameSelectInterface` | ~85 LoC; swaps with `GameCreatePanel` |
| `GameCreatePanel` | `CreateGameCreateInterface` + map-upload flow callbacks | ~250 LoC; swaps with `GameSelectPanel` |
| `GameJoinPanel` | `CreateGameJoinInterface` | ~35 LoC |
| `GameTechPanel` | `CreateGameTechInterface` + `UpdateTechInterface` | ~270 LoC |

### 3 Modals (overlays)

| Modal | Replaces | Lives in | Notes |
|---|---|---|---|
| `ModalDialog` | `CreateModalDialog` + `DestroyModalDialog` | `ui/modals/` | OK/cancel popup; carries an `onClose(bool)` callback. Generic. |
| `PasswordDialog` | `CreatePasswordDialog` | `ui/modals/` | password entry; `onClose(string)` callback. Generic. |
| `MapPreviewModal` | `CreateMapPreview` | `ui/screens/lobby/modals/` | Lobby-only (preview a map before joining). |

### What stays on `Game`

Gameplay states (`INGAME`, `SINGLEPLAYERGAME`, `HOSTGAME`, `JOINGAME`, `TESTGAME`, `REPLAYGAME`) are **not** screens — they're the actual game running, not UI. Those tick bodies stay as `Game::TickInGame()`, `Game::TickSinglePlayer()` etc. in `src/game/tick/` — mechanically split.

### Folder layout

UI lives in `clients/silencer/src/ui/` (where the existing widget files already live). The non-UI parts of the refactor (extracted collaborators, mechanical Tick split, etc.) stay under `clients/silencer/src/game/`.

**Co-location rule:** anything used by exactly one screen lives inside that screen's folder. Anything used across screens lives at the top of `ui/`.

```
clients/silencer/src/ui/
├── components/                     # existing widget primitives, moved into a subfolder
│   ├── button.h/.cpp
│   ├── interface.h/.cpp
│   ├── overlay.h/.cpp
│   ├── scrollbar.h/.cpp
│   ├── selectbox.h/.cpp
│   ├── stats.h/.cpp
│   ├── teambillboard.h/.cpp
│   ├── textbox.h/.cpp
│   ├── textinput.h/.cpp
│   └── toggle.h/.cpp
│
├── panels/                         # cross-screen panels (empty today; created when needed)
│   └── panel.h                     # Panel base class
│
├── modals/                         # cross-screen modals
│   ├── modal.h                     # Modal base (subclass of Screen with overlay flag)
│   ├── modal_dialog.h/.cpp         # generic OK/Cancel popup; reusable
│   └── password_dialog.h/.cpp      # password entry; reusable
│
└── screens/
    ├── screen.h                    # Screen base class
    ├── screen_context.h/.cpp       # ScreenContext
    │
    ├── main_menu/
    │   └── main_menu_screen.h/.cpp
    │
    ├── lobby_connect/
    │   └── lobby_connect_screen.h/.cpp
    │
    ├── lobby/
    │   ├── lobby_screen.h/.cpp     # orchestrates panels below
    │   ├── panels/                 # all 6 are lobby-only today
    │   │   ├── character_panel.h/.cpp
    │   │   ├── chat_panel.h/.cpp
    │   │   ├── game_select_panel.h/.cpp
    │   │   ├── game_create_panel.h/.cpp
    │   │   ├── game_join_panel.h/.cpp
    │   │   └── game_tech_panel.h/.cpp
    │   └── modals/
    │       └── map_preview_modal.h/.cpp  # only lobby triggers map preview
    │
    ├── options/
    │   ├── options_screen.h/.cpp
    │   ├── options_controls_screen.h/.cpp
    │   ├── options_display_screen.h/.cpp
    │   └── options_audio_screen.h/.cpp
    │
    ├── game_summary/
    │   └── game_summary_screen.h/.cpp
    │
    └── update/
        └── update_screen.h/.cpp
```

```
clients/silencer/src/game/         # non-UI Game internals
├── game.h                          # Game class — slimmer
├── game.cpp                        # ctor/dtor/Load/Loop/GoToState/GoBack + screen-stack impl
├── replay.cpp/.h                   # untouched
├── team.cpp/.h                     # untouched
├── user.cpp/.h                     # untouched
│
├── tick/                           # gameplay-state Tick bodies (mechanical split)
│   ├── tick_dispatch.cpp           # the switch — now thin
│   ├── tick_ingame.cpp             # INGAME (~228 lines)
│   ├── tick_singleplayer.cpp      # SINGLEPLAYERGAME (~391 lines)
│   ├── tick_hostjoin.cpp           # HOSTGAME, JOINGAME, TESTGAME
│   ├── tick_replay.cpp             # REPLAYGAME
│   └── tick_misc.cpp               # FADEOUT
│
├── ingame.cpp                      # LoadMap, UnloadGame, JoinGame, GiveDefaultItems,
│                                   # ShowDeployMessage, CheckForQuit/EndOfGame/ConnectionLost,
│                                   # ProcessInGameInterfaces, ShowTeamOverlays
│
├── events.cpp                      # HandleSDLEvents, OnScancodeDown/Up, UpdateInputState,
│                                   # OpenFirstGamepad, PollGamepadState
│
├── headless.cpp                    # DrainControlQueue, PostFrameReplies
│
├── map_downloader.h/.cpp           # extracted: owns dl*/mapjoin*/mapUpload* state + threads
│                                   # + ListFiles, FindMap, SaveMap, CalculateMapHash,
│                                   #   StringFromHash, LoadMapData, ProcessMapDownload
│
└── ambience_mixer.h/.cpp           # extracted: owns bgchannel[3], music state
                                    # + CreateAmbienceChannels, UpdateAmbienceChannels,
                                    #   LoadRandomGameMusic, FadedIn, PlayMusic,
                                    #   GetGameChannelName
```

**Modal placement rationale.** All three modals are currently invoked only from lobby code (verified by grep). But `ModalDialog` and `PasswordDialog` are conceptually generic UI primitives that any screen could need — they go in `ui/modals/`. `MapPreview` is genuinely lobby-specific (preview a map before joining a game) — it goes in `ui/screens/lobby/modals/`. Same rule as panels.

**`ui/panels/` is empty today.** All 6 panels are owned by `LobbyScreen` so they live in `ui/screens/lobby/panels/`. The top-level `ui/panels/` folder exists only to hold `panel.h` (the base class) and as the documented home for any future cross-screen panel.

**CMake includes.** The existing tree uses bare-filename includes (`#include "button.h"`) because `src/ui` is on the CMake include path. We add the new subdirectories (`src/ui/components`, `src/ui/panels`, `src/ui/modals`, `src/ui/screens`, and each screen subfolder) to `target_include_directories`, so existing `#include "button.h"` keeps resolving — no caller updates needed for the move. New code can use bare names too (`#include "lobby_screen.h"`).

### Result

- **`game.cpp` → ~250 lines** (ctor/dtor/Load/Loop/GoToState/GoBack/screen stack management).
- **`game.h` → ~170 lines** (down from 278), most of the `Uint16 *interface` members and menu-method declarations gone.
- Largest single file: `lobby_screen.cpp` at ~300 lines (down from one 6,544-line file; lobby's 990-line behavior is split across the screen + 6 panels).
- Most files: 50–250 lines.
- Total new files: ~45 (most are `.h/.cpp` pairs).

