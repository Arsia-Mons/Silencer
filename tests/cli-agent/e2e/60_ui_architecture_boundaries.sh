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

# src/ui/components is now the cppx generic-primitive layer (SIL-16); no longer banned.
fail_if_path_exists "clients/silencer/src/ui/modals"
fail_if_path_exists "clients/silencer/src/ui/panels"
fail_if_path_exists "clients/silencer/src/ui/screens"
fail_if_path_exists "clients/silencer/src/ui/clay"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.h"
fail_if_path_exists "clients/silencer/src/ui/runtime/clay_inspector.cpp"

# The legacy Clay layer lived at src/ui/{modals,panels,screens}; ban references
# to those exact paths. (The live cppx app-shell screens are a distinct, allowed
# location: src/client/ui/screens — golden's layout. SIL-18.)
fail_if_match \
  "\\b(currentinterface|ProcessInGameInterfaces|Interface \\*|new Interface|src/ui/modals|src/ui/panels|src/ui/screens)\\b" \
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
  'nlohmann::json[[:space:]]+(Game::)?GetWorldSummary|nlohmann::json[[:space:]]+(InGameUiController::)?ConfigureForControl' \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  '#include "(game|world|player|lobby|lobbygame|team|buyableitem|weapon|renderer|surface)[.]h"' \
  "$REPO_ROOT/clients/silencer/src/ui" \
  --glob '!third_party/**'

# SIL-20: the app-shell (providers/hooks/screens) reaches gameplay only through
# hook intent closures + POD snapshots assembled by the composition root
# (src/game/ui). It must never name a raw Game/World/Lobby — banning these
# includes structurally forbids a raw `Lobby*` (or game/world handle) in screen,
# hook, or provider code. The lobby snapshot is a POD in client::ui; the only
# place that touches the real Lobby is the game-layer composition root.
fail_if_match \
  '#include "(game|world|player|lobby|lobbygame|team|buyableitem|weapon|renderer|surface)[.]h"' \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
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

# src/ui (the golden retained runtime + styling substrate) is SDL-free by
# construction: it speaks the RGBA DrawCommand IR and the injected text-measure
# seam, never SDL directly. SIL-18 wires real input, but only the game/event
# layer touches SDL — the ui/ substrate must not gain an SDL include.
if rg -n '#include +[<"]SDL' \
  "$REPO_ROOT/clients/silencer/src/ui"; then
  echo "src/ui runtime/styling must stay SDL-free (no SDL includes)" >&2
  exit 1
fi

# SIL-15/22: the legacy Clay UI layer is deleted. Lock the paths out…
fail_if_path_exists "clients/silencer/third_party/clay"
fail_if_path_exists "clients/silencer/src/render/clay_ui_compositor.cpp"
fail_if_path_exists "clients/silencer/src/render/clay_ui_payloads.h"
fail_if_path_exists "clients/silencer/src/render/clay_ui_tests"
fail_if_path_exists "clients/silencer/src/ui/runtime/ClayService.h"
fail_if_path_exists "clients/silencer/src/ui/primitives"
fail_if_path_exists "clients/silencer/src/client/ui/ClientUi.h"
fail_if_path_exists "clients/silencer/src/client/ui/screens/screen_context.h"
fail_if_path_exists "clients/silencer/src/client/ui/modals"
fail_if_path_exists "clients/silencer/src/client/ui/hud"
# …and lock the Clay/ScreenContext vocabulary out of all source (incl. docs).
fail_if_match \
  "Clay_[A-Za-z]|ClayService|clay_bridge|clay_ui_compositor|CLAY[[:space:]]*\\(|\\bScreenContext\\b|UiInteractionRegistry" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "Draw[A-Za-z0-9_]*Clay|BuildInGameHudUi|BuildInGameOverlaysUi|client/ui/hud|Clay_BeginLayout|Clay_EndLayout|Clay_SetPointerState|clay_bridge" \
  "$REPO_ROOT/clients/silencer/src/render/renderer.cpp" \
  "$REPO_ROOT/clients/silencer/src/render/renderer.h"

fail_if_match \
  "CLAY_TEXT|[.]font(Id|Size)|fontBank|fontWidth|BankText|bank_text|TextCellWidthFor|TextHeightForBank|MeasureBankText|Renderer::DrawText|\\bDrawText[[:space:]]*\\(" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
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
  --glob '!third_party/**'

fail_if_match \
  "UiAutomation|ActiveUiInteractionRegistry|automation::" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  "\\bHandleUiAction\\b" \
  "$REPO_ROOT/clients/silencer/src/client/ui"

fail_if_match \
  "\\bonClick\\b|\\bonClickRow\\b|\\bonEnter\\b|clickUser|enterUser|rowIndex|textBuffer|textBufferLen|DispatchAction|DispatchActions|DispatchUiActions|QueueClick|QueueRowSelect|QueueTextEnter|Notify[A-Za-z]*Clicked" \
  "$REPO_ROOT/clients/silencer/src/ui/runtime" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  "$REPO_ROOT/clients/silencer/src/net/controldispatch.cpp" \
  --glob '!third_party/**'

echo "PASS 60_ui_architecture_boundaries"
