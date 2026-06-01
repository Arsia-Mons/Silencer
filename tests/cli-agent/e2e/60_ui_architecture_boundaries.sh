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

fail_if_path_exists "clients/silencer/src/ui/modals"
fail_if_path_exists "clients/silencer/src/ui/panels"
fail_if_path_exists "clients/silencer/src/ui/screens"
fail_if_path_exists "clients/silencer/src/ui/clay"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.h"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.cpp"

fail_if_match \
  "\\b(currentinterface|ProcessInGameInterfaces|Interface \\*|new Interface)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "clay_inspector" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  'nlohmann::json|#include[[:space:]]*[<"]nlohmann/json[.]hpp[>"]' \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  --glob '!third_party/**'

fail_if_match \
  'nlohmann::json[[:space:]]+(Game::)?GetWorldSummary|nlohmann::json[[:space:]]+(InGameUi::)?ConfigureForControl' \
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
  "\\bBuildUi[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui"

fail_if_match \
  "CLAY|Clay_|clay_bridge|#include[[:space:]]*[<\"](clay|primitives)/" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  --glob '!*.cppx' \
  --glob '!*.hx'

fail_if_match \
  "\\b[A-Za-z0-9_]*Controller\\b|\\bcontroller\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/game/ui" \
  "$REPO_ROOT/clients/silencer/CLAUDE.md" \
  "$REPO_ROOT/clients/silencer/src/ui/CLAUDE.md"

fail_if_match \
  "\\b(MVC|Presenter|presenter|ViewModel|viewModel|view_model)\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/game/ui" \
  "$REPO_ROOT/clients/silencer/CLAUDE.md" \
  "$REPO_ROOT/clients/silencer/src/ui/CLAUDE.md"

fail_if_match \
  "set_flag\\(|set_index\\(|focus_index\\(|copy_to\\(|copy_ui_text\\(|&state->" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby/lobby_view.cppx"

fail_if_match \
  "focus_index\\(|[.]focus_agent|[.]focus_agency|extern ::ReactContext CharacterCreateContext" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/character_create"

fail_if_match \
  "extern ::ReactContext" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '*.hx'

fail_if_match \
  "clay_bridge::(EnsureInitialized|Render)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals" \
  "$REPO_ROOT/clients/silencer/src/client/ui/hud"

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
  'main_menu_screen|lobby_connect_screen|character_create_screen|mission_summary_screen|options_(screen|audio_screen|controls_screen|display_screen)|update_screen|lobby_screen|std::make_unique<.*Screen|case[[:space:]]+(MAINMENU|LOBBYCONNECT|LOBBY|CREATECHARACTER|UPDATING|MISSIONSUMMARY|OPTIONS|OPTIONSCONTROLS|OPTIONSDISPLAY|OPTIONSAUDIO):|EnterScreenState|ShowScreenForState' \
  "$REPO_ROOT/clients/silencer/src/game/ui" \
  "$REPO_ROOT/clients/silencer/src/game/game.cpp" \
  "$REPO_ROOT/clients/silencer/src/game/game.h"

fail_if_match \
  'EnterScreenRoute|#include "game[.]h"|Game[[:space:]]*&|gameSession|DestroyAllObjects|GetAuthorityPeer|ClearGames|ForgetAllUserInfo|Disconnect[[:space:]]*\(' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/screen_routes.cpp" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/screen_routes.h"

fail_if_match \
  '::ui::UiElement[[:space:]]+(PrimaryActionButton|CancelButton)[[:space:]]*\(' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/update/update_view.cppx"

fail_if_match \
  '::ui::component[[:space:]]*\(' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/update/update_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_audio_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_display_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/options/options_controls_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals/message_modal_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/modals/password_modal_view.cppx" \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby_connect/lobby_connect_view.cppx"

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
  "\\bHandleUiAction\\b" \
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
