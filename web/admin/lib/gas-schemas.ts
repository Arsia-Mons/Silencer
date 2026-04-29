// JSON schemas for each GAS file — used by Monaco for inline validation + hover docs.
// Field descriptions match the comments in clients/silencer/src/gas/gasloader.h.

const int = (description: string, defaultVal?: number) => ({
  type: 'integer' as const,
  description: defaultVal !== undefined ? `${description} (default: ${defaultVal})` : description,
});

const num = (description: string, defaultVal?: number) => ({
  type: 'number' as const,
  description: defaultVal !== undefined ? `${description} (default: ${defaultVal})` : description,
});

// ── player.json ──────────────────────────────────────────────────────────────

const playerSchema = {
  type: 'object',
  title: 'PlayerDef',
  description: 'Global player stats, economy, and timing constants.',
  additionalProperties: false,
  properties: {
    baseHealth:                  int('Starting health points', 100),
    maxHealth:                   int('Maximum health after upgrades', 200),
    baseShield:                  int('Starting shield points', 100),
    maxShield:                   int('Maximum shield after upgrades', 200),
    maxFiles:                    int('Maximum file count a player can carry', 999),
    speed:                       int('Base horizontal movement speed (px/tick)', 8),
    runSpeed:                    int('Run speed (no disguise, no secret)', 14),
    runSpeedDisguised:           int('Run speed while disguised', 11),
    runSpeedSecret:              int('Run speed while carrying a secret', 11),
    runSpeedSecretDisguised:     int('Run speed while disguised + carrying secret', 8),
    jetpackXvMax:                int('Max horizontal jetpack velocity (px/tick)', 14),
    jetpackYvMax:                int('Max vertical jetpack velocity (px/tick)', 9),
    jumpForce:                   int('Standing jump impulse', 17),
    airJumpForce:                int('Ladder jump impulse', 29),
    ladderJumpForce:             int('Jump-while-activating impulse off ladder', 8),
    healAmount:                  int('Health restored per healthpack use', 50),
    poisonDamage:                int('Health damage per poison tick', 2),
    poisonTickCycle:             int('Ticks between poison damage ticks', 24),
    hackingEffectTicks:          int('Ticks of hacking effect per input press', 5),
    hackingCompleteThreshold:    int('Hacking progress value that completes the hack', 15),
    hackingExitThreshold:        int('Hacking progress value that aborts and ejex the player', 17),
    disguiseActivationTicks:     int('Ticks for disguise animation to fully activate', 112),
    disguiseThreshold:           int('Disguise progress value at which disguise is active', 100),
    invisibilityDurationTicks:   int('Ticks of invisibility per powerup/ability use', 720),
    invisStepTime:               int('Ticks per step while invisible before revealing', 10),
    deployWaitTicks:             int('Ticks of deploy wait before beam-in animation', 60),
    deployAnimationTicks:        int('Ticks of DEPLOYING beam-in animation after deployWait', 8),
    neutronWarnTick:             int('Ticks before neutron detonation to show warning', 8),
    superShieldMultiplier:       int('Shield points restored per shield pickup', 2),
    powerupRespawnTicks:         int('Ticks before a dropped powerup respawns', 60),
    fileConversionBase:          int('Base in files*(base+creditsbonus) credit formula', 1),
    teamGiftCredits:             int('Credits awarded via BUY_GIVE actions to teammates', 100),
    secretDeliveryCredits:       int('Credits per team member on secret delivery', 1000),
    weaponFireCooldownPad:       int('Extra ticks added to every weapon fireDelay on fire', 3),
    startingCredits:             int('Credits each player starts with', 500),
    creditFloor:                 int('Minimum credits (cannot go below this)', 250),
    creditCap:                   int('Maximum credits a player can hold', 65535),
    secretsNeededToWin:          int('Secrets a team must deliver to win the round', 3),
    secretProgressBeamThresh:    int('secretprogress value that triggers secret-beaming sequence', 180),
    secretProgressSoundThresh:   int('Min progress delta to trigger team progress sound', 20),
    jetpackBonusDurationTicks:   int('Extra jetpack propellant powerup duration (20s @ 24 ticks/s)', 480),
    hackingBonusDurationTicks:   int('Double-hacking bonus duration (30s)', 720),
    radarBonusDurationTicks:     int('Radar powerup duration (30s)', 720),
    warpDurationTicks:           int('Total ticks for one warp animation cycle', 40),
    warpNonCollidableTicks:      int('state_warp <= this → entity non-collidable during warp', 24),
    warpTeleportTick:            int('state_warp == this → player x/y set to warp destination', 12),
    deadAutoRespawnTick:         int('Ticks in DEAD state before auto-respawn triggers', 48),
  },
};

// ── weapons.json ─────────────────────────────────────────────────────────────

const weaponDef = {
  type: 'object',
  title: 'WeaponDef',
  required: ['id'],
  properties: {
    id:                  { type: 'string', description: 'Weapon identifier (blaster, laser, rocket, flamer, flare, wall, plasma, grenade)' },
    fireDelay:           int('Ticks between shots'),
    healthDamage:        int('Direct health damage per hit'),
    shieldDamage:        int('Direct shield damage per hit'),
    healthDamageLarge:   int('Plasma large-state health damage'),
    shieldDamageLarge:   int('Plasma large-state shield damage'),
    velocity:            int('Projectile initial velocity (px/tick)'),
    moveAmount:          int('Projectile movement per tick step'),
    radius:              int('Blast/splash radius (px)'),
    launchYv:            int('Initial vertical velocity on fire'),
    // Grenade-only
    explosionTick:       int('Tick at which primary explosion triggers'),
    secondaryTick:       int('Tick at which secondary shockwave triggers'),
    destroyTick:         int('Tick at which grenade is destroyed'),
    neutronDestroyTick:  int('Tick at which neutron bomb is destroyed'),
    flareDuration:       int('Ticks a flare lasts before burning out'),
    throwXvStanding:     int('Horizontal throw speed when standing'),
    throwXvMoving:       int('Horizontal throw speed when moving (non-running)'),
    throwXvRunning:      int('Base horizontal throw speed when running (+ player xv added)'),
    throwYv:             int('Vertical throw velocity'),
    neutronTraceTime:    int('Ticks of trace effect on neutron bomb pickup'),
    detonatorLaunchYv:   int('Vertical launch velocity of detonator projectile'),
    // Rocket-only
    rocketSlowInitial:   num('Velocity multiplier on first slowdown (0–1)'),
    rocketHoverTick:     int('Tick at which rocket enters hover/slowdown phase'),
    rocketSlowHover:     num('Velocity multiplier during hover phase (0–1)'),
    splashRadius:        int('Rocket splash AABB half-size (px)'),
    // Plasma-only
    plasmaGravity:       int('Gravity applied to plasma projectile per tick'),
    plasmaLifeNormal:    int('Plasma lifetime in normal (small) state (ticks)'),
    plasmaLifeLarge:     int('Plasma lifetime in large (attached) state (ticks)'),
  },
};

const weaponsSchema = {
  type: 'object',
  title: 'Weapons',
  required: ['weapons'],
  properties: {
    weapons: { type: 'array', items: weaponDef },
  },
};

// ── enemies.json ─────────────────────────────────────────────────────────────

const enemyDef = {
  type: 'object',
  title: 'EnemyDef',
  required: ['id'],
  properties: {
    id:                   { type: 'string', description: 'Enemy identifier (guard-blaster, guard-laser, guard-rocket, robot, civilian)' },
    health:               int('Starting health'),
    shield:               int('Starting shield'),
    speed:                int('Movement speed (px/tick)'),
    weapon:               int('Guard weapon variant: 0=blaster 1=laser 2=rocket'),
    shotCooldown:         int('Ticks between guard shots (cooldowntime)', 48),
    chaseRangeClose:      int('px — within this range guard skips shooting (too close)', 60),
    chaseRangeStop:       int('px — within this distance guard stops chasing', 80),
    chaseRangeMax:        int('px — beyond this distance guard actively walks toward target', 90),
    searchTicks:          int('Ticks robot searches before returning to spawn', 600),
    meleeCheckInterval:   int('Robot checks melee every N ticks', 40),
    tractHealthDamage:    int('Civilian tract projectile health damage'),
    tractShieldDamage:    int('Civilian tract projectile shield damage'),
    respawnSeconds:       int('Seconds before enemy respawns after death'),
    ladderCooldown:       int('Guard: ticks between ladder re-climbs', 120),
    meleeDamageHealth:    int('Robot melee health damage', 60),
    meleeDamageShield:    int('Robot melee shield damage', 60),
    returnProximity:      int('Robot: px to spawn to consider "returned"', 20),
    sleepTicks:           int('Robot: ticks idle at spawn before resuming patrol', 100),
    meleeCycleTicks:      int('Guard melee attack state_hit modulus', 32),
    meleeDelayTicks:      int('Min state_hit within cycle to allow guard attack', 10),
    targetStandingHeight: int('Target AABB height >= this → standing (else crouched)', 50),
    ladderYThreshold:     int('abs(ydiff) > this to attempt ladder climb (px)', 48),
    ladderXTolerance:     int('abs(center-x) <= this to align with ladder (px)', 8),
    patrolReturnProximity:int('abs(x-originalx) <= this to consider at post (px)', 20),
    speedAlt:             int('Civilian actortype=1 speed override'),
    runSpeedBonus:        int('Civilian: xv = speed + runSpeedBonus when fleeing'),
    threatDetectX:        int('Civilian threat detection AABB half-width (px)', 200),
    threatDetectY:        int('Civilian threat detection AABB half-height (px)', 100),
    shootCooldownCap:     int('Robot: shootcooldown threshold for attack loop check', 50),
    deathDropFiles:       int('Robot: FILE pickup quantity spawned on death'),
    ladderClimbSpeed:     int('Guard/robot: abs(yv) when climbing a ladder', 5),
    rocketLaunchXv:       int('Robot: horizontal velocity of fired rocket (px/tick)', 25),
    ammoDropQuantity:     int('Guard: ammo dropped on death (0 = no drop)'),
    lookDefaultMinX:      int('Robot default look AABB: near X edge (px from robot)', 70),
    lookDefaultMaxX:      int('Robot default look AABB: far X edge (px from robot)', 500),
    lookDefaultY:         int('Robot default look AABB: Y offset (top & bottom)', -60),
    lookDirMinX:          int('Robot directional look AABB: near X edge', 70),
    lookDirMaxX:          int('Robot directional look AABB: far X edge', 200),
    lookDirY1:            int('Robot directional look AABB: top Y offset', -10),
    lookDirY2:            int('Robot directional look AABB: bottom Y offset', -100),
    _note:               { type: 'string', description: 'Optional dev note (ignored by game)' },
  },
};

const enemiesSchema = {
  type: 'object',
  title: 'Enemies',
  required: ['enemies'],
  properties: {
    enemies: { type: 'array', items: enemyDef },
  },
};

// ── agencies.json ─────────────────────────────────────────────────────────────

const upgradeLevels = { type: 'integer', minimum: 0, maximum: 10 };

const agencyDef = {
  type: 'object',
  title: 'AgencyDef',
  required: ['id', 'name'],
  properties: {
    id:                { type: 'integer', description: 'Agency numeric ID (0–4)' },
    name:              { type: 'string',  description: 'Display name' },
    defaultBonuses:    int('Free bonus points subtracted in TotalUpgradePointsPossible'),
    maxPlayersPerTeam: int('Max peers that can join this team (default: 4, Blackrose: 1)', 4),
    defaultUpgrades: {
      type: 'object',
      description: 'Free starting upgrade levels each agency receives',
      properties: {
        endurance: upgradeLevels,
        shield:    upgradeLevels,
        jetpack:   upgradeLevels,
        techslots: upgradeLevels,
        hacking:   upgradeLevels,
        contacts:  upgradeLevels,
      },
    },
    upgradeCaps: {
      type: 'object',
      description: 'Per-agency final upgrade cap values (bonuses baked in)',
      properties: {
        endurance: upgradeLevels,
        shield:    upgradeLevels,
        jetpack:   upgradeLevels,
        techslots: upgradeLevels,
        hacking:   upgradeLevels,
        contacts:  upgradeLevels,
      },
    },
  },
};

const agenciesSchema = {
  type: 'object',
  title: 'Agencies',
  required: ['agencies'],
  properties: {
    _comment:  { type: 'string' },
    agencies:  { type: 'array', items: agencyDef },
  },
};

// ── items.json ────────────────────────────────────────────────────────────────

const itemDef = {
  type: 'object',
  title: 'ItemDef',
  required: ['id'],
  properties: {
    id:            { type: 'string',  description: 'Item identifier matching BUY_* constants' },
    displayName:   { type: 'string',  description: 'Human-readable name shown in buy menu' },
    creditCost:    int('Purchase price in credits'),
    techSlotCost:  int('Tech slots consumed when equipping'),
    ammoCap:       int('Max ammo carried for this weapon (0 = not an ammo item)'),
    healAmount:    int('Health restored per use (healthpack only)'),
    poisonDose:    int('Poison units applied per use (poison item only)'),
    isWeapon:      { type: 'boolean', description: 'True if this item grants a weapon slot' },
    isAmmo:        { type: 'boolean', description: 'True if this item refills ammo' },
    isAbility:     { type: 'boolean', description: 'True if this item grants an ability' },
    abilityType:   { type: 'string',  description: 'Ability type string for ability items' },
    _note:         { type: 'string' },
  },
};

const itemsSchema = {
  type: 'object',
  title: 'Items',
  required: ['items'],
  properties: {
    items: { type: 'array', items: itemDef },
  },
};

// ── gameobjects.json ──────────────────────────────────────────────────────────

const gameObjectDef = {
  type: 'object',
  title: 'GameObjectDef',
  required: ['id'],
  properties: {
    id:              { type: 'string' },
    cooldownTicks:   int('Ticks before object can activate again'),
    health:          int('Starting health'),
    shield:          int('Starting shield'),
    healthMax:       int('Maximum health'),
    shieldMax:       int('Maximum shield'),
    healthRegen:     int('Health restored per upgrade level'),
    techHealth:      int('Tech station health value'),
    techShield:      int('Tech station shield value'),
    refireReadyTick: int('Wall defense: state_i when shot fires', 12),
    reloadTick:      int('Wall defense: state_i to reset from DEAD state', 60),
    innerRange:      int('Fixed cannon: near edge of detection box X offset (px)', 70),
    outerRange:      int('Fixed cannon: far edge of detection box X offset (px)', 300),
    detectionRange:  int('Wall defense: AABB half-extent for player detection (px)', 600),
    _note:           { type: 'string' },
  },
};

const terminalDef = {
  type: 'object',
  title: 'TerminalDef',
  required: ['id'],
  properties: {
    id:                  { type: 'string', description: '"big" or "small"' },
    juice:               int('Ticks to complete terminal hack'),
    files:               int('Files awarded on hack completion'),
    secretInfo:          int('Secret info awarded on hack completion'),
    traceTimeBase:       int('Trace timer with 0 secrets hacked', 90),
    traceTimeMedium:     int('Trace timer with 1 secret hacked', 120),
    traceTimeExtended:   int('Trace timer with 2+ secrets hacked', 150),
    beaconTimeSecs:      int('Seconds for beacon countdown when terminal selected (big only)', 65),
  },
};

const gameObjectsSchema = {
  type: 'object',
  title: 'GameObjects',
  properties: {
    objects:   { type: 'array', items: gameObjectDef },
    terminals: { type: 'array', items: terminalDef },
  },
};

// ── abilities.json ────────────────────────────────────────────────────────────

const abilityDef = {
  type: 'object',
  title: 'AbilityDef',
  description: 'Reserved for future dedicated ability system. Currently empty.',
  required: ['id'],
  properties: {
    id:          { type: 'string' },
    displayName: { type: 'string' },
    creditCost:  int('Credit cost to activate'),
    cooldownMs:  int('Cooldown in milliseconds'),
    effectType:  { type: 'string', description: 'Effect type identifier' },
  },
};

const abilitiesSchema = {
  type: 'object',
  title: 'Abilities',
  description: 'Placeholder for a future dedicated ability system. All ability values currently live in player.json.',
  properties: {
    _comment:  { type: 'string' },
    abilities: { type: 'array', items: abilityDef },
  },
};

// ── Export ────────────────────────────────────────────────────────────────────

export const GAS_SCHEMAS: Record<string, { uri: string; schema: object }> = {
  player:      { uri: 'inmemory://gas/player.json',      schema: playerSchema },
  weapons:     { uri: 'inmemory://gas/weapons.json',     schema: weaponsSchema },
  enemies:     { uri: 'inmemory://gas/enemies.json',     schema: enemiesSchema },
  agencies:    { uri: 'inmemory://gas/agencies.json',    schema: agenciesSchema },
  items:       { uri: 'inmemory://gas/items.json',       schema: itemsSchema },
  gameobjects: { uri: 'inmemory://gas/gameobjects.json', schema: gameObjectsSchema },
  abilities:   { uri: 'inmemory://gas/abilities.json',   schema: abilitiesSchema },
};
