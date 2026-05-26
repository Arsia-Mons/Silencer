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

fail_if_path_exists "clients/silencer/src/ui/components"
fail_if_path_exists "clients/silencer/src/ui/modals"
fail_if_path_exists "clients/silencer/src/ui/panels"
fail_if_path_exists "clients/silencer/src/ui/screens"
fail_if_path_exists "clients/silencer/src/ui/clay"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.h"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.cpp"

fail_if_match \
  "\\b(currentinterface|ProcessInGameInterfaces|Interface \\*|new Interface)\\b|(^|[^[:alnum:]_/])((clients/silencer/)?src/)?ui/(components|modals|panels|screens)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "\\bUiRenderCommand\\b|\\bCommands[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/game/ui" \
  "$REPO_ROOT/tests/ui_architecture_test.cpp" \
  --glob '!third_party/**'

fail_if_match \
  "std::vector[[:space:]]*<[^>]*UiAction" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/game/ui" \
  "$REPO_ROOT/clients/silencer/src/render/clay_ui_tests" \
  --glob '!third_party/**'

fail_if_match \
  "std::string[[:space:]]+(id|value)[[:space:]]*;" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime/UiActionQueue.h"

fail_if_match \
  "UiBoundedText\\(const std::string|operator=[[:space:]]*\\(const std::string" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime/UiActionQueue.h"

fail_if_match \
  "std::string[[:space:]]+InteractableId|InteractableId\\(const UiInteractable|FindInteractableById\\(const std::string|FocusInteractableById\\(const std::string" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime/UiInteractionRegistry.h" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime/UiInteractionRegistry.cpp"

fail_if_match \
  "\\breact_(init|begin_frame|end_frame|shutdown)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**' \
  --glob '!clients/silencer/src/ui/runtime/react.h' \
  --glob '!clients/silencer/src/ui/runtime/react.cpp' \
  --glob '!clients/silencer/src/ui/runtime/ClayService.cpp'

fail_if_match \
  "\\b(react_(enter|leave|next_child_index|make_instance_id|provider_push|provider_pop)|REACT_INSTANCE_ID(_KEY)?)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**' \
  --glob '!clients/silencer/src/ui/runtime/react.h' \
  --glob '!clients/silencer/src/ui/runtime/react.cpp'

fail_if_match \
  "\\bui_focus_(init|set_current|begin_frame|end_layout)[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**' \
  --glob '!clients/silencer/src/ui/focus/UiFocus.h' \
  --glob '!clients/silencer/src/ui/focus/UiFocus.cpp' \
  --glob '!clients/silencer/src/client/ui/ClientUi.cpp'

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
  '#include[[:space:]]*[<"]((audio|gasloader|world)[.]h)[>"]|Audio::|GASLoader::|soundUIClick' \
  "$REPO_ROOT/clients/silencer/src/client/ui/ClientUi.h" \
  "$REPO_ROOT/clients/silencer/src/client/ui/ClientUi.cpp"

fail_if_match \
  'ctx[.]game[.]UiInteractions[[:space:]]*[(]' \
  "$REPO_ROOT/clients/silencer/src/client/ui"

fail_if_match \
  '\bGetTopScreen[[:space:]]*[(]' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens" \
  --glob '!**/screen_context.cpp'

fail_if_match \
  'ctx[.]game' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby"

fail_if_match \
  '#include[[:space:]]*"game[.]h"' \
  "$REPO_ROOT/clients/silencer/src/client/ui/screens/lobby"

fail_if_match \
  'nlohmann::json[[:space:]]+(Game::)?GetWorldSummary|nlohmann::json[[:space:]]+(InGameUiController::)?ConfigureForControl' \
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
  "$REPO_ROOT/clients/silencer/src/client/ui/navigation/ScreenStack.cpp"

fail_if_match \
  "std::vector|push_back|reserve|capacity[[:space:]]*\\(|\\.erase[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui/navigation/ScreenStack.h" \
  "$REPO_ROOT/clients/silencer/src/client/ui/navigation/ScreenStack.cpp"

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
