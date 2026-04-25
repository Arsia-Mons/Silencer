zSilencer
=========

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
