#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

fail_if_match() {
  local pattern="$1"
  shift
  if rg -n "$pattern" "$@"; then
    echo "architecture boundary violation: $pattern" >&2
    exit 1
  fi
}

fail_if_path_exists() {
  local path="$1"
  if [ -e "$REPO_ROOT/$path" ]; then
    echo "legacy UI path still exists: $path" >&2
    exit 1
  fi
}

fail_if_ui_taxonomy_dirs_escape_roots() {
  local bad_paths
  bad_paths="$(
    find "$REPO_ROOT/clients/silencer/src" -type d \
      \( -name hooks -o -name providers -o -name components \) \
      -not -path "$REPO_ROOT/clients/silencer/src/client/ui/*" \
      -not -path "$REPO_ROOT/clients/silencer/src/ui/*" \
      -print
  )"
  if [ -n "$bad_paths" ]; then
    echo "$bad_paths" >&2
    echo "hooks/providers/components directories must live under client/ui or ui" >&2
    exit 1
  fi
}

fail_if_ui_controller_paths() {
  local bad_paths
  bad_paths="$(
    find "$REPO_ROOT/clients/silencer/src/client/ui" "$REPO_ROOT/clients/silencer/src/ui" \
      -type f \( -iname '*controller*' -o -iname '*_controller.*' \) \
      -print
  )"
  if [ -n "$bad_paths" ]; then
    echo "$bad_paths" >&2
    echo "UI controller files are not allowed; use provider-backed hooks" >&2
    exit 1
  fi
}

fail_if_tracked_generated_cppx_outputs() {
  local bad_paths
  bad_paths="$(
    git -C "$REPO_ROOT" ls-files 'clients/silencer/**' \
      | rg '(^|/)generated/cppx/|(^|/)cppx/generated/|[.](generated|transpiled)[.](cpp|h)$' \
      || true
  )"
  if [ -n "$bad_paths" ]; then
    echo "$bad_paths" >&2
    echo "cppx generated output belongs in the build tree, not committed source" >&2
    exit 1
  fi
}

fail_if_ui_taxonomy_dirs_escape_roots
fail_if_ui_controller_paths
fail_if_tracked_generated_cppx_outputs

fail_if_path_exists "clients/silencer/src/ui/modals"
fail_if_path_exists "clients/silencer/src/ui/panels"
fail_if_path_exists "clients/silencer/src/ui/screens"
fail_if_path_exists "clients/silencer/src/ui/clay"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.h"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/state_screens.h"
fail_if_path_exists "clients/silencer/src/client/ui/state_screens.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/navigation/ClientUiRoute.h"
fail_if_path_exists "clients/silencer/src/client/ui/screens/lobby/game_select_panel_layout.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/screens/options/components/boolean_setting_row.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/screens/options/components/boolean_setting_row.h"
fail_if_path_exists "clients/silencer/src/generated/cppx"
fail_if_path_exists "clients/silencer/src/client/ui/generated"
fail_if_path_exists "clients/silencer/src/ui/generated"

fail_if_match \
  "CLAY[[:space:]]*[(]|clay_ui_compositor|clay/clay[.]h|SurfacePayload|ClayCustomData" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel.h" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel_map_form.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_chrome_frame.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_screen.cpp"

fail_if_match \
  "\\b(currentinterface|ProcessInGameInterfaces|Interface \\*|new Interface)\\b|(^|[^[:alnum:]_/])ui/(modals|panels|screens)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "clay_inspector" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "\\b[A-Za-z0-9_]*Controller\\b|\\bcontroller(-shaped|-owned)?\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui" \
  --glob '!third_party/**'

fail_if_match \
  "\\b(ScreenContext|UiElementFrame|UiTree|FocusRuntime|ScreenStack|DeferredUiMutationSink|Surface|Renderer|RenderDevice|SDL_Window|UiInteractionRegistry)\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hooks" \
  "$REPO_ROOT/clients/silencer/src/client/ui/components" \
  "$REPO_ROOT/clients/silencer/src/ui/components" \
  --glob '*.hx' \
  --glob '*.h'

fail_if_match \
  "\\b(actions|Actions|ActionBag|CommandBag|Controller|Presenter|ViewModel|Manager)\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hooks" \
  --glob '*.h'

fail_if_match \
  "\\b(spriteBank|spriteIndex|bank|texture_id|nine_slice|palette)[[:space:]]*(=|;|,|\\)|\\})" \
  "$REPO_ROOT/clients/silencer/src/client/ui/components" \
  "$REPO_ROOT/clients/silencer/src/ui/components" \
  --glob '*.hx'

fail_if_match \
  'return[[:space:]]+([[:alnum:]_:]+[.])?[A-Z][A-Za-z0-9_]*[[:space:]]*[(]' \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui" \
  --glob '*.cppx'

if ! rg -q "src/client/ui/screens/.+[.](cppx|hx)" \
  "$REPO_ROOT/clients/silencer/CMakeLists.txt"; then
  echo "at least one product screen component must be authored as cppx" >&2
  exit 1
fi

fail_if_match \
  'nlohmann::json|#include[[:space:]]*[<"]nlohmann/json[.]hpp[>"]' \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  --glob '!third_party/**'

fail_if_match \
  'nlohmann::json[[:space:]]+(Game::)?GetWorldSummary' \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  '#include "(game|world|player|lobby|lobbygame|team|buyableitem|weapon|renderer|surface)[.]h"' \
  "$REPO_ROOT/clients/silencer/src/ui" \
  --glob '!third_party/**'

if rg -n "SDL_GetMouseState" \
  "$REPO_ROOT/clients/silencer/src/client/ui"; then
  echo "client UI must not collect SDL pointer state directly" >&2
  exit 1
fi

# Raw SDL event consumption belongs in the platform/event layer (game/).
# client/ui and ui/runtime must consume the typed UiInputState only.
if rg -n "SDL_EVENT_KEY_DOWN|SDL_EVENT_KEY_UP|SDL_KEYDOWN|SDL_KEYUP|SDL_PollEvent" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime"; then
  echo "client UI and ui/runtime must not consume raw SDL key events" >&2
  exit 1
fi

fail_if_match \
  "Clay_(BeginLayout|EndLayout|SetPointerState)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hud"

fail_if_match \
  "clay_bridge::(EnsureInitialized|Render)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hud"

fail_if_path_exists "clients/silencer/src/client/ui/hud/InGameOverlays.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/hud/InGameOverlays.h"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_player_list_overlay.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_player_list_overlay.h"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_buy_tech_overlay.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_buy_tech_overlay.h"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_chat_overlay.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_chat_overlay.h"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_readouts.cpp"
fail_if_path_exists "clients/silencer/src/client/ui/hud/hud_readouts.h"

fail_if_match \
  "Draw[A-Za-z0-9_]*Clay|BuildInGameHudUi|BuildInGameOverlaysUi|client/ui/hud|Clay_BeginLayout|Clay_EndLayout|Clay_SetPointerState|clay_bridge" \
  "$REPO_ROOT/clients/silencer/src/render/renderer.cpp" \
  "$REPO_ROOT/clients/silencer/src/render/renderer.h"

fail_if_match \
  "CLAY_TEXT|[.]font(Id|Size)|fontBank|fontWidth|BankText|bank_text|TextCellWidthFor|TextHeightForBank|MeasureBankText|Renderer::DrawText|\\bDrawText[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui/primitives" \
  --glob '!**/text.cpp' \
  --glob '!**/text_internal.h'

fail_if_match \
  "BeginClayFrame|BeginClayLayout|EndClayFrame" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/game"

fail_if_match \
  "\\bscreenStack\\b|\\bTickActiveScreen\\b" \
  "$REPO_ROOT/clients/silencer/src/game/game.cpp" \
  "$REPO_ROOT/clients/silencer/src/game/game.h"

fail_if_match \
  "\\bClientUiRef[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/game" \
  --glob '!third_party/**'

fail_if_match \
  "\\b(PushScreen|PopScreen|ReplaceScreen)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/game/game.cpp" \
  "$REPO_ROOT/clients/silencer/src/game/game.h"

fail_if_match \
  '#include "(character_create_screen|lobby_connect_screen|main_menu_screen|mission_summary_screen|options_audio_screen|options_controls_screen|options_display_screen|options_screen|update_screen|lobby_screen)[.]h"|PushScreen[[:space:]]*[(][[:space:]]*std::make_unique<' \
  "$REPO_ROOT/clients/silencer/src/game/loop/game_loop.cpp"

fail_if_match \
  "ShowStateScreen|state_screens" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "\\b(ClientUiRoute|GoToUiRoute|PrepareFrontendRoute|RequestRouteAfterClear|RunReadyRouteRequest|routeAfterClear|readyRoute|frontendRoute|frontendEntry)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "\\b(StateNeedsScreen|HasVisibleUiScreen)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "GoToState[[:space:]]*[(][^;]*std::make_unique<|GoToState[[:space:]]*[(][^;]*,[^;]*std::unique_ptr<" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "GoToState[[:space:]]*[(][[:space:]]*(GameState::)?(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)\\b" \
  "$REPO_ROOT/clients/silencer/src/game" \
  --glob '!third_party/**'

fail_if_match \
  "\\bcase[[:space:]]+(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)[[:space:]]*:|\\bstate[[:space:]]*=[[:space:]]*(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)\\b|return[[:space:]]+\"(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)\"" \
  "$REPO_ROOT/clients/silencer/src/game" \
  --glob '!third_party/**'

fail_if_match \
  "\\b(GoToState|GoBack|RequestQuit|LeaveJoinedGame|PushScreen|PopScreen|ReplaceScreen)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/screen_context.h" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/screen_context.cpp"

fail_if_match \
  "friend class ScreenContext|GetScreenContext[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/game/game.h" \
  "$REPO_ROOT/clients/silencer/src/game/game.cpp"

fail_if_match \
  "friend class ScreenContext|Screen[[:space:]]*[*][[:space:]]*Push[[:space:]]*[(]|void[[:space:]]+(Pop|Replace)[[:space:]]*[(]" \
  "$REPO_ROOT/clients/silencer/src/game/ui/game_ui_pipeline.h" \
  "$REPO_ROOT/clients/silencer/src/game/ui/game_ui_pipeline.cpp"

fail_if_match \
  "\\bWithClientUiNavigation\\b|\\bWithNavigation[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "NavigationProviderValue[[:space:]]*[{][[:space:]]*(&ctx|ctxPtr|provider[.]ctx)|use_navigation[[:space:]]*[(][[:space:]]*NavigationProviderValue" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "AppProviderValue[[:space:]]*[{][[:space:]]*&ctx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "UpdateProviderValue[[:space:]]*[{][[:space:]]*&ctx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "MissionSummaryProviderValue[[:space:]]*[{][[:space:]]*&ctx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "OptionsProviderValue[[:space:]]*[{][[:space:]]*&ctx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "LobbyProviderValue[[:space:]]*[{][[:space:]]*&ctx|provider_[.]ctx|provider[.]ctx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "ctx[.]GoToState[[:space:]]*[(][[:space:]]*(GameState::)?(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!third_party/**'

fail_if_match \
  "ctx[.](GoToState|RequestQuit|LeaveJoinedGame|GoBack)[[:space:]]*[(]|ctx->(GoToState|RequestQuit|LeaveJoinedGame|GoBack)[[:space:]]*[(]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  --glob '!screen_context.*'

fail_if_match \
  "(ctx|ctxPtr|provider_[.]ctx)->(PushScreen|PopScreen|ReplaceScreen|ShowMessage)|ctx[.](PushScreen|PopScreen|ReplaceScreen|ShowMessage)" \
  "$REPO_ROOT/clients/silencer/src/client/ui/providers" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hooks" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  --glob '!navigation_provider.cpp'

fail_if_match \
  "Config::GetInstance[[:space:]]*[(][[:space:]]*[)][.](Save|Load)|CycleKeybindPreset|ForkActiveProfileIfBuiltin|LoadActiveKeymap|WriteBinding|SDL_SetWindowFullscreen|SetScaleFilter|Audio::GetInstance[[:space:]]*[(][[:space:]]*[)][.](ResumeMusic|PauseMusic)|cfg[.](music|fullscreen|scalefilter)[[:space:]]*=" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_audio_screen.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_display_screen.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_controls_screen.cpp"

fail_if_match \
  "ctx[.]world" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_controls_screen.cpp"

fail_if_match \
  "ctx[.]updater|#include[[:space:]]*[<\"]updater(stage2)?[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/update/update_screen.cpp"

fail_if_match \
  "ctx[.](world|lobby|updater|ambienceMixer)|#include[[:space:]]*[<\"](lobby|updater|ambience_mixer|config|world)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby_connect/lobby_connect_screen.cpp"

fail_if_match \
  "ctx[.]lobby|#include[[:space:]]*[<\"](lobby|team)[.]h[>\"]|Lobby::|Team::" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/character_create/character_create_screen.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/character_create/character_create_layout.cpp"

fail_if_match \
  "\\b(World|Resources|Lobby|User|Config)::|#include[[:space:]]*[<\"](world|lobby|resources|user|config)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/character_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/character_panel.h"

fail_if_match \
  "\\b(World|Resources|Lobby|LobbyGame)::|#include[[:space:]]*[<\"](world|lobby|lobbygame|resources)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/chat_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/chat_panel.h"

fail_if_match \
  "\\b(World|Resources|Lobby|LobbyGame|User|Config)::|#include[[:space:]]*[<\"](world|lobby|lobbygame|resources|user|config)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_select_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_select_panel.h"

fail_if_match \
  "\\b(World|Resources|Lobby|LobbyGame|User|Team)::|#include[[:space:]]*[<\"](world|lobby|lobbygame|resources|user|team)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_join_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_join_panel.h"

fail_if_match \
  "\\b(World|Resources|Lobby|LobbyGame|Team|Peer|User|BuyableItem|Config)::|#include[[:space:]]*[<\"](world|lobby|lobbygame|team|peer|user|buyableitem|config|resources)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_tech_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_tech_panel.h"

fail_if_match \
  "ctx[.](world|mapDownloader|ambienceMixer)|\\b(World|MapDownloader|LobbyGame|Peer)[[:space:]*&]+|Config::|Lobby::|#include[[:space:]]*[<\"](world|lobby|lobbygame|map_downloader|ambience_mixer|peer|config)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_screen.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_screen.h"

fail_if_match \
  "ctx[.](world|mapDownloader|ambienceMixer)|\\b(World|Resources|MapDownloader|LobbyGame|Peer)[[:space:]*&]+|Config::|Lobby::|#include[[:space:]]*[<\"](world|resources|lobby|lobbygame|map_downloader|ambience_mixer|peer|config)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_chrome_frame.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_chrome_frame.hx"

fail_if_match \
  "ctx[.](world|mapDownloader|game)|\\b(World|Lobby|LobbyGame|Game|MapDownloader|Config)::|#include[[:space:]]*[<\"](world|lobby|lobbygame|game|config|map_downloader|mapfetch|os)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel.h"

fail_if_match \
  "ctx[.](world|mapDownloader|lobby|ambienceMixer)|\\b(World|Resources|Lobby|LobbyGame|MapDownloader|Config)::|#include[[:space:]]*[<\"](world|resources|lobby|lobbygame|config|map_downloader|audio|gasloader)[.]h[>\"]|Audio::GetInstance|GASLoader" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel_options.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/game_create_panel_map_form.cpp"

fail_if_match \
  "ctx[.]world|#include[[:space:]]*[<\"](world|lobby|user|stats)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/mission_summary/mission_summary_screen.cpp"

fail_if_match \
  "ctx[.]world|#include[[:space:]]*[<\"](world|game)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_screen.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals/message_modal.cpp"

fail_if_match \
  "ctx[.]world|\\bResources\\b|#include[[:space:]]*[<\"](world|resources)[.]h[>\"]" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/main_menu/main_menu_screen.cpp"

fail_if_match \
  '#include "(character_create_screen|lobby_connect_screen|main_menu_screen|mission_summary_screen|options_audio_screen|options_controls_screen|options_display_screen|options_screen|update_screen|lobby_screen|message_modal)[.]h"|std::make_unique<(MainMenuScreen|LobbyConnectScreen|LobbyScreen|CharacterCreateScreen|UpdateScreen|MissionSummaryScreen|OptionsScreen|OptionsControlsScreen|OptionsDisplayScreen|OptionsAudioScreen)|dynamic_cast<(LobbyScreen|MessageModal)' \
  "$REPO_ROOT/clients/silencer/src/game/ui/game_ui_pipeline.cpp"

fail_if_match \
  "\\b(World|Player)[[:space:]*&]+[A-Za-z_]|UiInteractionRegistry" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hooks/use_match.h"

fail_if_match \
  "\\bsprite_(bounds|frame)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hooks/use_app.h"

fail_if_match \
  "TopProgressModal|TopScreenIsOverlay|TopIsOverlay|HasTopLobbyScreen|ShowLobbyPanel" \
  "$REPO_ROOT/clients/silencer/src/game/game.cpp" \
  "$REPO_ROOT/clients/silencer/src/game/game.h" \
  "$REPO_ROOT/clients/silencer/src/game/ui/game_ui_pipeline.cpp" \
  "$REPO_ROOT/clients/silencer/src/game/ui/game_ui_pipeline.h"

fail_if_match \
  "Handle(TextInput|KeyPress|ScancodeDown|MousePress|MouseMove)|DispatchKeyPress|DispatchPreparedUiNavActions|UiNavActionToAscii|DispatchChatKey|HandleInGameMenuKey" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "DispatchInGameUiInput|Clay_OnHover" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "rawKeyDownCodes|CaptureRawKeyDown|BuildUiInputState|QueueUiTextInput|QueueUiNavAction|AddUiRawKeyDown|AddUiWheelDelta|QueueUiPointerWindowEvent|DispatchInGameUiActions|ConfigureInGameUiForControl" \
  "$REPO_ROOT/clients/silencer/src" \
  "$REPO_ROOT/tests/ui_architecture_test.cpp" \
  --glob '!third_party/**'

fail_if_match \
  "UiAutomation|ActiveUiInteractionRegistry|automation::" \
  "$REPO_ROOT/clients/silencer/src" \
  "$REPO_ROOT/tests/ui_architecture_test.cpp" \
  --glob '!third_party/**'

fail_if_match \
  "wait_for_state[[:space:]]+--state[[:space:]]+(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO)\\b" \
  "$REPO_ROOT/tests/cli-agent/e2e" \
  "$REPO_ROOT/shared/skills" \
  --glob '!60_ui_architecture_boundaries.sh'

fail_if_match \
  "\\bHandleUiAction\\b|\\bhandle_ui_actions\\b|\\bHandleUiActions\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui"

fail_if_match \
  "\\bonClick\\b|\\bonClickRow\\b|\\bonEnter\\b|clickUser|enterUser|rowIndex|textBuffer|textBufferLen|DispatchAction|DispatchActions|DispatchUiActions|QueueClick|QueueRowSelect|QueueTextEnter|Notify[A-Za-z]*Clicked" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  "$REPO_ROOT/clients/silencer/src/ui/primitives" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/net/controldispatch.cpp" \
  "$REPO_ROOT/clients/silencer/src/render/clay_ui_tests" \
  "$REPO_ROOT/tests/ui_architecture_test.cpp" \
  --glob '!third_party/**'

echo "PASS 60_ui_architecture_boundaries"
