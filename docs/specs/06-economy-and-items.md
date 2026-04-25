# 06 — Economy & Items

## Credits

Credits are the in-game currency used to purchase items during a match.

### Earning Credits

| Source | Amount |
|--------|--------|
| Returning files at a credit machine | `files × (1 + credits_bonus)` |
| Team secret delivery | 1,000 to each team member |
| "Give To" purchase by teammate | 100 received |

`credits_bonus` is derived from the player's **Contacts** stat:
`credits_bonus = contacts_level × 0.10`.

### Starting Credits

The game creator sets the starting credits (default is 0, but the
single-player tutorial may set it to `0xFFFF`).

## Buy Menu

The buy menu is accessed by pressing `activate` near an **InventoryStation**
inside the player's own base. Only items whose required tech bit is available
(and not disabled) appear.

### Buyable Items

| ID | Name | Price | Repair | Tech Bit | Slots | Agency | Description |
|----|------|-------|--------|----------|-------|--------|-------------|
| BUY_LASER | Laser | 150 | 300 | 0 | 1 | — | 5 laser ammo |
| BUY_ROCKET | Rocket | 250 | 400 | 1 | 1 | — | 3 rocket ammo |
| BUY_FLAMER | Flamer Ammo | 200 | 350 | 2 | 1 | — | 15 flamer ammo |
| BUY_HEALTH | Health Pack | 200 | 400 | 3 | 1 | Noxis | Inventory item |
| BUY_TRACT | Lazarus Tract | 250 | 500 | 14 | 1 | Lazarus | Inventory item |
| BUY_SECURITYPASS | Security Pass | 1,000 | 1,000 | 17 | 1 | Caliber | Inventory item |
| BUY_VIRUS | Virus | 400 | 300 | 19 | 1 | Static | Inventory item |
| BUY_POISON | Poison | 200 | 300 | 4 | 1 | — | Inventory item |
| BUY_EMPB | EMP Bomb | 100 | 200 | 4 | 1 | — | Inventory item |
| BUY_SHAPEDB | Shaped Bomb | 100 | 200 | 5 | 1 | — | Inventory item |
| BUY_PLASMAB | Plasma Bomb | 200 | 300 | 6 | 2 | — | Inventory item |
| BUY_NEUTRONB | Neutron Bomb | 4,000 | 2,000 | 7 | 8 | — | Inventory item |
| BUY_DET | Plasma Detonator | 200 | 400 | 8 | 2 | — | Inventory item |
| BUY_FIXEDC | Fixed Cannon | 300 | 500 | 9 | 1 | — | Inventory item |
| BUY_FLARE | Flare | 200 | 400 | 10 | 1 | — | Inventory item |
| BUY_POISONFLARE | Poison Flare | 200 | 700 | 16 | 1 | Black Rose | Inventory item |
| BUY_CAMERA | Camera | 100 | 200 | 18 | 1 | — | Inventory item |
| BUY_DOOR | Base Door | 300 | 600 | 11 | 1 | — | Inventory item |
| BUY_DEFENSE | Base Defense | 100 | 500 | 12 | 1 | — | Adds in-base turret durability |
| BUY_INFO | Insider Info | 500 | 500 | 13 | 1 | — | +20 secret progress |
| BUY_GIVE0–3 | Give To [teammate] | 100 | 100 | — | 0 | — | Transfer 100 credits to a teammate |

### Purchase Rules

1. The player must be **near an InventoryStation** inside their **own base**.
2. The player must have enough **credits**.
3. The item's **tech bit** must be enabled in the team's available tech AND not
   in the team's `disabled_tech` mask.
4. Ammo purchases (Laser, Rocket, Flamer) are refused if the player is already
   at maximum ammo.
5. Inventory items are refused if all 4 inventory slots are occupied.

### Repair

Repairing re-enables a tech bit that was disabled by an enemy virus. The player
must be near a **TechStation** and pay the item's `repair_price`.

### Virus

Using a virus on an enemy tech station disables one of their tech bits. The
player must be inside an **enemy base** near a TechStation, and have a Virus
in inventory.

## Ammo Limits

| Ammo Type | Per Purchase | Maximum |
|-----------|-------------|---------|
| Laser | 5 | 30 |
| Rocket | 3 | 30 |
| Flamer | 15 | 75 |

## Item Drops

When a player dies, all carried items drop as **PickUp** objects at the death
location:

- Secret (if carried)
- Files (if any)
- Laser ammo (if any)
- Rocket ammo (if any)
- Flamer ammo (if any)
- Each inventory item with quantity > 0

Dropped items inherit gravity and settle on the nearest platform below. Any
player can pick them up by walking over them.
