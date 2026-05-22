#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
TMP_DIR="$(mktemp -d)"
trap 'stop_silencer "$PID" "$PORT"; rm -rf "$TMP_DIR"' EXIT

wait_alive "$PORT"

make_doc() {
  local out="$1" mutator="$2"
  MUTATOR="$mutator" OUT="$out" bun -e '
import { writeFileSync } from "node:fs";
import { createDefaultUiDocument } from "./web/admin/lib/ui-layout.ts";

const document = createDefaultUiDocument();
if (process.env.MUTATOR === "invalid-viewport") {
  document.viewport.width = 10;
} else if (process.env.MUTATOR === "empty-surface") {
  document.surface = "";
} else if (process.env.MUTATOR === "empty-name") {
  document.root.children[0].name = "";
} else if (process.env.MUTATOR === "invalid-button-font") {
  document.root.children[1].children[0].style.font = "title";
} else if (process.env.MUTATOR === "invalid-button-height") {
  document.root.children[1].children[0].style.height = { mode: "fixed", value: 48 };
}
writeFileSync(process.env.OUT, JSON.stringify(document));
'
}

expect_bad_document() {
  local name="$1" mutator="$2"
  local path="$TMP_DIR/$name.json"
  local log="$TMP_DIR/$name.log"
  make_doc "$path" "$mutator"
  if cli --port "$PORT" ui_editor_preview_capture --document "$(cat "$path")" --out "$TMP_DIR/$name.png" >"$log" 2>&1; then
    echo "expected ui_editor_preview_capture to reject $name" >&2
    cat "$log" >&2
    exit 1
  fi
  if ! rg -q "BAD_REQUEST" "$log"; then
    echo "expected BAD_REQUEST for $name" >&2
    cat "$log" >&2
    exit 1
  fi
}

valid_doc="$TMP_DIR/valid.json"
valid_png="$TMP_DIR/valid.png"
make_doc "$valid_doc" "valid"
cli --port "$PORT" ui_editor_preview_capture --document "$(cat "$valid_doc")" --out "$valid_png" >/dev/null
test -s "$valid_png"

expect_bad_document "invalid-viewport" "invalid-viewport"
expect_bad_document "empty-surface" "empty-surface"
expect_bad_document "empty-name" "empty-name"
expect_bad_document "invalid-button-font" "invalid-button-font"
expect_bad_document "invalid-button-height" "invalid-button-height"

echo "PASS 62_ui_editor_preview_validation"
