# Silencer — Feature Roadmap

A forward-looking roadmap of proposed additions: new game modes, mechanics,
weapons, NPCs, object types, items, and agency kit.

This document is the **design counterpart** to [`docs/specs/`](specs/README.md).
Where the specs *preserve what is already built*, this roadmap *proposes what
comes next*. Every entry is grounded in an existing system and links back to the
relevant spec so additions stay consistent with the simulation
(24 ticks/second, side-scrolling, +Y down, authoritative server).

Tracking: [#310](https://github.com/Arsia-Mons/Silencer/issues/310). Individual
features graduate to their own issue + branch + PR when picked up
(see [git-workflow.md](git-workflow.md)).

## How to read this

| Column | Meaning |
|--------|---------|
| **Status** | `proposed` (idea only) · `designing` (spec being written) · `in-progress` (issue + branch open) · `shipped` |
| **Effort** | `S` ≈ tune existing system · `M` ≈ new verb on the existing loop · `L` ≈ new subsystem |
| **Builds on** | The spec/system it extends — read before implementing |

Nothing here is committed scope. Prune aggressively; a smaller, sharper game
beats a feature checklist (see CLAUDE.md *Combat overengineering*).

---

## 1. Game Modes

Most modes in `src/game/modes/*` are **scaffolds** — `IsMatchOver()` returns
`false` and only `DataRetrievalMode` carries real win logic. Finishing the
stubs is the highest-leverage work because the framework, IDs, and lobby wiring
already exist.

### 1a. Finish the stubbed modes

| Mode | File | Proposed win condition | Status | Effort |
|------|------|------------------------|--------|--------|
| **Extraction** | `extraction_mode.h` | First team to deliver N secrets to `SecretReturn` *and* beam out all surviving members. Builds on the existing secret lifecycle. | proposed | M |
| **Sabotage** | `sabotage_mode.h` | Attackers plant a Plasma Detonator on the enemy `TechStation` and detonate it; defenders win on timer. | proposed | M |
| **Manhunt** | `manhunt_mode.h` | One marked target per team; killing the target scores. Marked player is shown on radar. | proposed | M |
| **Escort** | `escort_mode.h` | A neutral VIP NPC (new, §4) must be walked/jetpacked to an exit; escorts score, defenders intercept. | proposed | L |
| **Control Points** | `control_points_mode.h` | Capture-and-hold map terminals; score per tick held. Reuses hacking dwell. | proposed | M |
| **Assassination** | `assassination_mode.h` | Asymmetric: one team has a high-value target that must survive to extraction. | proposed | M |
| **Survival** | `survival_mode.h` | Co-op vs. escalating guard/robot waves; last team standing or rounds cleared. | proposed | L |
| **Team Deathmatch** | `team_deathmatch_mode.h` | First team to score limit. Trivial once `IsMatchOver` reads team frags. | proposed | S |

> Implementation note: each mode owns its `IsMatchOver(World&)` plus any
> per-tick scoring; win detection currently delegates to `Team::Tick`
> ([spec 07](specs/07-game-objectives.md)). Keep that boundary.

### 1b. Net-new modes

| Mode | Concept | Builds on | Status | Effort |
|------|---------|-----------|--------|--------|
| **Vertical Heist** (asymmetric) | Infiltrators steal a secret from the lowest base and jetpack it to a rooftop drop; Wardens defend with cameras/cannons, cut jetpack fuel, and collapse platforms below. | secrets, jetpack fuel, gadgets, verticality | proposed | L |
| **Blackout** | One team hacks terminals to kill the lights section-by-section (lower `ambience`); the other defends them. Vision/radar become the currency. | hacking, lighting, radar powerup | proposed | M |
| **Courier (King-of-the-Briefcase)** | A single secret with a live radar ping; holding it high in a vertical arena scores over time. | secrets + control-point scoring | proposed | M |
| **Infection / Patient Zero** | Survival twist — players killed by poison/virus convert to the infected team; last clean agent wins. | viruses, poison, survival scaffold | proposed | M |
| **Sudden Drop** (elimination) | A rising hazard (gas/flood) forces everyone upward; last alive wins. | verticality, flare/poison hazards | proposed | M |
| **Demolition** (plant/defuse) | Attackers plant a det on one of two reactors; defenders defuse via hack. | dets + hacking | proposed | M |

---

## 2. Mechanics

Side-scroller mechanics that make verticality, jetpack fuel, and stealth into
contested resources rather than free movement.
Builds on [spec 03](specs/03-player-mechanics.md).

| Mechanic | Description | Builds on | Status | Effort |
|----------|-------------|-----------|--------|--------|
| **Jetpack fuel war** | EMP grenades drain enemy `fuel`; a powerup/hack refills it. Verticality becomes contested. | EMP grenade, `max_fuel` | proposed | S |
| **Fall damage & blast knockback** | Shaped/plasma blasts launch agents; pit deaths become a real threat and a weapon. | grenades, platforms | proposed | M |
| **Ceiling / wall cling** | Hold position on a ceiling to drop ambushes or break a horizontal sightline; pairs with invisibility. | `Bipedal`, player FSM | proposed | M |
| **Secret "heat" trail** | A carried secret periodically pings its location on radar, scaling tension. | secret `trace_time`, radar | proposed | S |
| **Class/agency signature movement** | STATIC = blink/dash, CALIBER = double-jump, LAZARUS = slow-fall glide, NOXIS = wall-cling, BLACK ROSE = grapple. Makes the 5 agencies play differently vertically. | agencies, player FSM | proposed | L |
| **Hack-to-sabotage environment** | Terminals toggle doors/turrets/lights/trapdoors map-wide, turning hacking into area control. | hacking, terminals | proposed | M |
| **Body / secret stashing** | Drag downed bodies or hide a stolen secret in a cache for a later run. | PickUp, bodies | proposed | M |
| **Captured gadgets** | An EMP'd enemy camera/cannon can be *re-hacked* to your team instead of merely stunned. | virus, fixed cannon, camera | proposed | M |
| **Powerup tradeoffs** | Invisibility cancels on fire; super-shield slows jetpack. Mostly tuning + a flag. | powerups | proposed | S |

---

## 3. Weapons & Projectiles

Additions to the four weapon slots and the grenade/deployable family.
Builds on [spec 04](specs/04-weapons-and-projectiles.md). Keep the
`health_damage`/`shield_damage`/`bypass_shield` model and 8-direction firing.

### 3a. Weapons / projectiles

| Name | Concept | Builds on | Status | Effort |
|------|---------|-----------|--------|--------|
| **Railgun** | Charged hitscan-style line shot; high shield + health damage, long cooldown, pierces. Pairs with vertical sniping lanes. | projectile model | proposed | M |
| **Grappling hook** | Not a damage weapon — a traversal projectile that anchors and reels (BLACK ROSE signature, see §2). | projectile + player FSM | proposed | L |
| **Sticky bomb launcher** | Fires a projectile that adheres to platforms/players and detonates on a timer. | grenade/det physics | proposed | M |
| **Cryo / glue shot** | Slows or roots a target for a short window; no damage. Counterplay to jetpack escapes. | projectile + status timers | proposed | M |
| **Ricochet blaster mod** | Blaster variant whose shots bounce off platforms once — area denial in tight vertical shafts. | blaster, platform collision | proposed | S |

### 3b. Grenades / deployables (extends the grenade sub-type table)

| Sub-Type | Effect | Status | Effort |
|----------|--------|--------|--------|
| **SMOKE** | Blocks line of sight (and guard targeting) in a radius for N ticks. | proposed | M |
| **GRAVITY** | Briefly inverts or nullifies gravity in a blast radius — reshapes a firefight. | proposed | L |
| **STICKY EMP** | EMP that adheres before pulsing; drains fuel + shields. | proposed | S |
| **DECOY** | Deployable that reads as a player to guards/robots/cameras, drawing fire. | proposed | M |

---

## 4. NPCs & Security

New authoritative, server-controlled NPCs beyond Guard / Robot / Civilian.
Builds on [spec 08](specs/08-npcs-and-security.md). Each new NPC ships with a
behavior tree under `shared/assets/behaviortrees/` so designers can tune it
without a client rebuild.

| NPC | Concept | Builds on | Status | Effort |
|-----|---------|-----------|--------|--------|
| **VIP / Hostage** | Neutral escort target for Escort/Assassination modes; follows or flees, can be downed. | civilian FSM | proposed | M |
| **Sentry Drone** | Flying patrol unit that ignores platforms — pressures jetpack airspace. | guard targeting + free flight | proposed | M |
| **Hacker NPC** | Government counter-hacker that re-secures terminals players have hacked. | terminal states | proposed | M |
| **Riot Guard** | Shielded melee guard that advances behind a frontal shield; flank or EMP to break. | guard BT | proposed | M |
| **Scientist (civilian variant)** | Civilian that, if escorted to a terminal, grants secret progress — a non-combat objective NPC. | civilian, secret progress | proposed | M |
| **Heavy Robot variant** | Stationary turret-robot with the plasma profile but no patrol. | robot BT | proposed | S |

---

## 5. Object / Entity Types

New authoritative object types (extends [spec 02](specs/02-entities-and-objects.md))
and map-placed actors ([spec 09](specs/09-map-and-environment.md)).

| Object | Concept | Builds on | Status | Effort |
|--------|---------|-----------|--------|--------|
| **MovingPlatform** | Platform that traverses a `TRACK`; carries riders. Enables vertical-puzzle maps. | platform `TRACK` type | proposed | L |
| **Trapdoor / BlastDoor** | Hack/EMP-toggled floor or gate; the hook for hack-to-sabotage (§2). | platforms, hacking | proposed | M |
| **DestructibleWall** | Platform segment that shaped charges can blow open to create shortcuts. | shaped grenade, platforms | proposed | M |
| **Zipline** | One-way silent traversal lane bypassing jetpack noise. | new traversal object | proposed | M |
| **SecretCache** | A stash point where a carried secret can be hidden and retrieved later. | PickUp, secrets | proposed | M |
| **PressurePlate / Trigger** | Map logic primitive that fires events (open door, raise platform). | platform triggers | proposed | M |
| **HazardEmitter** | Map-placed gas/flood source for Sudden Drop and environmental hazards. | flare/poison area logic | proposed | M |

---

## 6. Items, Economy & Agencies

New buyable tech ([spec 06](specs/06-economy-and-items.md)) and agency kit
([spec 05](specs/05-teams-and-agencies.md)). The tech mask is 32-bit with
**unused bits 4, 15, and 20–31** available for new items.

### 6a. Buyable items / powerups

| Item | Concept | Tech bit | Builds on | Status | Effort |
|------|---------|----------|-----------|--------|--------|
| **Smoke Grenade** | See §3b SMOKE. | free bit | grenades | proposed | M |
| **Decoy** | See §3b DECOY. | free bit | deployables | proposed | M |
| **Jammer** | Deployable that suppresses enemy radar/minimap in a radius. | free bit | radar powerup | proposed | M |
| **Ammo Cache** | Deployable teammates can resupply from, away from base. | free bit | InventoryStation | proposed | M |
| **Grapple Kit** | Enables the grappling hook (§3a) for non-Black-Rose agencies. | free bit | new weapon | proposed | M |

New **power-up** sub-types (extends the map power-up table, currently 0–6):

| Sub-Type | Name | Effect | Status |
|----------|------|--------|--------|
| 7 | **Overdrive** | Temporary fire-rate / move-speed boost. | proposed |
| 8 | **Phase** | Brief pass-through-projectiles window. | proposed |
| 9 | **Magnet** | Auto-collects nearby dropped files/items for a duration. | proposed |

> Reminder (user preference, carried from sister projects): **powerups are
> always time-based and temporary — never permanent.** New powerups must expire
> on a per-player countdown.

### 6b. Sixth agency (optional, L)

A sixth agency slot would touch team colors, agency-exclusive tech, and lobby
selection. **Proposed, low priority** — flagged here so color/tech tables get
extended deliberately rather than ad hoc. Candidate identity: **Vox** —
information warfare (exclusive **Jammer**, hacking-disruption focus).

---

## 7. Cross-cutting / sequencing

Suggested order of attack, highest payoff first:

1. **Team Deathmatch** win logic (S) — proves the mode framework end-to-end.
2. **Jetpack fuel war** + **fall/knockback** (S/M) — cheap, high-impact feel.
3. **Extraction / Sabotage / Manhunt** stubs (M) — fills out the mode roster.
4. **Hack-to-sabotage + Trapdoor/DestructibleWall** (M) — unlocks several modes
   and the environmental-control fantasy.
5. **Agency signature movement** (L) — differentiates the 5 agencies.
6. Net-new modes (**Vertical Heist**, **Blackout**) once the primitives above land.

Each item, when picked up, gets: an issue (`gh issue create`), a branch
(`feat/issue-N-desc`), a short design note in `docs/specs/` *if it changes
existing rules*, and a PR closing its issue. Update this table's **Status** as
features move.
