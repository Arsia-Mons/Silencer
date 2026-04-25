#!/usr/bin/env bash
# Bypass CI: rsync current src to silencer, build the ARM64 dedicated server
# binary there, swap it into /opt/zsilencer/current, restart the service.
# For debug iterations only — prod should go through .github/workflows/deploy.yml.
#
# NOTE: terraform's cloud-init still parameterizes /opt/zsilencer/* and the
# zsilencer-lobby systemd unit; the C++ binary on disk is now `silencer`,
# but the unit's ExecStart still references the old `current/zsilencer`
# path. Run a unit-file fix (or a terraform reapply) before exercising
# this script against an existing host.
set -euo pipefail

HOST="${HOST:-silencer}"
REMOTE="/home/ubuntu/silencer-src"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

echo "==> rsync source to $HOST:$REMOTE"
rsync -az --delete \
  --exclude=build --exclude=.git --exclude=terraform \
  --exclude=.github --exclude='*.o' --exclude='*.zip' \
  "$REPO/" "ubuntu@$HOST:$REMOTE/"

echo "==> build on $HOST"
ssh "ubuntu@$HOST" "cd $REMOTE && mkdir -p build && cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release -DSILENCER_LOBBY_HOST=silencer.hventura.com ../clients/silencer > /tmp/cmake.log 2>&1 && \
  make -j\$(nproc) silencer 2>&1 | tail -3"

echo "==> swap binary and restart lobby"
ssh "ubuntu@$HOST" "sudo cp $REMOTE/build/silencer /opt/zsilencer/current/silencer && \
  sudo chmod +x /opt/zsilencer/current/silencer && \
  sudo systemctl restart zsilencer-lobby && \
  sleep 2 && systemctl is-active zsilencer-lobby"
