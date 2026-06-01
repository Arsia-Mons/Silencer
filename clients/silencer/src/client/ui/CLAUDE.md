# clients/silencer/src/client/ui - Client UI app-shell

This subtree owns Silencer's app-side UI composition: the retained
`client::ui::ClientUi` shell, screen/overlay navigation, the global
provider chain, the capability hooks screens read, and the product theme.
Screens themselves are authored as `.cppx` view functions composing these
hooks + the `src/ui` runtime; they land in later slices (today `AppRoot`
renders per-phase scaffolds).

The live UI is the golden RETAINED cppx engine (a React-style hook runtime
+ Yoga flex layout + an RGBA draw-command IR — `src/ui`). This layer adds
Silencer's product concerns on top of that screen-agnostic substrate; it is
not the renderer, event loop, or game state owner.

## Ownership

- `ClientUi` (`app_shell/client_ui.h`) owns one retained UI tree across
  frames. Per frame the `UiPipeline` (`app_shell/ui_pipeline.h`) calls
  `begin_frame` → `build_visible_screens` (wrapped by the App's
  `FrameProvider`) → `end_layout` → `update_retained_runtime`, which builds
  the retained tree, runs Yoga layout, the focus/hit-test pass, and emits the
  `::ui::DrawCommandList` the renderer executes. Screens never run this
  lifecycle themselves.
- `ScreenStack` (`app_shell/navigation/`) is the single owner of the screen
  stack. Entry 0 is the always-mounted `AppRoot` (`app_shell/app_root.h`):
  each frame it reads `use_session().phase` and renders the screen that owns
  that phase as its child (the declarative phase reconciler). Tier-1
  navigation (options, pause, modals) pushes `OverlayScreen`s *above* AppRoot
  via `use_navigation()`; overlays re-establish their own providers and never
  change `phase`.
- Screens declare UI by returning a `::ui::UiElement` tree (`UiScreen::
  build_element`). Stack mutations that change persisted/navigation state are
  queued and drained after render — never mutate the stack, world, or domain
  state mid-build. Use `queue_push_screen` / `queue_pop_*` /
  `queue_deferred_mutation` (`app_shell/deferred_ui_mutation.h`).
- The renderer bridge (`src/render/cppx_ui`) owns rasterization, fonts,
  textures, and text measurement. Screens and components emit the SDL-free
  draw IR only; they do not call SDL, `Renderer`, or `Surface`.

## Hooks (capability seams)

Screens read app state through hooks under `hooks/`; the composition root
(`src/game/ui/game_ui_pipeline.cpp`) builds the model + intent closures fresh
each frame and installs them via the providers under `providers/`. Hooks
expose read state + named intent closures — never a raw `Game`/`World` handle.

- `use_server()` — the live `Game*` (game-coupled; `silencer::game_ui`).
- `use_session()` — `SessionPhase` projection + transition intents
  (`play_online`, `leave_match`, `set_paused`, …). AppRoot's reconciler reads
  `phase`.
- `use_app()` — app-global affordances (today `quit`).
- `use_settings()` — persisted prefs (music/volume/fullscreen/scaling) with
  live-apply `set_*`, `commit`, `revert`, and a `dirty` flag.
- `use_key_map()` — rows-of-combos keybind model + the six mutation intents
  (cap-enforcing, fork-if-builtin).
- `use_updater()` — self-updater phase/progress + consent/cancel/retry.
- `use_navigation()` — push/pop/reset against the `ScreenStack`.
- `use_tokens()` — the resolved product `::ui::Theme` (forwards to
  `::ui::use_theme()`).

## Theme

`app_theme.{h,cpp}` holds the product look (dark slate, accent blue, control
gradients). `ThemeProvider` installs it OUTERMOST into `ui::ThemeContext` so
`use_tokens()`/`use_theme()` resolves the slate palette tree-wide. The neutral
fallback (`ui::default_theme()`) lives in `src/ui` and applies only when no
provider is mounted. Components resolve their own `VisualStyle` at authoring
time from the theme; the renderer never sees the theme.

## Verification

- Build through `clients/silencer/build.ps1` or `clients/silencer/build.sh`;
  do not run raw CMake/Ninja commands.
- Verify visual/interaction behavior through the real runtime (control socket
  + screenshots via `clients/cli/index.ts`), not compile success alone.
- Update `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` when ownership
  boundaries change; it guards this layer's seams.
