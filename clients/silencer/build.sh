#!/usr/bin/env bash
# Canonical Silencer client build entry point (macOS/Linux).
#
# Mirror of build.ps1: all agents and humans build/configure the client
# ONLY through this script. It uses VCPKG_ROOT when provided, otherwise
# preserves the historical macOS/Linux CMake path that discovers deps from
# the host system. It also serializes against concurrent builds so parallel
# agents / an IDE can't corrupt the shared CMake cache by configuring at the
# same time.
#
# Usage:  ./build.sh [win-ninja|win-ninja-release|win-ninja-unity] [--clean]
#   default preset : win-ninja (Debug)
#   --clean        : wipe CMakeCache.txt + CMakeFiles (keep vcpkg_installed) first
set -euo pipefail

fail() { echo "build.sh: $*" >&2; exit 1; }

probe_bundle_writability() {
    local bundle="$1"
    local probe="$bundle/Contents/.silencer-write-test"
    [ -d "$bundle/Contents" ] || return 0
    if touch "$probe" 2>/dev/null; then
        rm -f "$probe"
        return 0
    fi
    return 1
}

command -v cmake >/dev/null 2>&1 || fail "cmake is not on PATH"

preset="win-ninja"
if [ "${1:-}" ] && [ "${1#-}" = "${1:-}" ]; then preset="$1"; fi
clean=0
for a in "$@"; do [ "$a" = "--clean" ] && clean=1; done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "$preset" in
    win-ninja)         bdir="$script_dir/build";         btype=Debug;   unity=OFF; tests=ON  ;;
    win-ninja-release) bdir="$script_dir/build-release"; btype=Release; unity=OFF; tests=OFF ;;
    win-ninja-unity)   bdir="$script_dir/build-unity";   btype=Release; unity=ON;  tests=OFF ;;
    *) fail "unknown preset '$preset' (use win-ninja | win-ninja-release | win-ninja-unity)" ;;
esac

# --- Optional vcpkg toolchain (macOS/Linux can also use system packages) ---
toolchain=()
if [ -n "${VCPKG_ROOT:-}" ]; then
    vcpkg_toolchain="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    [ -f "$vcpkg_toolchain" ] \
        || fail "VCPKG_ROOT=$VCPKG_ROOT has no scripts/buildsystems/vcpkg.cmake"
    toolchain=(-DCMAKE_TOOLCHAIN_FILE="$vcpkg_toolchain")
fi

# --- Abort if a build is actively running (idle IDE is fine) ---
for p in cmake ninja cc1plus clang; do
    if pgrep -x "$p" >/dev/null 2>&1; then
        fail "a build is already running ($p) — wait for it to finish; concurrent configures corrupt the CMake cache"
    fi
done

# --- Refuse if a previously-built instance is running (locks the link target) ---
running_silencer_pid=""
while IFS= read -r pid; do
    args="$(ps -p "$pid" -o args= 2>/dev/null || true)"
    exe="${args%% *}"
    case "$exe" in
        "$bdir"/*[Ss]ilencer*) running_silencer_pid="$pid"; break ;;
    esac
done < <(pgrep -f "$bdir/.*[Ss]ilencer" || true)
if [ -n "$running_silencer_pid" ]; then
    fail "a built Silencer instance from $bdir is running — it locks the link target; stop it, then rebuild"
fi

# --- Cooperative build lock (mkdir is atomic on POSIX) ---
mkdir -p "$bdir"
lock="$bdir/.silencer-build.lock"
if ! mkdir "$lock" 2>/dev/null; then
    owner="$(cat "$lock/pid" 2>/dev/null || true)"
    if [ -n "$owner" ] && kill -0 "$owner" 2>/dev/null; then
        fail "another build holds the lock (PID $owner) on $bdir — wait for it to finish"
    fi
    echo "build.sh: stale lock (PID ${owner:-?}) — reclaiming" >&2
    rm -rf "$lock"; mkdir "$lock"
fi
echo "$$" > "$lock/pid"
trap 'rm -rf "$lock"' EXIT

if [ "$clean" = 1 ]; then
    echo "build.sh: cleaning cache in $bdir (keeping vcpkg_installed)"
    rm -f "$bdir/CMakeCache.txt"
    rm -rf "$bdir/CMakeFiles"
fi

gen=()
requested_generator=""
if command -v ninja >/dev/null 2>&1; then
    requested_generator="Ninja"
    gen=(-G "$requested_generator")
fi

if [ -n "$requested_generator" ] && [ -f "$bdir/CMakeCache.txt" ]; then
    cached_generator="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$bdir/CMakeCache.txt" | head -n 1)"
    if [ -n "$cached_generator" ] && [ "$cached_generator" != "$requested_generator" ]; then
        fail "$bdir was previously configured with generator '$cached_generator', but this wrapper wants '$requested_generator'; rerun with --clean"
    fi
fi

bundle_dir="$bdir/Silencer.app"
if [ "$(uname -s)" = "Darwin" ] && ! probe_bundle_writability "$bundle_dir"; then
    if [ "$clean" = 1 ]; then
        echo "build.sh: removing stale app bundle in $bundle_dir (macOS blocked writes inside it)"
        rm -rf "$bundle_dir"
    else
        fail "existing app bundle $bundle_dir is not writable (macOS blocked Info.plist updates); rerun with --clean to rebuild it"
    fi
fi

echo "build.sh: $preset -> $bdir"
if [ "${#toolchain[@]}" -gt 0 ]; then
    echo "build.sh: using vcpkg toolchain from VCPKG_ROOT=$VCPKG_ROOT"
else
    echo "build.sh: VCPKG_ROOT not set; using system CMake/package discovery"
fi
cmake -S "$script_dir" -B "$bdir" "${gen[@]+"${gen[@]}"}" \
    -DCMAKE_BUILD_TYPE="$btype" \
    -DSILENCER_UNITY_BUILD="$unity" \
    -DSILENCER_BUILD_TESTS="$tests" \
    ${toolchain+"${toolchain[@]}"}
cmake --build "$bdir" --parallel
echo "build.sh: OK ($preset)"
