Silencer
========

Quick Start (Linux server)
--------------------------
Clone the repo and run one script on a fresh Ubuntu 22.04+ VM to install
Docker and bring up the lobby, admin API, and admin dashboard:

```bash
git clone https://github.com/Arsia-Mons/Silencer.git
cd Silencer
sudo bash infra/scripts/install-linux-server.sh [PUBLIC_IP]
```

If `PUBLIC_IP` is omitted it is auto-detected. Once complete:

| Service      | Address                                        |
|--------------|------------------------------------------------|
| Lobby        | `<ip>:15170`                                 |
| Admin UI     | `http://<ip>:24000` (admin / admin)          |
| Admin API    | `http://<ip>:24080`                          |
| Game ports   | `<ip>:20000-20199` (UDP, up to 200 concurrent games) |

```bash
docker compose logs -f   # tail logs
docker compose down      # stop everything
```

To change the concurrent game limit, update two values in `docker-compose.yml`
and restart:

```yaml
ports:
  - "20000-20199:20000-20199/udp"   # match game-port-count
command:
  - "-game-port-base"
  - "20000"
  - "-game-port-count"
  - "200"                           # one UDP port per concurrent game
```

Compiling on Linux
------------------
```bash
cmake -B build -S clients/silencer
cmake --build build -j
sudo cmake --install build
```

Compiling on Windows
--------------------
SDL2 and SDL2_mixer development libraries will have to be installed (vcpkg
manifest mode picks them up automatically when configured with the vcpkg
toolchain — see `clients/silencer/vcpkg.json`). Configure with:

```pwsh
cmake -B build -S clients/silencer -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

Supported platforms
-------------------
The game is supported on Windows, Mac OS X, and Linux. Other platforms,
such as Android, work but are not fully tested.

Running game
------------
SDL2, SDL2_mixer, and ZLIB are required to run the game.

Documentation
-------------
- [Changelog](CHANGELOG.md) — release notes and feature history
- [Developer Guide](CLAUDE.md) — project layout, build commands, dedicated-server contract, gotchas
- [Production Setup](docs/production.md) — stand up your own lobby on AWS: Terraform, CI wiring, day-2 ops, failure modes
- [Lobby Server](services/lobby/README.md) — self-hosted lobby server (Go): build, run, protocol, deployment
- [Admin API](admin/api/README.md) — Express.js REST + WebSocket API powering the admin dashboard
- [Admin Web](admin/web/README.md) — Next.js admin dashboard and player self-service portal
