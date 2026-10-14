# Item Tool

The Item Tool (`/items`) is the admin editor for all purchasable and
collectible items — weapons for sale, consumables, deployables, and
agency-restricted gadgets.  Everything is edited locally against the GAS
folder on your machine; changes are saved by downloading the updated
`items.json`.

---

## Opening a folder

The landing screen shows **[ OPEN FOLDER ]**.

1. Click it and select the `shared/assets/gas/` directory.
2. The tool reads `items.json` and redirects to the first item automatically.
3. If another GAS tool (GAS Editor, Weapon Editor) already has a folder open
   the tool loads from that cached folder without prompting.

**Change folder** — click **[ CHANGE ]** in the header to pick a different
directory.  
**Close folder** — click **[ CLOSE ]** to clear all state and return to the
landing screen.

---

## Layout

```
┌──────────────────┬───────────────────────────────────────────────────┐
│  Item list       │  Editor                                           │
│  [ search ]      │  Identity · Sprite · Purchase · Tech Tree        │
│  [ + ADD ]       │  Stats & Effects · Sounds                        │
│  item rows…      │  [ actions ]                                      │
└──────────────────┴───────────────────────────────────────────────────┘
```

---

## Item list (left sidebar)

| Control | Action |
|---|---|
| Search box | Filter items by name or ID (case-insensitive) |
| **+ ADD** | Create a new blank item and open it in the editor |
| Click item row | Navigate to that item |
| `↑` / `↓` arrow keys | Navigate items (inactive when an input/select/textarea has focus) |

---

## Editor panels (right)

### Header bar

| Element | Description |
|---|---|
| Folder name | Shows the open folder path |
| Unsaved indicator | Appears when there are uncommitted changes |
| **[ DOWNLOAD JSON ]** | Downloads `items.json` with all current changes |
| Validation warnings | Inline notices for missing required fields |

---

### Identity

Core metadata for the item.

| Field | Type | Description |
|---|---|---|
| `id` | string | Unique identifier used in code and GAS references |
| `enumId` | int | Numeric ID used by the C++ client |
| `name` | string | Display name shown in-game |
| `description` | string | Tooltip / shop description |
| `agencyRestriction` | select | Which agency (team) can purchase this item |

**Agency restriction values**

| Value | Meaning |
|---|---|
| `-1` | No restriction — all agencies |
| `0` | Agency 0 (Blackrose) only |
| `1` | Agency 1 (Vanguard) only |
| `2` | Agency 2 (Syndicate) only |
| `3` | Agency 3 (Ghost) only |
| `4` | Agency 4 (Reaper) only |

---

### Sprite

Controls the item's inventory icon.

| Field | Description |
|---|---|
| **Sprite preview** | Shows the current bank + frame combination |
| **[ PICK SPRITE ]** | Opens the sprite picker modal |
| `spriteBank` | The sprite bank number |
| `spriteIndex` | The frame index within the bank |

> Set `spriteBank: 255` for abstract items that have no visible icon.

**Sprite picker modal**

- Lists all non-empty sprite banks from `/api/sprites`.
- Selecting a bank shows its individual frames.
- Click a frame to assign `spriteBank` and `spriteIndex`.
- Click the background or **✕** to close without changing.

---

### Purchase

Economy fields for the in-game shop.

| Field | Type | Description |
|---|---|---|
| `price` | int | Credits to purchase |
| `repairPrice` | int | Credits to repair after use |
| `spawnInventoryCount` | int | How many are given when the item spawns into inventory |

---

### Tech Tree

Controls how the item fits into the upgrade system.

| Field | Type | Description |
|---|---|---|
| `techSlots` | int | Number of tech slots consumed when equipped |
| `techChoice` | bitmask | Category flags — determines which tech-choice slots the item occupies |

`techChoice` is a bitmask where each bit represents a distinct tech category.
Setting multiple bits means the item occupies more than one category slot.
Common values used in the current item set:

| `techChoice` | Item |
|---|---|
| `1` | Laser |
| `2` | Rocket |
| `4` | Flamer Ammo |
| `8` | Health Pack |
| `16` | E.M.P. Bomb |
| `32` | Shaped Bomb |
| `64` | Plasma Bomb |
| `128` | Neutron Bomb |
| `256` | Plasma Detonator |
| `512` | Fixed Cannon |
| `1024` | Flare |
| `2048` | Base Door |
| `4096` | Base Defense |
| `8192` | Insider Info |
| `16384` | Lazarus Tract |
| `32768` | Poison |
| `65536` | Poison Flare |
| `131072` | Security Pass |
| `262144` | Camera |
| `524288` | Virus |

---

### Stats & Effects

Gameplay values applied when the item is used or picked up.

| Field | Type | Description |
|---|---|---|
| `spawnAmmo` | int | Ammo granted when spawned into the world |
| `pickupAmmo` | int | Ammo granted when a player picks it up |
| `maxAmmo` | int | Maximum ammo the player can carry |
| `healAmount` | int | HP restored when used |
| `poisonDose` | int | Poison stacks applied to target on use |

---

### Sounds

Sound slots for item interactions.  Each field is a dropdown of all uploaded
sound files (fetched from `/api/sounds`), with a **▶ / ■** button to
preview or stop playback in the browser.

| Slot | When it plays |
|---|---|
| `soundPickup` | Player picks up the item |
| `soundUse` | Item is activated/used |
| `soundWarn` | Warning signal (e.g. bomb countdown) |

All slots accept a plain filename (`"pickup.wav"`) or a sound cue reference
(`"cue:item_pickup"`).  See [sound-cue-system.md](sound-cue-system.md).

---

## Actions

| Button | Action |
|---|---|
| **Duplicate** | Clone the current item with a new auto-generated ID |
| **Delete** | Remove the item from the list (requires confirmation) |
| **[ DOWNLOAD JSON ]** | Save all changes to `items.json` |

---

## All items

| ID | Name | Price | Tech slots | Agency |
|---|---|---|---|---|
| `laser` | Laser | 150 | 1 | All |
| `rocket` | Rocket | 250 | 1 | All |
| `flamer` | Flamer Ammo | 200 | 1 | All |
| `healthpack` | Health Pack | 200 | 1 | Blackrose (0) |
| `lazarustract` | Lazarus Tract | 250 | 1 | Vanguard (1) |
| `securitypass` | Security Pass | 1000 | 1 | Syndicate (2) |
| `virus` | Virus | 400 | 1 | Ghost (3) |
| `poison` | Poison | 100 | 1 | Reaper (4) |
| `empbomb` | E.M.P. Bomb | 1000 | 1 | All |
| `shapedbomb` | Shaped Bomb | 100 | 1 | All |
| `plasmabomb` | Plasma Bomb | 200 | 1 | All |
| `neutronbomb` | Neutron Bomb | 4000 | 1 | All |
| `plasmadetonator` | Plasma Detonator | 200 | 1 | All |
| `fixedcannon` | Fixed Cannon | 300 | 1 | All |
| `flare` | Flare | 200 | 1 | All |
| `poisonflare` | Poison Flare | 200 | 1 | Reaper (4) |
| `camera` | Camera | 100 | 1 | All |
| `basedoor` | Base Door | 300 | 1 | All |
| `basedefense` | Base Defense | 100 | 1 | All |
| `insiderinfo` | Insider Info | 500 | 1 | All |
| `give0–give3` | Give To… | 100 | 0 | All |

---

## `items.json` schema

```jsonc
{
  "_comment": "...",
  "items": [
    {
      "id": "laser",               // string — unique identifier
      "enumId": 1,                 // int — C++ enum value
      "name": "Laser",             // string — display name
      "description": "...",        // string — shop tooltip
      "price": 150,                // int — purchase cost (credits)
      "repairPrice": 0,            // int — repair cost
      "spriteBank": 120,           // int — icon sprite bank (255 = none)
      "spriteIndex": 0,            // int — frame within sprite bank
      "techChoice": 1,             // int — bitmask for tech-slot category
      "techSlots": 1,              // int — slots consumed when equipped
      "agencyRestriction": -1,     // int — -1 = all, 0-4 = specific agency
      // Optional fields:
      "spawnAmmo": 0,              // int — ammo on world spawn
      "pickupAmmo": 5,             // int — ammo on pickup
      "maxAmmo": 20,               // int — ammo cap
      "spawnInventoryCount": 1,    // int — count given on spawn
      "healAmount": 0,             // int — HP restored on use
      "poisonDose": 0,             // int — poison stacks on use
      "soundPickup": "pickup.wav", // string — pickup sound (or cue:id)
      "soundUse": "",              // string — use sound
      "soundWarn": ""              // string — warning sound
    }
  ]
}
```

---

## API calls

All calls require a valid admin JWT stored in `localStorage` as `zs_token`.

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/sprites` | Populate sprite picker bank list |
| `GET` | `/api/sprites/:bank/frames` | Load frame thumbnails in picker |
| `GET` | `/api/sprites/:bank/:frame` | Display selected sprite preview |
| `GET` | `/api/sounds` | Populate sound slot dropdowns |
| `GET` | `/api/sounds/:name/play` | Stream audio for in-browser preview |

There are no server-side item write endpoints — saving is local only.

---

## Deployment workflow

1. Open the GAS folder, make your edits.
2. Click **[ DOWNLOAD JSON ]** → saves `items.json`.
3. Copy it into `shared/assets/gas/`.
4. Rebuild / restart the game server.
