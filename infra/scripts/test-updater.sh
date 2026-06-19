#!/usr/bin/env bash
# Automated end-to-end auto-updater test (headless; runs in CI).
#
# Proves the FULL self-update path: an installed client downloads a new build
# over loopback HTTP, verifies its sha256, self-replaces in place via stage-2,
# and the NEW version relaunches AUTOMATICALLY — confirmed by pinging the
# auto-relaunched process over the control socket and asserting its version.
#
# It drives the REAL cppx "Update" button headlessly (the retained UI renders
# and is clickable headless), so the STAGING -> PumpStage2 -> stage-2 handoff
# that regressed in #301 is exercised exactly. The lobby is bypassed
# (show_update_screen injects a real url+sha256) so the test is uniform across
# macOS + Linux — the lobby's update manifest has no Linux platform.
#
# Observability relies on env fallbacks the binary honors when a flag is
# absent: stage-2 relaunches the new client with NO argv (production does too),
# but the environment IS inherited across the exec, so we set SILENCER_HEADLESS
# + SILENCER_CONTROL_PORT on the OLD launch and the relaunched NEW client comes
# up headless on its own control port. The OLD client drives on a separate
# flag-port; the relaunched NEW client answers on the env-port.
#
# Manual failure-case probing (still works):
#   corrupt sha:   flip a hex digit in the --sha256 arg below
#   http 404:      rm the served zip before the client downloads
#
# Env knobs (all optional):
#   OLD_VER (default 00023)  NEW_VER (default 99999)  — distinct so ping.version
#                                                       is an unambiguous signal
#   OLD_BUILD_DIR / NEW_BUILD_DIR — reuse a prebuilt client dir (skip the build)
#   EXTRA_LIB_DIRS — colon-separated dirs added to the dynamic-loader path and
#                    exported (inherited by the relaunched client); use in CI to
#                    point at the SDL install when it isn't on the default path.
#   KEEP_WORK=1    — leave the scratch dir for inspection
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OLD_VER="${OLD_VER:-00023}"
NEW_VER="${NEW_VER:-99999}"
ZIP_NAME="silencer-update.zip"

# macOS only. Windows runs infra/scripts/test-updater.ps1. Linux is excluded
# on purpose: it isn't a shipped self-update platform (the lobby update
# manifest has no Linux URL), its stage-2 code is the union of the macOS
# POSIX-rename swap + the Windows minizip extract (no Linux-unique path), and
# its asset paths are cwd-relative (GetResDir()=="" for a portable build) —
# which an in-place self-replace + CleanupPreviousUpdate() actively breaks,
# whereas macOS/Windows resolve assets from the executable path.
case "$(uname)" in
  Darwin) OS=mac ;;
  *) echo "test-updater.sh: macOS only (Windows: test-updater.ps1; Linux: intentionally excluded)" >&2; exit 1 ;;
esac

ncpu()      { if command -v nproc >/dev/null 2>&1; then nproc; else sysctl -n hw.ncpu; fi; }
sha256_of() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; else shasum -a 256 "$1" | awk '{print $1}'; fi; }
pick_port() { bun -e 'const s=Bun.listen({hostname:"127.0.0.1",port:0,socket:{data(){}}});console.log(s.port);s.stop();'; }
cli()       { bun "$REPO_ROOT/clients/cli/index.ts" "$@"; }

# version field out of a `ping` reply (empty if the socket isn't up yet).
ping_version() {
  cli --port "$1" ping 2>/dev/null | bun -e \
    'try{const j=JSON.parse((await new Response(Bun.stdin.stream()).text())||"{}");process.stdout.write(j.version||"")}catch{}' \
    2>/dev/null || true
}

# Poll inspect until a node with the given control_id/label is present.
wait_for_label() {
  local port="$1" label="$2" tries="${3:-150}"
  for _ in $(seq 1 "$tries"); do
    local found
    found=$(cli --port "$port" inspect 2>/dev/null | LABEL="$label" bun -e \
      'try{const r=JSON.parse(await new Response(Bun.stdin.stream()).text());const l=process.env.LABEL;
        console.log((r.nodes||[]).some(w=>w.label===l||w.control_id===l)?"yes":"no")}catch{console.log("no")}' \
      2>/dev/null || echo no)
    [ "$found" = yes ] && return 0
    sleep 0.1
  done
  return 1
}

build_version() {  # <dir> <version>
  local dir="$1" ver="$2"
  echo "=== building client $ver -> $dir ===" >&2
  # Release + unity match release.yml so sccache reuses its objects (only the
  # few version-embedding TUs miss); the sccache launcher is added only when
  # CI has it enabled, so local runs stay plain.
  local extra=(-DCMAKE_BUILD_TYPE=Release -DSILENCER_UNITY_BUILD=ON)
  if [ "${SCCACHE_GHA_ENABLED:-}" = "true" ] && command -v sccache >/dev/null 2>&1; then
    extra+=(-DCMAKE_C_COMPILER_LAUNCHER=sccache -DCMAKE_CXX_COMPILER_LAUNCHER=sccache)
  fi
  cmake -B "$dir" -S "$REPO_ROOT/clients/silencer" \
    -DSILENCER_VERSION="$ver" \
    -DSILENCER_LOBBY_HOST=127.0.0.1 -DSILENCER_LOBBY_PORT=15170 \
    "${extra[@]}" >/dev/null
  cmake --build "$dir" --config Release --target silencer -j"$(ncpu)" >/dev/null
}

# Lay a runnable .app bundle down at <dest> and echo its client binary path.
# The macOS bundle is self-contained (Contents/assets, Contents/Helpers/
# updater-stage-2) and resolves assets from the executable path, so it runs
# from any location — including after the in-place swap.
stage_install() {  # <build_dir> <dest_dir>
  local bd="$1" dest="$2"
  mkdir -p "$dest"
  ditto "$bd/Silencer.app" "$dest/Silencer.app"
  echo "$dest/Silencer.app/Contents/MacOS/Silencer"
}

# Package a staged install into the update zip the way release.yml does
# (ditto -ck --keepParent -> Silencer.app/ at the zip root), so stage-2's
# single-top-dir unwrap + atomic-rename path runs against the real zip shape.
package_zip() {  # <pkg_dir> <out_zip>
  local pkg="$1" out="$2"
  ( cd "$pkg" && ditto -ck --sequesterRsrc --keepParent Silencer.app "$out" )
}

OLD_BUILD_DIR="${OLD_BUILD_DIR:-$REPO_ROOT/e2e-build-old}"
NEW_BUILD_DIR="${NEW_BUILD_DIR:-$REPO_ROOT/e2e-build-new}"
[ -d "$OLD_BUILD_DIR" ] || build_version "$OLD_BUILD_DIR" "$OLD_VER"
[ -d "$NEW_BUILD_DIR" ] || build_version "$NEW_BUILD_DIR" "$NEW_VER"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/silencer-updater-e2e.XXXXXX")"
HOST_DIR="$WORK/host"        # served over HTTP
INSTALL_DIR="$WORK/install"  # OLD client runs here; stage-2 swaps it in place
mkdir -p "$HOST_DIR" "$INSTALL_DIR"

OLD_PID=""; HTTP_PID=""
cleanup() {
  [ -n "$CTRL_Q" ] && cli --port "$CTRL_Q" quit >/dev/null 2>&1 || true
  [ -n "$OLD_PID" ] && kill "$OLD_PID" >/dev/null 2>&1 || true
  [ -n "$HTTP_PID" ] && kill "$HTTP_PID" >/dev/null 2>&1 || true
  if [ "${KEEP_WORK:-0}" = 1 ]; then echo "kept scratch: $WORK" >&2; else rm -rf "$WORK"; fi
}
CTRL_Q=""
trap cleanup EXIT

# Add any extra lib dirs to the loader path AND export them so the relaunched
# client inherits them (it gets no argv, only the environment).
if [ -n "${EXTRA_LIB_DIRS:-}" ]; then
  export DYLD_LIBRARY_PATH="$EXTRA_LIB_DIRS${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
fi

echo "=== staging NEW ($NEW_VER) update package ==="
NEW_PKG="$WORK/new-pkg"; mkdir -p "$NEW_PKG"
stage_install "$NEW_BUILD_DIR" "$NEW_PKG" >/dev/null
package_zip "$NEW_PKG" "$HOST_DIR/$ZIP_NAME"
SHA="$(sha256_of "$HOST_DIR/$ZIP_NAME")"
echo "NEW zip: $HOST_DIR/$ZIP_NAME  sha256=$SHA"

echo "=== staging OLD ($OLD_VER) install (stage-2 will swap this in place) ==="
OLD_BIN="$(stage_install "$OLD_BUILD_DIR" "$INSTALL_DIR")"
echo "OLD binary: $OLD_BIN"

HTTP_PORT="$(pick_port)"
echo "=== HTTP server on :$HTTP_PORT serving $HOST_DIR ==="
( cd "$HOST_DIR" && python3 -m http.server "$HTTP_PORT" >/dev/null 2>&1 ) &
HTTP_PID=$!

CTRL_P="$(pick_port)"   # OLD client, driven by us (flag)
CTRL_Q="$(pick_port)"   # relaunched NEW client, observed by us (env)
URL="http://127.0.0.1:$HTTP_PORT/$ZIP_NAME"
OLD_LOG="$WORK/old.log"

echo "=== launching OLD client (drive-port=$CTRL_P, relaunch-env-port=$CTRL_Q) ==="
env SILENCER_HEADLESS=1 SILENCER_CONTROL_PORT="$CTRL_Q" \
  "$OLD_BIN" --headless --control-port "$CTRL_P" >"$OLD_LOG" 2>&1 &
OLD_PID=$!

# OLD is up?
for _ in $(seq 1 60); do [ -n "$(ping_version "$CTRL_P")" ] && break; sleep 0.5; done
OLD_PING="$(ping_version "$CTRL_P")"
[ -n "$OLD_PING" ] || { echo "FAIL: OLD client never answered ping" >&2; tail -20 "$OLD_LOG" >&2; exit 1; }
echo "OLD client up, version=$OLD_PING"
[ "$OLD_PING" != "$NEW_VER" ] || { echo "FAIL: OLD and NEW share version $NEW_VER; pick distinct OLD_VER/NEW_VER" >&2; exit 1; }

echo "=== driving the real update UI: present -> consent ==="
cli --port "$CTRL_P" show_update_screen --url "$URL" --sha256 "$SHA" >/dev/null
cli --port "$CTRL_P" wait_for_state --state UPDATING --timeout-ms 15000 >/dev/null
wait_for_label "$CTRL_P" UpdateConsent || { echo "FAIL: Update button never rendered" >&2; cli --port "$CTRL_P" inspect >&2 || true; exit 1; }
cli --port "$CTRL_P" click --label UpdateConsent >/dev/null
echo "clicked Update — download -> verify -> STAGING -> stage-2 -> relaunch in flight"

echo "=== waiting for OLD client to exit (stage-2 spawned) ==="
for _ in $(seq 1 120); do kill -0 "$OLD_PID" >/dev/null 2>&1 || break; sleep 0.5; done
if kill -0 "$OLD_PID" >/dev/null 2>&1; then
  echo "FAIL: OLD client did not exit — stage-2 handoff never happened" >&2
  tail -30 "$OLD_LOG" >&2; exit 1
fi
OLD_PID=""
echo "OLD client exited; awaiting auto-relaunched NEW client on :$CTRL_Q"

echo "=== polling relaunched client until it reports the NEW version ==="
FINAL=""
for _ in $(seq 1 120); do
  FINAL="$(ping_version "$CTRL_Q")"
  [ "$FINAL" = "$NEW_VER" ] && break
  sleep 0.5
done

STAGE2_LOG="${HOME:-/tmp}/Library/Logs/Silencer/update.log"
[ -f "$STAGE2_LOG" ] || STAGE2_LOG="/tmp/silencer-update.log"

if [ "$FINAL" = "$NEW_VER" ]; then
  echo
  echo "PASS test-updater: $OLD_PING auto-updated and relaunched as $FINAL"
  exit 0
fi

echo "FAIL: relaunched client never reported NEW version $NEW_VER (last ping: '${FINAL:-<none>}')" >&2
echo "--- OLD client log (tail) ---" >&2; tail -30 "$OLD_LOG" >&2 || true
echo "--- stage-2 log ($STAGE2_LOG) ---" >&2; tail -30 "$STAGE2_LOG" 2>/dev/null >&2 || true
exit 1
