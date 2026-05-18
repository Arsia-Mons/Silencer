#!/usr/bin/env bash
# Deploy Silencer to a Steam Deck for sideload playtesting.
#
# Prerequisites on the Deck:
#   1. Settings → System → Enable Developer Mode → on
#   2. Settings → System → SSH → on (sets password for "deck" user)
#   3. Note the Deck's IP address (Settings → Network → ... → IP address)
#
# Usage:
#   ./deploy-deck.sh <deck-ip>          # deploy only
#   ./deploy-deck.sh <deck-ip> --run    # deploy and launch
#
# Build must be done first via Docker (see issue #185):
#   docker run --platform linux/amd64 --rm \
#     -v "$(pwd):/src" -v "$(pwd)/../../shared:/shared" \
#     silencer-linux-build \
#     bash -c "cmake -S /src -B /src/build-linux -G Ninja \
#       -DSDL3_DIR=/usr/local/lib/cmake/SDL3 \
#       -DSDL3_mixer_DIR=/usr/local/lib/cmake/SDL3_mixer && \
#       cmake --build /src/build-linux"
set -euo pipefail

DECK_IP="${1:?Usage: $0 <deck-ip> [--run]}"
RUN=0
for a in "$@"; do [ "$a" = "--run" ] && RUN=1; done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/build-linux/silencer"
ASSETS="$SCRIPT_DIR/../../shared/assets"
DECK_USER="deck"
DECK_DIR="/home/deck/silencer"

[ -f "$BINARY" ] || { echo "deploy-deck.sh: $BINARY not found — build first (see comment in script)"; exit 1; }

echo "deploy-deck.sh: deploying to $DECK_USER@$DECK_IP:$DECK_DIR"

# Create remote dir structure
ssh "$DECK_USER@$DECK_IP" "mkdir -p $DECK_DIR/assets"

# Copy binary
echo "  → binary"
scp "$BINARY" "$DECK_USER@$DECK_IP:$DECK_DIR/silencer"

# Copy shared assets (incremental — only changed files)
echo "  → assets"
rsync -az --exclude="CLAUDE.md" --exclude="AGENTS.md" \
    "$ASSETS/" "$DECK_USER@$DECK_IP:$DECK_DIR/assets/"

echo "deploy-deck.sh: done"
echo ""
echo "To run manually:"
echo "  ssh $DECK_USER@$DECK_IP"
echo "  cd $DECK_DIR && ./silencer"
echo ""

if [ "$RUN" = 1 ]; then
    echo "deploy-deck.sh: launching Silencer on Deck..."
    # DISPLAY/WAYLAND_DISPLAY picked up from the active session via systemd-run
    ssh "$DECK_USER@$DECK_IP" \
        "cd $DECK_DIR && DISPLAY=:0 ./silencer"
fi
