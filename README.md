zSilencer
=========

Quick Start (Linux server)
--------------------------
Clone the repo and run one script on a fresh Ubuntu 22.04+ VM to install
Docker and bring up the lobby, admin API, and admin dashboard:

```bash
git clone https://github.com/Arsia-Mons/Silencer.git
cd Silencer
sudo bash scripts/install-linux-server.sh [PUBLIC_IP]
```

If `PUBLIC_IP` is omitted it is auto-detected. Once complete:

| Service      | Address                          |
|--------------|----------------------------------|
| Lobby        | `<ip>:15170`                   |
| Admin UI     | `http://<ip>:24000` (admin / admin) |
| Admin API    | `http://<ip>:24080`            |
| Game ports   | `<ip>:20000-20009` (UDP)       |

```bash
docker compose logs -f   # tail logs
docker compose down      # stop everything
```

Compiling on Linux
------------------
`mkdir build`

`cd build`

`cmake ..`

`make`

`sudo make install`

Compiling on Windows
--------------------
SDL2 and SDL2_mixer development libraries will have to be installed into the Visual Studio include and lib directories  
Open zSILENCER.sln Visual Studio Project  
Compile project using Visual Studio  

Supported platforms
-------------------
The game is supported on Windows, Mac OS X, and Linux.  Other platforms, such as Android, work but are not fully tested.

Running game
------------
SDL2, SDL2_mixer, and ZLIB are required to run the game.

Documentation
-------------
- [Developer Guide](CLAUDE.md) — project layout, build commands, dedicated-server contract, gotchas
- [Production Setup](docs/production.md) — stand up your own lobby on AWS: Terraform, CI wiring, day-2 ops, failure modes
- [Lobby Server](server/README.md) — self-hosted lobby server (Go): build, run, protocol, deployment
- [Admin API](admin/api/README.md) — Express.js REST + WebSocket API powering the admin dashboard
- [Admin Web](admin/web/README.md) — Next.js admin dashboard and player self-service portal
