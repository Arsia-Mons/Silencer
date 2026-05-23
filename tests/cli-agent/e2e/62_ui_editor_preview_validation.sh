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
import { createDefaultUiDocument, findNode } from "./web/admin/lib/ui-layout.ts";

const document = createDefaultUiDocument();
const button = findNode(document.root, "MainMenuTutorialButton");
if (process.env.MUTATOR === "invalid-viewport") {
  document.viewport.width = 10;
} else if (process.env.MUTATOR === "empty-surface") {
  document.surface = "";
} else if (process.env.MUTATOR === "empty-name") {
  document.root.children[0].name = "";
} else if (process.env.MUTATOR === "invalid-button-font") {
  button.style.font = "title";
} else if (process.env.MUTATOR === "invalid-button-height") {
  button.style.height = { mode: "fixed", value: 48 };
} else if (process.env.MUTATOR === "unknown-document-field") {
  document.debug = true;
} else if (process.env.MUTATOR === "unknown-node-field") {
  document.root.unsupportedLayoutMode = "grid";
} else if (process.env.MUTATOR === "unknown-size-field") {
  document.root.style.width.preferred = 640;
} else if (process.env.MUTATOR === "invalid-size-bounds") {
  document.root.style.width.min = 1;
  document.root.style.width.max = 0;
} else if (process.env.MUTATOR === "unsupported-input") {
  document.root.children.push({
    id: "UnsupportedInput",
    kind: "input",
    name: "Unsupported Input",
    style: {
      width: { mode: "fixed", value: 180 },
      height: { mode: "fixed", value: 24 },
    },
  });
} else if (process.env.MUTATOR === "fixed-small-button") {
  button.text = "OK";
  button.buttonVariant = "chrome";
  button.buttonSize = "auto";
  button.style.width = { mode: "fixed", value: 80 };
} else if (process.env.MUTATOR === "fixed-long-button") {
  button.text = "VERY LONG PREVIEW BUTTON LABEL";
  button.buttonVariant = "chrome";
  button.buttonSize = "auto";
  button.style.width = { mode: "fixed", value: 120 };
}
writeFileSync(process.env.OUT, JSON.stringify(document));
'
}

make_surface_doc() {
  local out="$1" surface="$2"
  SURFACE="$surface" OUT="$out" bun -e '
import { writeFileSync } from "node:fs";
import { createDefaultUiDocument } from "./web/admin/lib/ui-layout.ts";

writeFileSync(process.env.OUT, JSON.stringify(createDefaultUiDocument(process.env.SURFACE)));
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

expect_surface_preview_component() {
  local surface="$1" label="$2" component_id="$3" expected_width="$4"
  local path="$TMP_DIR/$surface.json"
  local png="$TMP_DIR/$surface.png"
  local log="$TMP_DIR/$surface.out"
  make_surface_doc "$path" "$surface"
  cli --port "$PORT" ui_editor_preview_capture --document "$(cat "$path")" --out "$png" >"$log"
  test -s "$png"
  LOG="$log" LABEL="$label" COMPONENT_ID="$component_id" EXPECTED_WIDTH="$expected_width" bun -e '
const result = JSON.parse(await Bun.file(process.env.LOG).text());
const widgets = result.inspect?.widgets ?? [];
const elements = result.inspect?.elements ?? [];
if (!widgets.some((candidate) =>
  candidate.kind === "button" && candidate.label === process.env.LABEL
)) {
  console.error(`button ${process.env.LABEL} missing in ${process.env.LOG}`);
  process.exit(1);
}
const component = elements.find((candidate) => candidate.id === process.env.COMPONENT_ID);
if (!component) {
  console.error(`component ${process.env.COMPONENT_ID} missing in ${process.env.LOG}`);
  process.exit(1);
}
const expectedWidth = Number(process.env.EXPECTED_WIDTH);
if (component.w !== expectedWidth) {
  console.error(`component ${process.env.COMPONENT_ID} width ${component.w} !== ${expectedWidth}`);
  process.exit(1);
}
'
}

expect_button_width() {
  local name="$1" mutator="$2" label="$3" expected="$4"
  local path="$TMP_DIR/$name.json"
  local png="$TMP_DIR/$name.png"
  local log="$TMP_DIR/$name.json.out"
  make_doc "$path" "$mutator"
  cli --port "$PORT" ui_editor_preview_capture --document "$(cat "$path")" --out "$png" >"$log"
  test -s "$png"
  LOG="$log" LABEL="$label" EXPECTED="$expected" bun -e '
const result = JSON.parse(await Bun.file(process.env.LOG).text());
const widgets = result.inspect?.widgets ?? [];
const widget = widgets.find((candidate) =>
  candidate.kind === "button" && candidate.label === process.env.LABEL
);
if (!widget) {
  console.error(`button ${process.env.LABEL} missing in ${process.env.LOG}`);
  process.exit(1);
}
const expected = Number(process.env.EXPECTED);
if (widget.w !== expected) {
  console.error(`button ${process.env.LABEL} width ${widget.w} !== ${expected}`);
  process.exit(1);
}
'
}

valid_doc="$TMP_DIR/valid.json"
valid_png="$TMP_DIR/valid.png"
make_doc "$valid_doc" "valid"
cli --port "$PORT" ui_editor_preview_capture --document "$(cat "$valid_doc")" --out "$valid_png" >/dev/null
test -s "$valid_png"

expect_button_width "fixed-small-button" "fixed-small-button" "OK" "80"
expect_button_width "fixed-long-button" "fixed-long-button" "VERY LONG PREVIEW BUTTON LABEL" "120"
expect_surface_preview_component "options-display" "Smooth Scaling" "OptionsDisplaySmoothScalingIndicator" "50"
expect_surface_preview_component "options-audio" "Music" "OptionsAudioMusicIndicator" "50"

expect_bad_document "invalid-viewport" "invalid-viewport"
expect_bad_document "empty-surface" "empty-surface"
expect_bad_document "empty-name" "empty-name"
expect_bad_document "invalid-button-font" "invalid-button-font"
expect_bad_document "invalid-button-height" "invalid-button-height"
expect_bad_document "unknown-document-field" "unknown-document-field"
expect_bad_document "unknown-node-field" "unknown-node-field"
expect_bad_document "unknown-size-field" "unknown-size-field"
expect_bad_document "invalid-size-bounds" "invalid-size-bounds"
expect_bad_document "unsupported-input" "unsupported-input"

echo "PASS 62_ui_editor_preview_validation"
