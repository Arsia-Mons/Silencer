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

fail_if_match \
  "\\b(currentinterface|ProcessInGameInterfaces|Interface \\*|new Interface|ui/components|ui/modals|ui/panels|ui/screens)\\b" \
  "$REPO_ROOT/clients/silencer/src" \
  --glob '!third_party/**'

fail_if_match \
  '#include "(game|world|player|lobby|lobbygame|team|buyableitem|weapon|renderer|surface)[.]h"' \
  "$REPO_ROOT/clients/silencer/src/ui" \
  --glob '!third_party/**'

if rg -n "SDL_GetMouseState|Clay_SetPointerState\\(\\{ mx" \
  "$REPO_ROOT/clients/silencer/src/client/ui" \
  --glob '!**/screen_context.cpp'; then
  echo "screen UI must use ScreenContext::BeginClayFrame for native pointer setup" >&2
  exit 1
fi

fail_if_match \
  "Draw(BuyTech|Chat)OverlayClay|InGame(BuyTech|Chat)Root" \
  "$REPO_ROOT/clients/silencer/src/render"

echo "PASS 60_ui_architecture_boundaries"
