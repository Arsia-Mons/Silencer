# Game Asset System (GAS)

The Game Asset System (GAS) is a set of JSON files that define all tunable
gameplay parameters — damage values, movement speeds, NPC behaviour, weapon
physics, sound assignments, game modes, and more.  The game client loads them
at startup; changing a value and restarting the server is all that is needed
to take effect.

Files live in `shared/assets/gas/` and are shared between the C++ game client,
the Go lobby server, and the admin web app.

---

## GAS Editor

The editor lives at `/gas` in the admin web app.  It is a Monaco-based JSON
editor with schema validation and baseline-integrity checking.

### Opening files

Click **Open gas folder** and select the `shared/assets/gas/` directory from
the repo.  All JSON files in the folder are loaded into memory.  The sidebar
shows the editable files:

| Tab | File |
|---|---|
| PLAYER | `player.json` |
| AGENCIES | `agencies.json` |
| WEAPONS | `weapons.json` |
| ENEMIES | `enemies.json` |
| ITEMS | `items.json` |
| GAME OBJECTS | `gameobjects.json` |
| ABILITIES | `abilities.json` |

The following files are loaded and validated by the game but have no dedicated
editor tab: `world.json`, `gameengine.json`, `effects.json`, `lights.json`,
`gamemodes.json`.  Edit these directly in a text editor.

### Monaco editor features

- **JSON schema validation** — all known fields are type-checked live as you
  type; errors are underlined and counted in the status bar.
- **Hover documentation** — hover over any key to see its description from
  the schema.
- **Autocomplete** — `Ctrl+Space` suggests known keys and values.
- **Bracket pair colorization, format on paste**, minimap, line numbers.
- The editor uses a custom dark theme (`silencer-dark`).

### Saving

`Ctrl+S` (or click **Save**) saves the current file.

Before saving, the editor:
1. Parses the JSON — blocks if the JSON is syntactically invalid.
2. Checks schema errors — blocks if there are type or constraint violations.
3. Checks **baseline integrity** — blocks if you have removed a key or array
   entry that existed in the file when you opened the folder.

Saving uses the **File System Access API** if available (writes directly to
the file on disk); otherwise it downloads the JSON.

### Baseline integrity / Validate All

The **Validate All** panel compares every open file against the version that
was on disk when the folder was opened.  It flags any fields or array entries
that have been removed.  You can:

- **Re-add a single violation** — inserts the missing field back with its
  original value.
- **Re-add all violations** — restores every removed field at once.

This prevents accidental deletion of fields the game depends on.

### Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+S` | Save current file |
| `Ctrl+Shift+F` | Format JSON |

---

## Files reference

### `player.json` — Player def

Defines player character stats, movement tuning, timing constants, sound
slots, and AI parameters.  Consumed by `PlayerDef` in the C++ loader.

#### Movement & physics

| Key | Type | Description |
|---|---|---|
| `runSpeed` | int | Base run speed |
| `runSpeedDisguised` | int | Run speed while disguised |
| `runSpeedSecret` | int | Run speed while carrying a secret |
| `runSpeedSecretDisguised` | int | Run speed while disguised + carrying secret |
| `walkAcceleration` | int | Ground acceleration |
| `jumpImpulse` | int | Jump velocity |
| `ladderJumpImpulse` | int | Impulse when jumping off a ladder |
| `ladderActivateImpulse` | int | Speed when grabbing a ladder |
| `ladderSpeedReduction` | int | Speed reduction on ladder |
| `rollSpeed` | int | Speed during a roll |
| `worldGravity` | int | Global gravity constant |
| `worldMaxYVelocity` | int | Terminal fall velocity |
| `hitKnockbackAirFactor` | number | Knockback multiplier when airborne |
| `fallingNudgeMax` | int | Max ledge-nudge distance during a fall |
| `fallingNudgeXvDivisor` | int | Divisor for fall nudge calculation |
| `deathDropYV` | int | Upward velocity on death |
| `deathDropXVRange` | int | Horizontal velocity spread on death |

#### Jetpack

| Key | Type | Description |
|---|---|---|
| `baseFuel` | int | Starting jetpack fuel |
| `jetpackXvMax` | int | Max horizontal jetpack speed |
| `jetpackXvMaxDisguised` | int | Horizontal max while disguised |
| `jetpackYvMax` | int | Max vertical jetpack speed |
| `jetpackThrust` | int | Per-tick thrust |
| `jetpackXvAccel` | int | Horizontal acceleration while jetting |
| `jetpackCeilingCheckRange` | int | Ceiling detection distance |
| `jetpackBonusDurationTicks` | int | Ticks added by jetpack powerup |
| `audioFadeJetpackMs` | int | Jetpack sound fade-out duration |

#### Health, shield & upgrades

| Key | Type | Description |
|---|---|---|
| `baseHealth` | int | Starting HP |
| `baseShield` | int | Starting shield |
| `upgradeMultiplierEndurance` | int | HP multiplier per endurance level |
| `upgradeMultiplierShield` | int | Shield multiplier per shield level |
| `upgradeMultiplierJetpack` | int | Fuel multiplier per jetpack level |
| `upgradeMultiplierHacking` | number | Hacking speed multiplier |
| `upgradeMultiplierContacts` | number | Economy multiplier |
| `superShieldMultiplier` | int | Super-shield bonus multiplier |
| `shieldShrapnelThreshold` | int | Shield remaining before shrapnel spawns |
| `shrapnelCount` | int | Number of shrapnel pieces on shield break |
| `shrapnelSpeed` | number | Shrapnel launch speed |
| `shieldEffectTicks` | int | Duration of shield-hit VFX |
| `hitSoundCooldownTicks` | int | Minimum ticks between hit sounds |

#### Abilities & actions

| Key | Type | Description |
|---|---|---|
| `disguiseActivationTicks` | int | Ticks to enter disguise |
| `disguiseDeactivationTicks` | int | Ticks to exit disguise |
| `disguiseThreshold` | int | Proximity threshold that breaks disguise |
| `invisibilityDurationTicks` | int | Cloaking duration |
| `warpDurationTicks` | int | Warp effect duration |
| `warpNonCollidableTicks` | int | Ticks of phase-through after warping |
| `warpTeleportTick` | int | Tick within warp animation when position jumps |
| `deployWaitTicks` | int | Ticks before a deployed object activates |
| `deployAnimationTicks` | int | Deploy animation length |
| `rollSpeed` | int | Roll speed |
| `hackingBonusDurationTicks` | int | Hacking powerup duration |
| `radarBonusDurationTicks` | int | Radar powerup duration |
| `powerupRespawnTicks` | int | Ticks before a powerup respawns |
| `hackingCompleteThreshold` | int | Hacking progress needed to complete |
| `hackingExitThreshold` | int | Progress needed to exit safely |
| `hackingEffectTicks` | int | Duration of hack VFX |
| `hackingPowerupBonus` | number | Extra hacking speed with powerup |
| `poisonTickCycle` | int | Ticks between poison damage ticks |
| `maxPoisoned` | int | Max stacks of poison |

#### Economy

| Key | Type | Description |
|---|---|---|
| `startingCredits` | int | Credits at mission start |
| `creditFloor` | int | Minimum credits (cannot go below) |
| `creditCap` | int | Maximum credits |
| `teamGiftCredits` | int | Credits given to teammates on gift |
| `secretDeliveryCredits` | int | Credits for delivering a secret |
| `fileConversionBase` | int | Base credit value of a converted file |

#### Win conditions

| Key | Type | Description |
|---|---|---|
| `secretsNeededToWin` | int | Secrets required for a win |
| `secretProgressBeamThresh` | int | Progress at which beam VFX starts |
| `secretProgressSoundThresh` | int | Progress at which sound plays |
| `maxFiles` | int | Max files the player can carry |

#### AI parameters

| Key | Type | Description |
|---|---|---|
| `aiDisguiseInterval` | int | AI disguise check interval |
| `aiHackInterval` | int | AI hack attempt interval |
| `aiLadderJumpUpInterval` | int | AI ladder jump-up frequency |
| `aiLadderJumpDownInterval` | int | AI ladder jump-down frequency |
| `aiArrivalThreshold` | int | Distance to consider AI "arrived" |
| `aiCombatRange` | int | AI combat engagement range |
| `aiFireInterval` | int | AI firing interval |
| `aiEvadeInterval` | int | AI evasion interval |
| `aiTargetLockTicks` | int | Ticks before AI locks onto target |
| `aiRetreatHealthPct` | int | Health % at which AI retreats |
| `aiReactionTicks` | int | AI reaction delay |
| `aiShootBurstTicks` | int | AI burst fire duration |
| `aiShootPauseTicks` | int | AI pause between bursts |
| `aiJetpackCombatInterval` | int | AI jetpack usage frequency |
| `aiThinkDelayMax` | int | Max AI think delay |

#### Sound slots

All sound slots accept either a filename (`"select2.wav"`) or a sound cue
reference (`"cue:player_reload"`).

`soundImpactBlaster`, `soundImpactLaserShield`, `soundImpactLaser`,
`soundImpactFlamer`, `soundShieldDown`, `soundGrunt`, `soundDisguise`,
`soundJackout`, `soundJetpack`, `soundMenuSelect`, `soundWeaponCharged`,
`soundAlertWarn`, `soundAlertInvestigate`, `soundAmmo1–4`, `soundUIClick`,
`soundTeamJoin`, `soundTeamHQ`, `soundTeamHeal`, `soundTeamHack`,
`soundRoundCountdown`, `soundUndeploy`, `soundBreath`, `soundFootstepCrouchL/R`,
`soundFootstepStairL/R`, `soundBaseAlarm`, `soundIntrude`, `soundSecurityPass`,
`soundRoll`, `soundPickup`, `soundJackIn`, `soundHackAmbient`, `soundType`,
`soundRepair`, `soundHurt`, `soundLandCrouch`, `soundReload`,
`soundJetpackLoop`, `soundLand`, `soundFall`, `soundLadder`, `soundPowerUp`

---

### `agencies.json` — Agency defs

Defines the playable agencies (factions).

| Key | Type | Description |
|---|---|---|
| `id` | int | Numeric agency ID |
| `name` | string | Display name |
| `defaultBonuses` | int | Starting bonus points |
| `maxPlayersPerTeam` | int | Max players per team (`1` for solo agencies) |
| `weapons` | string[] | Allowed weapon IDs |
| `defaultUpgrades` | object | Starting free upgrade points per category |
| `upgradeCaps` | object | Maximum upgrade level per category |

`defaultUpgrades` / `upgradeCaps` fields: `endurance`, `shield`, `jetpack`,
`techslots`, `hacking`, `contacts`.

---

### `weapons.json` — Weapon defs

Defines every weapon's damage, physics, timings, audio, and sprite data.

#### Damage

| Key | Type | Description |
|---|---|---|
| `healthDamage` | int | HP damage per hit (normal) |
| `shieldDamage` | int | Shield damage per hit (normal) |
| `healthDamageLarge` | int | HP damage for large variant |
| `shieldDamageLarge` | int | Shield damage for large variant |
| `poisonRate` | int | Poison stacks per hit |
| `poisonMax` | int | Maximum poison stacks inflicted |
| `poisonCheckInterval` | int | Interval between poison damage ticks |

#### Projectile physics

| Key | Type | Description |
|---|---|---|
| `projectileType` | string | Physics mode identifier |
| `velocity` | int | Projectile base speed |
| `moveAmount` | int | Per-tick movement distance |
| `radius` | int | Collision radius |
| `projectileLife` | int | Ticks before expiry |
| `bounceDamping` | number | Speed reduction per bounce |
| `plasmaGravity` | int | Downward pull for plasma |
| `plasmaLifeNormal` | int | Plasma normal lifetime |
| `plasmaLifeLarge` | int | Plasma large variant lifetime |
| `rocketSlowInitial` | int | Rocket slow phase duration |
| `rocketHoverTick` | int | Tick when rocket enters hover |
| `rocketSlowHover` | int | Rocket hover speed |
| `neutronTraceTime` | int | Neutron trace duration |

#### Throw physics

`throwSpeedStanding`, `throwSpeedMoving`, `throwSpeedRunning`,
`throwYvDown`, `throwXvDownDiag`, `throwYvDownDiag`, `throwXvUp`,
`throwYvUp`, `throwXvUpDiag`, `throwYvUpDiag`, `throwXvCrouch`,
`throwYvCrouch`

#### Timing

| Key | Type | Description |
|---|---|---|
| `fireDelay` | int | Ticks between shots |
| `reloadTicks` | int | Ticks to reload |
| `ammoCapacity` | int | Magazine size |
| `explosionTick` | int | Tick of primary explosion |
| `secondaryTick` | int | Tick of secondary effect |
| `destroyTick` | int | Tick when projectile is removed |
| `flareDuration` | int | Flare effect duration |
| `flareSpawnInterval` | int | Ticks between flare spawns |

#### Emit & visual

| Key | Type | Description |
|---|---|---|
| `emitOffset` | int | Barrel offset for projectile spawn |
| `exhaustPlumes` | int | Number of exhaust plumes |
| `trailPlumes` | int | Number of trail plumes |
| `rocketExplosionPlumeSpeed` | int | Explosion plume speed |
| `spriteBanks` | int[] | Sprite banks used by the weapon |
| `hitOverlayBank` | int | Sprite bank for hit overlay VFX |
| `primaryCount` / `secondaryCount` | int | Number of simultaneous projectiles |
| `primaryVectors` / `secondaryVectors` | `{xv,yv}[]` | Per-projectile launch vectors |

#### Sound slots (cue-capable)

`soundFire`, `soundHit1`, `soundHit2`, `soundExplosion`, `soundLand`,
`soundThrow`, `soundWarn`, `soundLoop` — all accept `"cue:name"` or filename.

---

### `enemies.json` — Enemy defs

Defines every NPC type's stats, AI behaviour, and sound slots.

#### Stats

| Key | Type | Description |
|---|---|---|
| `health` | int | Starting HP |
| `shield` | int | Starting shield |
| `speed` | int | Base movement speed |
| `speedAlt` | int | Alternative speed (patrol vs. chase) |
| `runSpeedBonus` | int | Speed bonus when chasing |
| `weapon` | int | Weapon ID |
| `shotCooldown` | int | Ticks between shots |
| `respawnSeconds` | int | Seconds before respawn |

#### Chase / detection

| Key | Type | Description |
|---|---|---|
| `chaseRangeClose` | int | Distance to stop chasing |
| `chaseRangeStop` | int | Distance at which pursuit ends |
| `chaseRangeMax` | int | Max detection range |
| `threatDetectX` / `threatDetectY` | int | Detection box dimensions |
| `lookBoxes` | `{dir,x1,x2,y1,y2}[]` | Directional line-of-sight boxes |
| `searchTicks` | int | Ticks to search after losing target |
| `searchTimeoutTicks` | int | Ticks before giving up search |
| `targetStandingHeight` | int | Target height offset for aim |

#### Melee

| Key | Type | Description |
|---|---|---|
| `meleeDamageHealth` | int | Melee HP damage |
| `meleeDamageShield` | int | Melee shield damage |
| `meleeCycleTicks` | int | Full melee cycle duration |
| `meleeDelayTicks` | int | Delay before melee hits |
| `meleeHitDuration` | int | Duration of hit stagger |
| `meleeCheckInterval` | int | How often melee range is checked |

#### Ladder & patrol

| Key | Type | Description |
|---|---|---|
| `ladderCooldown` | int | Ticks between ladder uses |
| `ladderYThreshold` | int | Vertical distance to attempt ladder |
| `ladderXTolerance` | int | Horizontal tolerance for ladder grab |
| `ladderClimbSpeed` | int | Climb speed |
| `patrolReturnProximity` | int | Distance to consider patrol point reached |
| `patrolTurnInterval` | int | Ticks between patrol direction changes |
| `standingDurationTicks` | int | Ticks to stand still during patrol |
| `walkingDurationTicks` | int | Ticks to walk during patrol |
| `returnProximity` | int | Distance to original spawn considered "home" |

#### Death & drops

| Key | Type | Description |
|---|---|---|
| `deathDropYV` | int | Upward velocity on death |
| `deathDropXVRange` | int | Horizontal spread on death |
| `deathDropFiles` | int | Files dropped on death |
| `ammoDropQuantity` | int | Ammo dropped on death |
| `deathExplosionDelayTicks` | int | Delay before death explosion |
| `deadRespawnTicks` | int | Ticks before body despawns |

#### AI behaviour

| Key | Type | Description |
|---|---|---|
| `behaviorTree` | string | Filename stem of the behavior tree to use |
| `activationTicks` | int | Ticks before first activation |
| `speakCooldownTicks` | int | Min ticks between speech sounds |
| `ambientSoundIntervalTicks` | int | Ticks between ambient sounds |
| `chaseProximityX` | int | X tolerance for "close enough to target" |
| `runDurationTicks` | int | Ticks of run before slowing |
| `sleepTicks` | int | Idle sleep duration |

#### Sound slots (cue-capable)

`soundFire`, `soundActivate`, `soundAmbient`, `soundMelee`, `soundMoveRight`,
`soundMoveLeft`, `soundDeath`, `soundHurt`, `soundAlert`

---

### `items.json` — Item defs

Defines purchasable and collectible items.

| Key | Type | Description |
|---|---|---|
| `id` | string | Item identifier |
| `enumId` | int | Internal numeric ID |
| `name` | string | Display name |
| `price` | int | Purchase price in credits |
| `repairPrice` | int | Repair cost |
| `spriteBank` / `spriteIndex` | int | Sprite for inventory display |
| `techChoice` | int | Bitmask for tech-slot category |
| `techSlots` | int | Number of tech slots consumed |
| `agencyRestriction` | int | Agency ID that can use this item; `-1` = all |
| `description` | string | Tooltip text |
| `spawnAmmo` | int | Ammo granted when spawned |
| `spawnInventoryCount` | int | Count when spawned into inventory |
| `pickupAmmo` | int | Ammo granted when picked up |
| `maxAmmo` | int | Maximum ammo capacity |
| `healAmount` | int | HP healed on use |
| `poisonDose` | int | Poison stacks on use |

---

### `gameobjects.json` — Game object defs

Defines deployable objects (walls, cannons, vents, stations) and terminals.

#### `gameObjects[]`

| Key | Type | Description |
|---|---|---|
| `id` | string | Object identifier |
| `health` / `healthMax` | int | HP and max HP |
| `shield` / `shieldMax` | int | Shield and max shield |
| `healthRegen` | int | HP regenerated per tick |
| `cooldownTicks` | int | Cooldown between uses |
| `refireReadyTick` | int | Tick when object can fire again |
| `reloadTick` | int | Reload duration |
| `innerRange` / `outerRange` | int | Inner/outer trigger ranges |
| `detectionRange` | int | Detection radius |
| `detectionWidth` / `detectionHeight` | int | Detection box dimensions |
| `techHealth` / `techShield` | int | Tech-upgrade HP/shield bonuses |
| `techPlumeYV` | int | Tech-upgraded vent plume speed |
| `downIdleTicks` | int | Ticks before idle shutdown |
| Sound slots | string | `soundDeploy`, `soundFire`, `soundDestroy`, `soundPurchase`, `soundHeal`, `soundAmbient`, `soundOpen` |
| Vent fields | int | `ventPlumeCount`, `ventActiveDuration`, `ventCycleTicks`, `ventSpreadX/Y`, `ventYOffset`, `ventBaseYV`, `ventYVRange` |

#### `terminals[]`

| Key | Type | Description |
|---|---|---|
| `id` | string | `"big"` or `"small"` |
| `juice` | int | Network juice reward |
| `files` | int | Files extracted per hack |
| `secretInfo` | int | Secret data value |
| `traceTimeBase/Medium/Extended` | int | Trace timer durations |
| `beaconTimeSecs` | int | Beacon active duration |
| `soundAmbient` / `soundHack` | string | Loop and hack sounds |
| `snapshotInterval` | int | Network snapshot frequency |
| `audioFadeMs` | int | Sound fade duration |

---

### `gameengine.json` — Engine constants

Low-level engine timing and networking parameters.  Change with caution.

| Key | Type | Description |
|---|---|---|
| `tickIntervalMs` | int | Milliseconds per game tick |
| `ticksPerSecond` | int | Game ticks per second |
| `audioStopAllFadeMs` | int | Global audio fade on stop-all |
| `nopeersTimeoutTicks` | int | Ticks before session ends with no peers |
| `maxStaleSnapshots` | int | Max queued stale snapshots before drop |
| `chatDisplayTicks` | int | Chat message display duration |
| `chatMaxLines` | int | Max visible chat lines |
| `snapshotQueueShrinkTicks` | int | Ticks before snapshot queue shrinks |
| `snapshotQueueMinSize` | int | Minimum snapshot queue size |
| `snapshotQueueInitMaxSize` | int | Initial max queue size |
| `snapshotQueueMaxCap` | int | Hard cap on snapshot queue |
| `pingIntervalMs` | int | Ping interval |
| `heartbeatIntervalTicks` | int | Connection heartbeat frequency |
| `shrapnelLifeNormal` | int | Shrapnel lifespan (normal) |
| `shrapnelLifeLaser` | int | Shrapnel lifespan (laser) |

---

### `gamemodes.json` — Game mode configs

| Key | Type | Description |
|---|---|---|
| `id` | int | Mode ID (matches map `supportedModes` bitmask) |
| `name` | string | Display name |
| `timeLimitSecs` | int | Match time limit |
| `scoreLimit` | int | Score needed to win |
| `fragLimit` | int | Kills needed to win |
| `friendlyFire` | bool | Whether team damage is on |
| `respawn` | bool | Whether players respawn |

---

### `world.json` — World parameters

| Key | Type | Description |
|---|---|---|
| `soundAmbience1/2/3` | string | Background ambient sound channels |
| `audioRange` | int | Max distance for spatial audio |
| `networkSyncRangeX/Y` | int | Object sync range |
| `grenadesyncRangeX/Y` | int | Grenade sync range |
| `illuminateLevel` | int | Base ambient light level |
| `terminalActivatePercent` | number | Fraction of terminals needed to activate |
| `terminalBigBeamMin/Range` | int | Big terminal beam timing |
| `terminalSmallBeamMin/Range` | int | Small terminal beam timing |
| `bodyPartSpawnYOffset` | int | Y offset for body part spawn |
| `bodyPartLaunchYV` | int | Body part launch velocity |
| `bodyPartVelocityRange` | int | Body part velocity spread |
| `minWallDistance` | int | Min distance from wall for placement |

---

### `effects.json` — VFX defs

| Key | Type | Description |
|---|---|---|
| `id` | string | Effect identifier |
| `name` / `description` | string | Display info |
| `bank` | int | Sprite bank |
| `frames` | int[] | Frame indices in play order |
| `fps` | int | Playback frame rate |
| `loop` | bool | Loop the effect |
| `pingPong` | bool | Reverse on completion |

---

### `lights.json` — Light defs

| Key | Type | Description |
|---|---|---|
| `id` | string | Light identifier |
| `name` / `description` | string | Display info |
| `bank` / `frame` | int | Sprite bank and frame |
| `radius` | int | Light radius in pixels |
| `intensity` | number | Brightness scalar |

---

### `abilities.json` — Ability defs *(placeholder)*

Currently empty.  Reserved for future active abilities.  Schema fields:
`id`, `displayName`, `creditCost`, `cooldownMs`, `effectType`.
Active ability parameters are in `player.json` in the meantime.

---

## Sound cue references

Any sound slot in any GAS file can hold either a plain WAV filename or a
sound cue reference.  The C++ `ResolveSound()` helper routes them:

```
"soundFire": "rocket1.wav"       → plays WAV directly
"soundFire": "cue:rocket_fire"   → evaluates the cue graph
```

See [sound-cue-system.md](sound-cue-system.md) for the full cue reference.

---

## Validation

The `shared/gas-validation` package provides JSON Schema definitions and
cross-file referential checks used by both the editor and the C++ loader.

Error shape: `{ file, instancePath, code, message }`

Error codes:
| Code | Meaning |
|---|---|
| `OPEN_FAILED` | File could not be read |
| `PARSE_ERROR` | Invalid JSON |
| `SCHEMA_ERROR` | Field type or constraint violation |
| `REFERENCE_ERROR` | ID referenced in one file not found in another |
| `FIELD_ERROR` | Semantic rule violation |

---

## C++ loader

`GASLoader` is a singleton that loads all files at startup and exposes typed
structs.

```cpp
GASLoader& gas = GASLoader::Get();
gas.Reload("/path/to/gas/");   // reload all files

// Access:
gas.player.runSpeed
gas.weapons[weaponId]
gas.enemies[enemyId]
gas.items[itemId]
gas.agencies[agencyId]
gas.gameObjects[objectId]
gas.world.audioRange
gas.engine.ticksPerSecond
```

`Reload()` resets all structs to their defaults before parsing, so removing a
key from JSON reverts to the hard-coded default rather than retaining the
previous value.

### Struct → file mapping

| C++ struct | JSON file |
|---|---|
| `PlayerDef` | `player.json` |
| `AgencyDef` | `agencies.json` |
| `WeaponDef` | `weapons.json` |
| `ItemDef` | `items.json` |
| `EnemyDef` | `enemies.json` |
| `GameObjectDef` | `gameobjects.json` (`gameObjects` array) |
| `TerminalDef` | `gameobjects.json` (`terminals` array) |
| `EffectDef` | `effects.json` |
| `LightDef` | `lights.json` |
| `AbilityDef` | `abilities.json` |
| `WorldDef` | `world.json` |
| `GameEngineDef` | `gameengine.json` |
| `GameModeConfig` | `gamemodes.json` |
