# zSILENCER Game Specifications

Implementation-agnostic specifications of the rules and mechanics that define
zSILENCER. These documents capture the game design in a way that is independent
of any particular framework, engine, or programming language. They serve two
purposes:

1. **Preservation** — immortalize the game design so it can be understood and
   recreated without access to the original source code.
2. **Portability** — provide a foundation for reimplementing the game in any
   technology stack.

## Documents

| # | Document | Summary |
|---|----------|---------|
| 01 | [World & Physics](01-world-and-physics.md) | Tick rate, gravity, coordinate system, collision detection |
| 02 | [Entities & Objects](02-entities-and-objects.md) | Object hierarchy, entity types, lifecycle |
| 03 | [Player Mechanics](03-player-mechanics.md) | Movement states, input model, health/shield, inventory |
| 04 | [Weapons & Projectiles](04-weapons-and-projectiles.md) | Weapon types, damage model, projectile behavior |
| 05 | [Teams & Agencies](05-teams-and-agencies.md) | Agencies, team structure, tech trees, colors |
| 06 | [Economy & Items](06-economy-and-items.md) | Buyable items, credits, pricing, tech slots |
| 07 | [Game Objectives](07-game-objectives.md) | Secrets, terminals, hacking, win conditions |
| 08 | [NPCs & Security](08-npcs-and-security.md) | Guards, robots, civilians, AI behavior |
| 09 | [Map & Environment](09-map-and-environment.md) | Map structure, platforms, bases, power-ups, warpers |
| 10 | [Progression & Stats](10-progression-and-stats.md) | User accounts, agency upgrades, leveling, statistics |
| 11 | [Animation System](11-animation-system.md) | Sprite banks, frame timing, per-state animation tables |

## Conventions

- All numeric values are expressed as integers unless noted otherwise.
- "Tick" refers to one simulation step. The game runs at **24 ticks per second**.
- Coordinates use a pixel-based system with the origin at the top-left of the
  map. **+X is right, +Y is down.**
- Damage, health, and shield values are unsigned 16-bit integers.
- Velocities are signed 8-bit integers (range −128 to 127).
