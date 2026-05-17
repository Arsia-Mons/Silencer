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

command -v cmake >/dev/null 2>&1 || fail "cmake is not on PATH"

preset="win-ninja"
if [ "${1:-}" ] && [ "${1#-}" = "${1:-}" ]; then preset="$1"; fi
clean=0
for a in "$@"; do [ "$a" = "--clean" ] && clean=1; done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "$preset" in
    win-ninja)         bdir="$script_dir/build";         btype=Debug;   unity=OFF ;;
    win-ninja-release) bdir="$script_dir/build-release"; btype=Release; unity=OFF ;;
    win-ninja-unity)   bdir="$script_dir/build-unity";   btype=Release; unity=ON  ;;
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
if pgrep -f "$bdir/.*[Ss]ilencer" >/dev/null 2>&1; then
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
command -v ninja >/dev/null 2>&1 && gen=(-G Ninja)

echo "build.sh: $preset -> $bdir"
if [ "${#toolchain[@]}" -gt 0 ]; then
    echo "build.sh: using vcpkg toolchain from VCPKG_ROOT=$VCPKG_ROOT"
else
    echo "build.sh: VCPKG_ROOT not set; using system CMake/package discovery"
fi
cmake -S "$script_dir" -B "$bdir" "${gen[@]}" \
    -DCMAKE_BUILD_TYPE="$btype" \
    -DSILENCER_UNITY_BUILD="$unity" \
    ${toolchain+"${toolchain[@]}"}
cmake --build "$bdir" --parallel
echo "build.sh: OK ($preset)"
