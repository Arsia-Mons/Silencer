#!/usr/bin/env bash
# install-linux-server.sh
# Run from the repo root on a fresh Ubuntu 22.04+ VM.
# Installs Docker (if needed) then builds and starts the Silencer stack
# (lobby + admin-api + admin-web).
#
# Usage:
#   sudo bash infra/scripts/install-linux-server.sh [PUBLIC_IP]
#
# PUBLIC_IP is the IP/hostname clients use to reach this server.
# If omitted, it is auto-detected from the network interface.

set -euo pipefail

# Must be run from the repo root (this script is two levels deep in
# infra/scripts/, so step up twice)
cd "$(dirname "$0")/../.."

# Who actually invoked sudo (the real user, not root)
REAL_USER="${SUDO_USER:-${USER}}"

# ── 1. Resolve public IP ──────────────────────────────────────────────────────
PUBLIC_ADDR="${1:-${PUBLIC_ADDR:-}}"
if [[ -z "$PUBLIC_ADDR" ]]; then
  PUBLIC_ADDR=$(hostname -I | awk '{print $1}')
  echo "==> Auto-detected IP: $PUBLIC_ADDR"
fi

echo "==> Silencer server setup (public addr: $PUBLIC_ADDR)"

# ── 2. Install Docker if missing ─────────────────────────────────────────────
if ! command -v docker &>/dev/null; then
  echo "==> Installing Docker..."
  apt-get update -qq
  apt-get install -y -qq ca-certificates curl gnupg lsb-release

  install -m 0755 -d /etc/apt/keyrings
  curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
    | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
  chmod a+r /etc/apt/keyrings/docker.gpg

  echo \
    "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
    https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" \
    > /etc/apt/sources.list.d/docker.list

  apt-get update -qq
  apt-get install -y -qq docker-ce docker-ce-cli containerd.io docker-compose-plugin
  systemctl enable --now docker
  echo "==> Docker installed."
else
  echo "==> Docker already installed."
fi

# ── 3. Add user to docker group if not already a member ──────────────────────
if ! groups "$REAL_USER" | grep -qw docker; then
  echo "==> Adding $REAL_USER to docker group..."
  usermod -aG docker "$REAL_USER"
  echo "==> Done. Running docker commands via sg docker for this session."
fi

# ── 4. Build image and start ─────────────────────────────────────────────────
echo "==> Building Docker image (first run takes a few minutes)..."
# Use sg to activate the docker group in this shell session without re-login
sg docker -c "PUBLIC_ADDR='$PUBLIC_ADDR' docker compose build"

echo "==> Starting services..."
sg docker -c "PUBLIC_ADDR='$PUBLIC_ADDR' docker compose up -d"

echo ""
echo "✅  Silencer is running!"
echo "    Lobby:      $PUBLIC_ADDR:15170"
echo "    Game ports: $PUBLIC_ADDR:20000-20009 (UDP)"
echo "    Dashboard:  http://$PUBLIC_ADDR:24000  (admin / admin)"
echo "    Admin API:  http://$PUBLIC_ADDR:24080"
echo ""
echo "    Logs:  docker compose logs -f"
echo "    Stop:  docker compose down"
echo ""
echo "    NOTE: Log out and back in for docker group to apply permanently."
