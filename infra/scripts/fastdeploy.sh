#!/usr/bin/env bash
# Bypass CI: rsync current src to silencer, build the ARM64 dedicated server
# binary there, swap it into /opt/silencer/current, restart the service.
# For debug iterations only — prod should go through .github/workflows/deploy.yml.
set -euo pipefail

HOST="${HOST:-silencer}"
REMOTE="/home/ubuntu/silencer-src"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

echo "==> rsync source to $HOST:$REMOTE"
rsync -az --delete \
  --exclude=build --exclude=.git --exclude=infra \
  --exclude=.github --exclude='*.o' --exclude='*.zip' \
  "$REPO/" "ubuntu@$HOST:$REMOTE/"

echo "==> build on $HOST"
ssh "ubuntu@$HOST" "cd $REMOTE && mkdir -p build && cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release -DSILENCER_LOBBY_HOST=lobby.arsiamons.com ../clients/silencer > /tmp/cmake.log 2>&1 && \
  make -j\$(nproc) silencer 2>&1 | tail -3"

echo "==> swap binary and restart lobby"
ssh "ubuntu@$HOST" "sudo cp $REMOTE/build/silencer /opt/silencer/current/silencer && \
  sudo chmod +x /opt/silencer/current/silencer && \
  sudo systemctl restart silencer-lobby && \
  sleep 2 && systemctl is-active silencer-lobby"
