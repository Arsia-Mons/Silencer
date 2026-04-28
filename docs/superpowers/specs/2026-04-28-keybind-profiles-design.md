# Keybinding profiles for the Silencer client — design

**Date:** 2026-04-28
**Scope:** Replace the flat-fields keybinding system in `clients/silencer`
with a profile-based, table-driven design that:

1. Is grokkable when grepped — no raw `SDL_Scancode` integers in source
   or config files.
2. Supports multiple named profiles (presets) the player can switch
   between.
3. Supports keyboard, mouse, and gamepad inputs in a single uniform
   binding vocabulary.
4. Exposes get/put/list operations over the existing CLI control
   socket, modeled on AWS SSM Parameter Store.

## Motivation

The current keybinding system has three problems:

- **Opaque on disk and in source.** `keymoveup = 82 0 0` in
  `config.cfg` and `(SDL_Scancode)107` literals in `config.cpp` mean
  nothing to a human or coding agent grepping the repo. There's no
  way to know what key 82 is without an SDL header open.
- **Adding an action is six edits.** A new game action requires
  touching `Config` (two new fields), `Input` (one bool + one
  serialize line), `LoadDefaults`/`Save`/`Load` (three lines each),
  `IndexToConfigKey` (one case), `keynames[]` (one entry), and the
  per-action `KeyIsPressed` cascade in `game.cpp`. The fan-out
  encourages drift.
- **One config, no profiles.** "Preset" today means a `#ifdef OUYA`
  block in `LoadDefaults`. Players can't keep a "wasd" alongside a
  "default" alongside a "controller" set without manually editing
  the file.

## Non-goals

- **Wire-format changes.** `Input`'s 24 named booleans stay
  byte-identical on the wire. Only the *poll loop* that fills them
  is refactored. No `SILENCER_VERSION` bump, no lobby coordination.
- **Steam Input integration.** The action table designed here is
  Steam-Input-shaped (named actions, string ids, human labels) so a
  future `#ifdef SILENCER_STEAM` path can drop in without
  restructuring. Out of scope for this change.
- **Per-context action sets** (e.g. separate "menu" / "vehicle" /
  "in-game" maps). One global keymap; if needed later, layered
  on top.
- **Backwards-compat reading of old `keymoveup = 82 0 0` lines.**
  First launch after this change drops the lines from `config.cfg`
  and writes a default profile. Players who customized keys lose
  their bindings exactly once. (Per repo policy: "No
  backwards-compat shims during refactors unless asked.")

## Architecture

Three layers, each with one job.

```
┌─────────────────────────────────────────────────────────────────┐
│  Profile JSON files                                              │
│  shared/assets/keybinds/<name>.json (built-in, read-only)        │
│  <datadir>/keybinds/<name>.json     (user, writable)             │
└────────────────┬────────────────────────────────────────────────┘
                 │ load / save (round-trip strings ↔ struct)
┌────────────────▼────────────────────────────────────────────────┐
│  KeyMap (in-memory, parsed)                                      │
│  ActionBindings actions[Action::Count]                           │
│  bool IsPressed(Action, kb, gp)                                  │
└────────────────┬────────────────────────────────────────────────┘
                 │ per-frame poll
┌────────────────▼────────────────────────────────────────────────┐
│  Input (wire-stable, unchanged on wire)                          │
│  bool keymoveup; bool keymovedown; ... (named bools as today)    │
└─────────────────────────────────────────────────────────────────┘
```

The CLI control socket reaches into the `KeyMap` layer directly:

```
silencer-cli ──JSON-lines TCP──> controlserver
                                     │
                                     ▼
                              keybind dispatch (game thread)
                                     │
                          ┌──────────┴──────────┐
                          ▼                     ▼
                    KeyMap (mutate)     KeyMap loader
                                            (file I/O)
```

## The action table

A single source of truth, owned by `keybinds.h`:

```cpp
enum class Action : uint8_t {
    MoveUp, MoveDown, MoveLeft, MoveRight,
    LookUpLeft, LookUpRight, LookDownLeft, LookDownRight,
    NextInv, NextCam, PrevCam, Detonate,
    Jump, Jetpack, Activate, Use, Fire,
    Disguise, NextWeapon, Chat,
    Weapon1, Weapon2, Weapon3, Weapon4,
    UiUp, UiDown, UiLeft, UiRight,
    Count
};

struct ActionInfo {
    Action      action;
    const char* id;     // "fire"   — stable string for files & CLI
    const char* label;  // "Fire"   — human-readable, shown in UI
};

extern const ActionInfo ACTION_TABLE[(int)Action::Count];
```

Defining a new action is one row in `ACTION_TABLE`. `keynames[]`,
`IndexToConfigKey`, and the per-action poll cascade all read from
this single table.

## Binding vocabulary

A `BindingKey` is one of four kinds, expressed as a tagged string at
the file/CLI boundary and as a small struct in memory.

| Kind                | String form     | In-memory                            |
|---------------------|-----------------|--------------------------------------|
| Keyboard scancode   | `KEY:Up`        | `{ Keyboard, SDL_SCANCODE_UP, 0 }`   |
| Mouse button        | `MOUSE:1`       | `{ Mouse, 1, 0 }`                    |
| Gamepad button      | `PAD:south`     | `{ GamepadButton, SDL_GAMEPAD_BUTTON_SOUTH, 0 }` |
| Gamepad axis (±)    | `PAD:lefty-`    | `{ GamepadAxis, SDL_GAMEPAD_AXIS_LEFTY, -1 }`    |

Round-trip uses SDL3's built-in helpers:

- `SDL_GetScancodeFromName("Up")` ↔ `SDL_GetScancodeName(82)`
- `SDL_GetGamepadButtonFromString("south")` ↔ `SDL_GetGamepadStringForButton(...)`
- `SDL_GetGamepadAxisFromString("lefty")` ↔ `SDL_GetGamepadStringForAxis(...)`

No bespoke string table is maintained — SDL3's existing controller-mapping
vocabulary (already used by `gamecontrollerdb.txt`) is the source of truth.

## Combo semantics

A `Binding` is a list of `BindingKey`s held simultaneously (AND).
An action's `bindings` is a list of `Binding`s, any of which fires
the action (OR).

```cpp
struct BindingKey {
    BindingDevice device;   // Keyboard / Mouse / GamepadButton / GamepadAxis
    int           code;     // SDL_Scancode | mouse btn | SDL_GamepadButton | SDL_GamepadAxis
    int8_t        axisDir;  // ±1 if device == GamepadAxis, else 0
};

struct Binding {
    std::vector<BindingKey> keys;   // size 1 = single key, size N = AND chord
};

struct ActionBindings {
    std::vector<Binding> bindings;  // OR across this list
};

class KeyMap {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    void LoadFromBuiltin(const std::string& profileName);
    bool IsPressed(Action a,
                   const Uint8* kb,
                   const GamepadState& gp) const;
    ActionBindings actions[(int)Action::Count];
};
```

The single rule: **OR-of-AND**. The 4-key chord aim actions
(`look_up_left` etc.) fall out as one `Binding` with two `BindingKey`s.
The fixed two-slot `primary`/`secondary` pair from the old design is
gone; bindings is just a list.

## File format

JSON, one file per profile, mirrors the `actordefs/`-per-file
pattern already used in this codebase.

`shared/assets/keybinds/default.json`:

```json
{
  "name": "default",
  "label": "Default",
  "actions": {
    "move_up":       { "bindings": ["KEY:Up"] },
    "move_down":     { "bindings": ["KEY:Down"] },
    "move_left":     { "bindings": ["KEY:Left"] },
    "move_right":    { "bindings": ["KEY:Right"] },
    "look_up_left":  { "bindings": [["KEY:Up", "KEY:Left"]] },
    "look_up_right": { "bindings": [["KEY:Up", "KEY:Right"]] },
    "look_down_left":  { "bindings": [["KEY:Down", "KEY:Left"]] },
    "look_down_right": { "bindings": [["KEY:Down", "KEY:Right"]] },
    "jump":          { "bindings": ["KEY:Tab"] },
    "jetpack":       { "bindings": ["KEY:Q"] },
    "activate":      { "bindings": ["KEY:Space"] },
    "use":           { "bindings": ["KEY:W"] },
    "fire":          { "bindings": ["KEY:E"] },
    "chat":          { "bindings": ["KEY:T"] },
    "next_inv":      { "bindings": ["KEY:R"] },
    "next_cam":      { "bindings": ["KEY:S"] },
    "prev_cam":      { "bindings": ["KEY:A"] },
    "detonate":      { "bindings": ["KEY:D"] },
    "disguise":      { "bindings": ["KEY:C"] },
    "next_weapon":   { "bindings": [] },
    "weapon_1":      { "bindings": ["KEY:1"] },
    "weapon_2":      { "bindings": ["KEY:2"] },
    "weapon_3":      { "bindings": ["KEY:3"] },
    "weapon_4":      { "bindings": ["KEY:4"] }
  }
}
```

A binding entry is **either** a string (single key) **or** an array
of strings (chord). The reader normalizes both to `Binding{ keys: [...] }`.

`shared/assets/keybinds/wasd.json` ships a modern-shooter preset.
`shared/assets/keybinds/gamepad.json` ships an SDL-Gamepad preset.

### File locations

Read-only built-ins ship with the install:

| Platform | Path                                                  |
|----------|-------------------------------------------------------|
| macOS    | `Silencer.app/Contents/Resources/assets/keybinds/`    |
| Linux    | `/usr/share/silencer/keybinds/`                       |
| Windows  | `<exe dir>\assets\keybinds\`                          |

Writable per-user profiles live in `<datadir>/keybinds/`:

| Platform | Path                                                  |
|----------|-------------------------------------------------------|
| macOS    | `~/Library/Application Support/Silencer/keybinds/`    |
| Linux    | `~/.config/silencer/keybinds/`                        |
| Windows  | (currently same as exe dir; future: `%APPDATA%`)      |

`GetResDir()` and `GetDataDir()` already resolve these. The keybinds
loader uses the existing helpers; no new platform code.

### Lookup order

| Operation               | Behavior                                             |
|-------------------------|------------------------------------------------------|
| `list profiles`         | Union of (datadir/*.json, resdir/*.json) by name     |
| `load profile X`        | Datadir copy if exists, else resdir copy             |
| `save profile X`        | Always writes to datadir (resdir is read-only)       |
| `unset action / reset`  | Delete writable file if it exists                    |

Player overrides of a built-in win automatically. Reverting is
deleting the writable file.

### Active profile pointer

One new line in `config.cfg`: `active_keybind_profile = default`.
Read on startup, written on `keybind use`. Existing `Config::Save`
gains one `WriteString` call; `Config::Load` gains one parse.

## Per-frame poll

The 28-line cascade in `game.cpp:1993` collapses to:

```cpp
const Uint8* kb = SDL_GetKeyboardState(nullptr);
GamepadState gp = ReadGamepadState();   // SDL3 gamepad query, see below
const KeyMap& km = Game::GetKeyMap();

// bool& accessor — captureless lambdas decay to function pointers,
// no overhead vs pointer-to-member. Needed because keyweapon[4] is
// an array; pointer-to-member can't index into it.
typedef bool& (*InputField)(Input&);

static const struct { Action a; InputField field; } INPUT_FIELDS[] = {
    { Action::MoveUp,        [](Input& i) -> bool& { return i.keymoveup;        } },
    { Action::MoveDown,      [](Input& i) -> bool& { return i.keymovedown;      } },
    { Action::MoveLeft,      [](Input& i) -> bool& { return i.keymoveleft;      } },
    { Action::MoveRight,     [](Input& i) -> bool& { return i.keymoveright;     } },
    { Action::LookUpLeft,    [](Input& i) -> bool& { return i.keylookupleft;    } },
    { Action::LookUpRight,   [](Input& i) -> bool& { return i.keylookupright;   } },
    { Action::LookDownLeft,  [](Input& i) -> bool& { return i.keylookdownleft;  } },
    { Action::LookDownRight, [](Input& i) -> bool& { return i.keylookdownright; } },
    { Action::NextInv,       [](Input& i) -> bool& { return i.keynextinv;       } },
    { Action::NextCam,       [](Input& i) -> bool& { return i.keynextcam;       } },
    { Action::PrevCam,       [](Input& i) -> bool& { return i.keyprevcam;       } },
    { Action::Detonate,      [](Input& i) -> bool& { return i.keydetonate;      } },
    { Action::Jump,          [](Input& i) -> bool& { return i.keyjump;          } },
    { Action::Jetpack,       [](Input& i) -> bool& { return i.keyjetpack;       } },
    { Action::Activate,      [](Input& i) -> bool& { return i.keyactivate;      } },
    { Action::Use,           [](Input& i) -> bool& { return i.keyuse;           } },
    { Action::Fire,          [](Input& i) -> bool& { return i.keyfire;          } },
    { Action::Chat,          [](Input& i) -> bool& { return i.keychat;          } },
    { Action::Disguise,      [](Input& i) -> bool& { return i.keydisguise;      } },
    { Action::NextWeapon,    [](Input& i) -> bool& { return i.keynextweapon;    } },
    { Action::Weapon1,       [](Input& i) -> bool& { return i.keyweapon[0];     } },
    { Action::Weapon2,       [](Input& i) -> bool& { return i.keyweapon[1];     } },
    { Action::Weapon3,       [](Input& i) -> bool& { return i.keyweapon[2];     } },
    { Action::Weapon4,       [](Input& i) -> bool& { return i.keyweapon[3];     } },
    { Action::UiUp,          [](Input& i) -> bool& { return i.keyup;            } },
    { Action::UiDown,        [](Input& i) -> bool& { return i.keydown;          } },
    { Action::UiLeft,        [](Input& i) -> bool& { return i.keyleft;          } },
    { Action::UiRight,       [](Input& i) -> bool& { return i.keyright;         } },
};

for (auto& f : INPUT_FIELDS) f.field(input) = km.IsPressed(f.a, kb, gp);
```

`Input`'s public bool fields are unchanged, so every reader downstream
(`actors/player.cpp`, `actors/playerai.cpp`, etc.) is untouched.

`GamepadState` is a small POD assembled once per frame from
`SDL_GetGamepads()` + `SDL_GetGamepadButton()` / `SDL_GetGamepadAxis()`.
If no gamepad is connected it's all-zero; `IsPressed` falls through
the `Pad*` cases trivially.

## Performance

Per action the new evaluator is one switch + one array index + one
vector iteration over a typically size-1 list. ~20–40ns per action,
~0.7µs total per frame for 24 actions. Current code is ~1µs/frame
(28 actions through three `Config::GetInstance()` calls each). New
version is marginally faster; both are < 0.01% of frame time. No
regression.

## CLI surface

Noun-first dispatch. Wire op is `"keybind"`; `args.subop` selects
the operation. The CLI wrapper parses two positionals
(`silencer-cli keybind <subop>`) before flag arguments.

| Sub-op      | Args                                          | Result shape                                          |
|-------------|-----------------------------------------------|-------------------------------------------------------|
| `list`      | —                                             | `{ active, profiles[], builtins[] }`                  |
| `actions`   | —                                             | `[{ id, label, default[] }, …]`                       |
| `get`       | `--profile X` (default active), `--action Y`  | one binding entry, or whole profile if `--action` omitted |
| `put`       | `--profile X --action Y --bindings KEY:F …`   | `{ profile, action, bindings[] }`                     |
| `unset`     | `--profile X --action Y`                      | `{ profile, action }` (writable override removed)     |
| `use`       | `--profile X`                                 | `{ active }`                                          |
| `new`       | `--profile X [--from Y]`                      | `{ profile }`                                         |
| `delete`    | `--profile X`                                 | `{ profile }` (writable file removed)                 |

### Behavior contract

- **Live application.** `put` / `unset` / `use` mutate the in-memory
  `KeyMap` immediately; the next per-frame poll picks them up. No
  Save+Load round-trip needed.
- **Replace, not merge.** `put` replaces the entire `bindings` list
  for one action. To remove a single binding the caller fetches,
  edits, and puts the new list. Mirrors AWS SSM `PutParameter`.
- **Built-ins are read-only.** `put` / `unset` / `delete` against a
  profile that has no writable copy will copy-on-write the built-in
  to the datadir before mutating. `delete` removes only the writable
  file; the built-in stays visible.
- **Atomic save.** Writes go to `<name>.json.tmp` then rename to
  `<name>.json`. Crash mid-save can't corrupt a profile.
- **Validation.** `put` parses each binding string before mutating.
  Unknown vocabulary returns `BAD_REQUEST` with the offending
  string in the error message. No partial application.

### Error codes

Reuses existing CLI error vocabulary from `controldispatch.cpp`:

| Code             | When                                                |
|------------------|-----------------------------------------------------|
| `BAD_REQUEST`    | Missing required arg, malformed binding string      |
| `NOT_FOUND`      | Unknown action id, unknown profile (on `get`/`use`) |
| `ALREADY_EXISTS` | `new` with a profile name that already exists       |
| `READ_ONLY`      | `delete` against a built-in-only profile            |
| `UNKNOWN_OP`     | Unrecognized `subop`                                |

### Sample session

```
$ silencer-cli keybind list
{"active":"default","profiles":["default"],"builtins":["default","wasd","gamepad"]}

$ silencer-cli keybind actions
[{"id":"fire","label":"Fire","default":["KEY:E"]}, ...]

$ silencer-cli keybind get --action fire
{"action":"fire","label":"Fire","bindings":["KEY:E"]}

$ silencer-cli keybind put --profile default --action fire \
                           --bindings KEY:Space PAD:righttrigger
{"profile":"default","action":"fire","bindings":["KEY:Space","PAD:righttrigger"]}

$ silencer-cli keybind new --profile mine --from default
{"profile":"mine"}

$ silencer-cli keybind use mine
{"active":"mine"}
```

## Files touched

New:
- `clients/silencer/src/input/keybinds.h` — `Action`, `ACTION_TABLE`,
  `BindingKey`, `Binding`, `ActionBindings`, `KeyMap`.
- `clients/silencer/src/input/keybinds.cpp` — load/save, parse,
  stringify, evaluator.
- `clients/silencer/src/input/keybinds_dispatch.cpp` — CLI
  `"keybind"` op handler. Hooked into `controldispatch.cpp`.
- `shared/assets/keybinds/default.json`
- `shared/assets/keybinds/wasd.json`
- `shared/assets/keybinds/gamepad.json`

Modified:
- `clients/silencer/src/platform/config.h` / `.cpp` — drop all
  `keyXxxbinding` / `keyXxxoperator` fields and the
  `KeyIsPressed` method. Add `active_keybind_profile` string.
- `clients/silencer/src/game/game.cpp` — replace the 28-line
  poll cascade (around line 1993) with the table-driven loop.
  Remove `IndexToConfigKey` (lines 5355–5458). Remove `keynames[]`
  init (lines 62–82) and replace with `ACTION_TABLE`-driven UI.
- `clients/silencer/src/game/game.h` — remove `IndexToConfigKey`
  declaration and `keynames[numkeys]`. Add `KeyMap& GetKeyMap()`.
- `clients/silencer/src/ui/interface.cpp` (and the `OPTIONSCONTROLS`
  flow in `game.cpp:1486`) — controls UI now scrolls
  `ACTION_TABLE`, edits a `Binding` list per row, "+ Add binding"
  / "Remove" instead of fixed two slots and OR/AND toggle.
- `clients/silencer/src/net/controldispatch.cpp` — add `"keybind"`
  dispatch entry.
- `clients/cli/index.ts` — recognize two positionals when first is
  `keybind`; pass through `subop` in `args`.
- `clients/silencer/CMakeLists.txt` — install
  `shared/assets/keybinds/` into the resource dir alongside
  existing assets.

Deleted:
- All `#ifdef OUYA` blocks in `Config::LoadDefaults` (their content
  moves into `keybinds/ouya.json` if the platform is still wanted;
  current CLAUDE.md flags OUYA as unmaintained, so it ships only
  if explicitly requested).

## Testing

The CLI control socket already has an end-to-end harness
(`tests/cli-agent/e2e/lib.sh`). New scenario tests:

- **Round-trip.** Start headless game, `keybind put fire KEY:Space`,
  `keybind get fire`, assert `["KEY:Space"]`.
- **Live application.** `keybind put fire KEY:F`, then assert next
  frame reports the matching `Input::keyfire` after a synthetic
  keystate (or via `world_state` introspection if available).
- **Profile switching.** `keybind use wasd`, `keybind get move_up`,
  assert `["KEY:W"]`.
- **Built-in revert.** Modify `default`, then `keybind delete
  default`, then `keybind get fire`, assert built-in reappears.
- **Validation.** `keybind put fire KEY:Bogus` returns
  `BAD_REQUEST` and does not mutate.
- **Atomic save.** Kill game mid-save (signal); restart; profile
  is either fully old or fully new, never partial.

Unit tests for `ParseBindingKey` / `Stringify` round-trip on every
SDL3 scancode + every gamepad button + every axis direction.

## Migration

First launch after this change:

1. `Config::Load` reads `config.cfg`. The legacy `keymoveup = …`
   lines are unknown keys and silently ignored (existing
   behavior — `Load` drops unrecognized lines).
2. `active_keybind_profile` is missing → defaults to `"default"`.
3. `KeyMap::LoadFromBuiltin("default")` runs, reading
   `<resdir>/keybinds/default.json`.
4. `Config::Save` rewrites `config.cfg` without the legacy
   keybinding lines.

Players who had customized keys lose them once. We are not
shipping a one-shot importer — the new defaults match the existing
non-OUYA defaults exactly, so most players see no change.

## Open questions

- **Mouse delta for aiming.** The current code does not bind mouse
  movement to anything; aim is keyboard-driven via the four
  diagonal chords. Out of scope here, but the `BindingDevice` enum
  has room for a future `MouseAxis` variant if needed.
- **Per-profile sensitivity / deadzone tuning.** Not in v1. The
  gamepad-axis deadzone is a constant in `keybinds.cpp`. Promoting
  it to per-profile metadata is a follow-up.
- **UI for picking a profile.** v1 ships the data layer + CLI. The
  in-game profile-picker UI (a new dropdown in `OPTIONSCONTROLS`)
  is its own implementation plan.
