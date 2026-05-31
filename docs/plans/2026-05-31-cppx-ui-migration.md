# cppx UI migration — intent record (2026-05-31)

Durable record of *why* and the stable high-level decisions. **The living source of
truth is the Linear project**, not this file — see
[Silencer client cppx migration](https://linear.app/silencer-cc/project/silencer-client-cppx-migration-f106f12cf8a7)
and its doc [Migration goal and architecture notes](https://linear.app/silencer-cc/document/migration-goal-and-architecture-notes-3b8352cbedb6).
Task tracking, the full provider/hook/screen contract, and progress live in Linear; this
file stays short so it can't go stale.

## The reframe
`clients/silencer`'s UI is built entirely on **Clay**. The golden reference
`/Users/hv/repos/ui` uses **zero Clay** — it is a self-contained retained React-style
engine (hook runtime + Yoga layout + RGBA draw-command IR + `.cppx` authoring). So the
migration is a **full engine replacement**, not adding JSX on top of Clay.

## Stable decisions
- **Renderer bridge:** keep the golden RGBA `draw_command`/executor authoritative; bridge
  Silencer chrome through the existing `Image` command (`texture_id`+`nine_slice`+`tint`)
  backed by an RGBA `TextureRegistry` (sprite→RGBA bake). World renderer untouched. Retire
  the sprite-bitmap font for UI text (golden TTF). Never use `DrawCommandKind::Custom`.
- **Architecture:** no MVC; screens are components; hooks are virtualized models (state +
  actions, carved by UI domain, not backend object); public API = props/children/hooks/
  providers only; mutations queued + drained after render.
- **Navigation:** two-tier — imperative `use_navigation` for menus/overlays; state-driven
  `AppRoot` reconciler (`use_session().phase` → stack root) for match transitions. The game
  names no screen.
- **Two game-layer refactors:** (1) promote the local-player command/transition methods to
  public + delete the `GameUiPipeline`/`LobbyScreen`/`ScreenContext` friend grants
  (replication-safe; FADEOUT-gated UI mutation drain); (2) the multi-device binding model
  already exists in `keybinds.h` — delete the 2-slot UI projection + OR/AND toggle, expose
  rows-of-combos.

## Execution
Single long-lived branch `hv/cppx-migration-cc` → draft PR #267. Slices = Linear
SIL-6…SIL-24 (blocked-by graph wired). `/Users/hv/repos/ui` wins on disagreement.
