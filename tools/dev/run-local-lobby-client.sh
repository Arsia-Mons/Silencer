#!/usr/bin/env bash
# Build and run a local Silencer lobby paired with the dev client.
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: tools/dev/run-local-lobby-client.sh [--preset win-ninja|win-ninja-release|win-ninja-unity] [--lobby-port PORT]

Builds the interactive client + a headless dedicated server (the lobby spawns
the latter via -game-binary), builds the Go lobby, starts the lobby on
127.0.0.1:15170 by default, then launches the client pointed at that local lobby.
USAGE
}

fail() {
    echo "run-local-lobby-client.sh: $*" >&2
    exit 1
}

preset="win-ninja"
lobby_port="15170"
while [ "$#" -gt 0 ]; do
    case "$1" in
        --preset)
            [ "$#" -ge 2 ] || fail "--preset needs a value"
            preset="$2"
            shift 2
            ;;
        --lobby-port|--port)
            [ "$#" -ge 2 ] || fail "$1 needs a value"
            lobby_port="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument '$1'"
            ;;
    esac
done

case "$preset" in
    win-ninja|win-ninja-release|win-ninja-unity) ;;
    *) fail "unknown preset '$preset'" ;;
esac

command -v go >/dev/null 2>&1 || fail "go is not on PATH"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
client_dir="$repo_root/clients/silencer"
lobby_dir="$repo_root/services/lobby"
run_dir="$lobby_dir/run/local-dev"
maps_dir="$run_dir/maps"
lobby_bin="$run_dir/silencer-lobby"
lobby_db="$run_dir/lobby.json"
lobby_out="$run_dir/lobby.out.log"
lobby_err="$run_dir/lobby.err.log"
player_auth_port=$((lobby_port + 1))
map_api_port=$((lobby_port + 2))
lobby_pid=""

cleanup() {
    if [ -n "$lobby_pid" ] && kill -0 "$lobby_pid" 2>/dev/null; then
        echo "Stopping lobby..."
        kill "$lobby_pid" 2>/dev/null || true
        wait "$lobby_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

client_build_dir() {
    case "$preset" in
        win-ninja) echo "$client_dir/build" ;;
        win-ninja-release) echo "$client_dir/build-release" ;;
        win-ninja-unity) echo "$client_dir/build-unity" ;;
    esac
}

client_binary() {
    local bdir="$1"
    if [ -x "$bdir/Silencer.app/Contents/MacOS/Silencer" ]; then
        echo "$bdir/Silencer.app/Contents/MacOS/Silencer"
    elif [ -x "$bdir/silencer" ]; then
        echo "$bdir/silencer"
    elif [ -x "$bdir/Silencer.exe" ]; then
        echo "$bdir/Silencer.exe"
    else
        fail "client binary missing under $bdir after build"
    fi
}

wait_for_lobby() {
    local i
    for i in $(seq 1 80); do
        if (echo > "/dev/tcp/127.0.0.1/$lobby_port") >/dev/null 2>&1; then
            return 0
        fi
        if ! kill -0 "$lobby_pid" 2>/dev/null; then
            echo "Lobby exited before accepting connections." >&2
            tail -40 "$lobby_out" 2>/dev/null || true
            tail -40 "$lobby_err" 2>/dev/null || true
            exit 1
        fi
        sleep 0.25
    done
    fail "lobby did not open 127.0.0.1:$lobby_port"
}

mkdir -p "$run_dir" "$maps_dir"

echo "Building client ($preset)..."
"$client_dir/build.sh" "$preset"
client_bin="$(client_binary "$(client_build_dir)")"

# The lobby spawns the dedicated server as `silencer -s`; build it headless
# (no UI/SDL3_ttf), matching what deploy.yml ships to the prod lobby box.
echo "Building headless dedicated server (win-ninja-headless)..."
"$client_dir/build.sh" win-ninja-headless
server_bin="$(client_binary "$client_dir/build-headless")"

echo "Building lobby..."
(cd "$lobby_dir" && go build -o "$lobby_bin")

echo "Starting lobby on 127.0.0.1:$lobby_port..."
"$lobby_bin" \
    -addr ":$lobby_port" \
    -db "$lobby_db" \
    -game-binary "$server_bin" \
    -public-addr 127.0.0.1 \
    -update-manifest= \
    -player-auth-addr ":$player_auth_port" \
    -map-api-addr ":$map_api_port" \
    -maps-dir "$maps_dir" \
    >"$lobby_out" 2>"$lobby_err" &
lobby_pid=$!

wait_for_lobby
echo "Launching client against local lobby..."
echo "Lobby logs: $lobby_out / $lobby_err"
(cd "$repo_root" && "$client_bin" --lobby-host 127.0.0.1 --lobby-port "$lobby_port")
