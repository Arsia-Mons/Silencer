#!/usr/bin/env bash
# Orchestrates the full visual-regression-journeys flow:
#  1. Verify pre-flight (pixdiff, ImageMagick, git state).
#  2. Build current branch client/lobby unless skipped.
#  3. Capture current branch into $CURRENT_DIR.
#  4. Create a clean baseline worktree at $BASELINE_REF.
#  5. Build the baseline client/lobby unless skipped.
#  6. Capture baseline into $BASE_DIR.
#  7. Build labeled side-by-side composites into $COMP_DIR.
#  8. DM the generated composites to Henry on Discord unless disabled.
#  9. Print a sorted regression summary on stdout.
#
# Configurable via env:
#   BASELINE_REF        — git ref to diff against (default: origin/main)
#   CURRENT_DIR         — capture dir for current branch (default: /tmp/journeys-current)
#   BASE_DIR            — capture dir for baseline ref (default: /tmp/journeys-baseline)
#   COMP_DIR            — composite output dir (default: /tmp/journeys-composite)
#   WORKTREE            — baseline worktree path (default: /tmp/silencer-baseline)
#   BUILD_CURRENT=0     — skip current client/lobby build
#   BUILD_BASELINE=0    — skip baseline client/lobby build
#   FETCH_BASELINE=0    — skip git fetch for BASELINE_REF
#   SKIP_CURRENT=1      — re-use existing $CURRENT_DIR
#   SKIP_BASELINE=1     — re-use existing $BASE_DIR (skips checkout+build entirely)
#   SEND_DISCORD=0      — skip Discord DM delivery
#   DISCORD_DM_SEND_SCRIPT — path to discord-dm send.ts
#   PIXDIFF_BIN         — path to pixdiff (default: tools/pixdiff/build/pixdiff)

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

BASELINE_REF="${BASELINE_REF:-origin/main}"
CURRENT_DIR="${CURRENT_DIR:-/tmp/journeys-current}"
BASE_DIR="${BASE_DIR:-/tmp/journeys-baseline}"
COMP_DIR="${COMP_DIR:-/tmp/journeys-composite}"
WORKTREE="${WORKTREE:-/tmp/silencer-baseline}"
PIXDIFF_BIN="${PIXDIFF_BIN:-tools/pixdiff/build/pixdiff}"
SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CURRENT_REF="$(git rev-parse --abbrev-ref HEAD)@$(git rev-parse --short HEAD)"

current_silencer_bin() {
  local newest_bin=""
  for candidate in \
    "clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
    "clients/silencer/build/silencer" \
    "clients/silencer/build/Silencer.exe" \
    "build/Silencer.app/Contents/MacOS/Silencer" \
    "build/silencer" \
    "build/Silencer.exe"; do
    if [ -x "$candidate" ] && { [ -z "$newest_bin" ] || [ "$candidate" -nt "$newest_bin" ]; }; then
      newest_bin="$candidate"
    fi
  done
  [ -n "$newest_bin" ] || return 1
  printf '%s\n' "$PWD/$newest_bin"
}

baseline_silencer_bin() {
  local root="$1"
  for candidate in \
    "$root/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" \
    "$root/clients/silencer/build/silencer" \
    "$root/clients/silencer/build/Silencer.exe" \
    "$root/build/Silencer.app/Contents/MacOS/Silencer" \
    "$root/build/silencer" \
    "$root/build/Silencer.exe"; do
    if [ -x "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

build_lobby() {
  local root="$1"
  (cd "$root/services/lobby" && go build -o lobby)
}

prepare_worktree() {
  if [ -d "$WORKTREE" ]; then
    if git -C "$WORKTREE" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      git worktree remove --force "$WORKTREE" >/dev/null 2>&1 || rm -rf "$WORKTREE"
    else
      echo "FAIL: WORKTREE exists but is not a git worktree: $WORKTREE" >&2
      exit 1
    fi
  fi
  git worktree add --detach "$WORKTREE" "$BASELINE_REF" >/dev/null
}

resolve_discord_send_script() {
  if [ -n "${DISCORD_DM_SEND_SCRIPT:-}" ] && [ -f "$DISCORD_DM_SEND_SCRIPT" ]; then
    printf '%s\n' "$DISCORD_DM_SEND_SCRIPT"
    return 0
  fi
  if [ -n "${CLAUDE_PLUGIN_ROOT:-}" ] && [ -f "$CLAUDE_PLUGIN_ROOT/skills/discord-dm/send.ts" ]; then
    printf '%s\n' "$CLAUDE_PLUGIN_ROOT/skills/discord-dm/send.ts"
    return 0
  fi
  if [ -f "/Users/hv/repos/hv-skills/discord-dm/skills/discord-dm/send.ts" ]; then
    printf '%s\n' "/Users/hv/repos/hv-skills/discord-dm/skills/discord-dm/send.ts"
    return 0
  fi
  return 1
}

send_discord_batches() {
  local send_script="$1"
  local delivery_log="$COMP_DIR/discord_delivery.txt"
  local max_bytes="${DISCORD_MAX_ATTACHMENT_BYTES:-23000000}"
  local max_files="${DISCORD_MAX_FILES_PER_MESSAGE:-9}"
  local -a files=()
  while IFS= read -r file; do
    files+=("$file")
  done < <(find "$COMP_DIR" -maxdepth 1 -type f -name '*.png' | sort)

  if [ "${#files[@]}" -eq 0 ]; then
    echo "FAIL: no composite PNGs to DM from $COMP_DIR" >&2
    exit 1
  fi

  : > "$delivery_log"
  local -a batch=()
  local batch_bytes=0
  local batch_index=1

  flush_batch() {
    [ "${#batch[@]}" -gt 0 ] || return 0
    local message
    message="Silencer cppx visual regression batch ${batch_index}: ${CURRENT_REF} vs ${BASELINE_REF}. Stitched comparison images attached."
    echo "  DM batch ${batch_index}: ${#batch[@]} files" | tee -a "$delivery_log"
    bun "$send_script" "$message" "${batch[@]}" 2>&1 | tee -a "$delivery_log"
    batch=()
    batch_bytes=0
    batch_index=$((batch_index + 1))
  }

  local file size
  for file in "${files[@]}"; do
    size="$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file")"
    if [ "${#batch[@]}" -gt 0 ] && {
      [ $((batch_bytes + size)) -gt "$max_bytes" ] || [ "${#batch[@]}" -ge "$max_files" ];
    }; then
      flush_batch
    fi
    batch+=("$file")
    batch_bytes=$((batch_bytes + size))
  done
  flush_batch
}

# ---------- 1. Pre-flight ----------
echo ">>> Pre-flight"

if [ "${FETCH_BASELINE:-1}" = "1" ]; then
  git fetch origin main >/dev/null
fi

if [ ! -x "$PIXDIFF_BIN" ]; then
  echo "FAIL: $PIXDIFF_BIN missing. Build with:" >&2
  echo "  cmake -B tools/pixdiff/build -S tools/pixdiff && cmake --build tools/pixdiff/build" >&2
  exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "FAIL: ImageMagick 'magick' not on PATH. Install with: brew install imagemagick" >&2
  exit 1
fi

if ! command -v rg >/dev/null 2>&1; then
  echo "WARN: ripgrep not installed; some E2E side-effects (boundary checks) will silently no-op." >&2
fi

echo "  OK"

# ---------- 2. Build current ----------
if [ "${SKIP_CURRENT:-0}" != "1" ] && [ "${BUILD_CURRENT:-1}" = "1" ]; then
  echo ">>> Building current client"
  clients/silencer/build.sh
  echo ">>> Building current lobby"
  build_lobby "$PWD"
fi

if ! CURRENT_BIN="$(current_silencer_bin)"; then
  echo "FAIL: current branch silencer binary missing. Run 'clients/silencer/build.sh' first." >&2
  exit 1
fi

# ---------- 3. Capture current ----------
if [ "${SKIP_CURRENT:-0}" = "1" ] && [ -d "$CURRENT_DIR" ]; then
  echo ">>> Reusing existing $CURRENT_DIR ($(ls "$CURRENT_DIR" | wc -l | tr -d ' ') files)"
else
  echo ">>> Capturing current branch -> $CURRENT_DIR"
  rm -rf "$CURRENT_DIR"
  SILENCER_BIN="$CURRENT_BIN" OUT_DIR="$CURRENT_DIR" bash "$SKILL_DIR/capture_current.sh"
fi

# ---------- 4-6. Baseline ----------
if [ "${SKIP_BASELINE:-0}" = "1" ] && [ -d "$BASE_DIR" ]; then
  echo ">>> Reusing existing $BASE_DIR ($(ls "$BASE_DIR" | wc -l | tr -d ' ') files)"
else
  echo ">>> Preparing clean baseline worktree at $WORKTREE (ref $BASELINE_REF)"
  prepare_worktree

  if [ "${BUILD_BASELINE:-1}" = "1" ]; then
    echo ">>> Building baseline client"
    (cd "$WORKTREE" && clients/silencer/build.sh)
    echo ">>> Building baseline lobby"
    build_lobby "$WORKTREE"
  fi

  if ! BASELINE_BIN="$(baseline_silencer_bin "$WORKTREE")"; then
    echo "FAIL: baseline silencer binary missing after build in $WORKTREE" >&2
    exit 1
  fi

  echo ">>> Capturing baseline -> $BASE_DIR"
  rm -rf "$BASE_DIR"
  SILENCER_BIN="$BASELINE_BIN" WORKTREE="$WORKTREE" OUT_DIR="$BASE_DIR" bash "$SKILL_DIR/capture_baseline.sh"
fi

# ---------- 7. Composites + summary ----------
echo ">>> Building composites -> $COMP_DIR"
rm -rf "$COMP_DIR"
mkdir -p "$COMP_DIR"
BASE_DIR="$BASE_DIR" CURRENT_DIR="$CURRENT_DIR" OUT_DIR="$COMP_DIR" PIXDIFF_BIN="$PIXDIFF_BIN" \
  BASELINE_LABEL="$BASELINE_REF" CURRENT_LABEL="$CURRENT_REF" \
  bash "$SKILL_DIR/build_composites.sh" | tee "$COMP_DIR/summary.txt"

cat > "$COMP_DIR/manifest.txt" <<EOF
Silencer cppx visual regression
current:  $CURRENT_REF
baseline: $BASELINE_REF
current captures:  $CURRENT_DIR
baseline captures: $BASE_DIR
composites: $COMP_DIR
EOF

# ---------- 8. Discord delivery ----------
if [ "${SEND_DISCORD:-1}" = "1" ]; then
  echo ">>> Sending composites to Discord DM"
  if ! SEND_SCRIPT="$(resolve_discord_send_script)"; then
    echo "FAIL: Discord DM send script not found. Set DISCORD_DM_SEND_SCRIPT or CLAUDE_PLUGIN_ROOT." >&2
    exit 1
  fi
  send_discord_batches "$SEND_SCRIPT"
else
  echo ">>> Skipping Discord DM delivery (SEND_DISCORD=0)"
fi

echo ""
echo ">>> Visual regression run complete."
echo "    composites: $COMP_DIR"
echo "    baseline captures: $BASE_DIR"
echo "    current captures:  $CURRENT_DIR"
echo ""
echo "    Eyeball every composite. pixdiff alone is not a verdict — fade-in"
echo "    screens and animated logos can hit 40%+ without a real regression,"
echo "    and a real text-garble bug can hit <40%. The composites make both"
echo "    visible at a glance."
