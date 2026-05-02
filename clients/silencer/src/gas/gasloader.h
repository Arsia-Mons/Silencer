#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// ---------------------------------------------------------------------------
// GASLoader — Gameplay Ability System data loader
//
// Loads JSON definitions from shared/assets/gas/ at startup. The game reads
// from these structs instead of hardcoded C++ values. During the migration
// each phase wires one category; until a category is wired the game still
// uses its own compiled-in constants.
//
// All values in the seed JSON files are 1:1 copies of the original C++
// hardcoded values — no balance changes during migration.
// ---------------------------------------------------------------------------

// ---- Agency ----------------------------------------------------------------

struct AgencyUpgradeCaps {
    uint8_t endurance  = 5;
    uint8_t shield     = 5;
    uint8_t jetpack    = 5;
    uint8_t techslots  = 8;
    uint8_t hacking    = 5;
    uint8_t contacts   = 5;
};

// Starting upgrade values granted for free at account creation.
struct AgencyDefaultUpgrades {
    uint8_t endurance = 0;
    uint8_t shield    = 0;
    uint8_t jetpack   = 0;
    uint8_t techslots = 3;  // all agencies start with 3 tech slots
    uint8_t hacking   = 0;
    uint8_t contacts  = 0;
};

struct AgencyDef {
    int                  id             = 0;
    std::string          name;
    uint8_t              defaultBonuses = 3;  // free points subtracted in TotalUpgradePointsPossible
    uint8_t              maxPlayersPerTeam = 4; // max peers that can join this team
    AgencyDefaultUpgrades defaultUpgrades;
    AgencyUpgradeCaps    upgradeCaps;

    // Ordered list of weapon IDs this agency can equip.
    // Empty = no restriction (use compiled-in weapon availability logic).
    std::vector<std::string> weapons;
};

// ---- Player ----------------------------------------------------------------

struct PlayerDef {
    int baseHealth                  = 100;
    int baseShield                  = 100;
    int baseFuel                    = 80;
    int maxFiles                    = 2800;
    int upgradeMultiplierEndurance  = 20;   // HP per endurance point
    int upgradeMultiplierShield     = 20;   // shield per shield point
    int upgradeMultiplierJetpack    = 10;   // fuel per jetpack point
    double upgradeMultiplierHacking  = 0.10; // hacking speed bonus per point
    double upgradeMultiplierContacts = 0.10; // credits bonus per point
    int maxPoisoned                 = 9;    // max simultaneous poison doses
    // Movement speeds (px/tick)
    int runSpeed                    = 14;   // normal run xvmax
    int runSpeedDisguised           = 11;   // while disguised
    int runSpeedSecret              = 11;   // carrying secret
    int runSpeedSecretDisguised     = 8;    // carrying secret + disguised
    int rollSpeed                   = 12;   // horizontal speed while rolling
    int jetpackXvMax                = 14;   // jetpack horizontal max
    int jetpackXvMaxDisguised       = 12;   // jetpack horizontal max while disguised (unused currently)
    int jetpackYvMax                = 9;    // jetpack upward velocity cap (stored positive, applied negative)
    int jetpackThrust               = 1;    // upward thrust applied to yv every 2 ticks
    int jetpackXvAccel              = 1;    // horizontal acceleration per 2 ticks when moving left/right
    int jetpackCeilingCheckRange    = 30;   // ceiling proximity test range (px above player)
    float hitKnockbackAirFactor     = 0.6f; // knockback scale for airborne players (x and y)
    // Jump impulses (applied as negative yv; stored positive)
    int jumpImpulse                 = 17;   // normal jump
    int ladderJumpImpulse           = 29;   // jump from ladder (no directional input)
    int ladderActivateImpulse       = 8;    // jump from ladder with activate held
    // Ability timers (ticks)
    int disguiseActivationTicks     = 112;  // ticks to reach fully-disguised state
    int disguiseThreshold           = 100;  // value at which player is considered fully disguised
    int disguiseDeactivationTicks   = 12;   // countdown ticks for undisguise fade-out
    int invisibilityDurationTicks   = 720;  // duration of invisibility powerup (30 * 24)
    int poisonTickCycle             = 24;   // ticks per poison damage cycle
    int hackingEffectTicks          = 5;    // ticks for hacking visual/audio effect
    int hackingCompleteThreshold    = 15;   // state_i value when hack completes
    int hackingExitThreshold        = 17;   // state_i value when player regains movement
    // ---- Audio fade durations (ms) ----------------------------------------
    int audioFadeHackMs             = 700;  // hack ambient sound fade on exit
    int audioFadeJetpackMs          = 200;  // jetpack loop fade on land/stop
    int audioFadeFlamerMs           = 200;  // flamer loop fade on release
    int deployWaitTicks             = 60;   // ticks before deployed item becomes active
    int cannonBuildCheckX           = 40;   // half-width of cannon placement exclusion AABB
    int cannonBuildCheckY           = 50;   // height of cannon placement exclusion AABB
    int startingCredits             = 500;  // credits on spawn
    int creditFloor                 = 250;  // min credits after event
    int creditCap                   = 65535; // max credits
    int neutronWarnTick             = 8;    // tracetime value that triggers detonation warning sound
    int superShieldMultiplier       = 2;    // super shield powerup: shield = maxshield * N
    int powerupRespawnTicks         = 60;   // ticks before a dropped powerup respawns
    int fileConversionBase          = 1;    // base file-to-credit conversion factor (creditamount = files * (base + creditsbonus))
    int teamGiftCredits             = 100;  // credits awarded to a teammate via BUY_GIVE actions
    int secretDeliveryCredits       = 1000; // credits awarded to each team member on secret delivery
    int weaponFireCooldownPad       = 3;    // extra ticks added to every weapon's fireDelay on each shot
    int secretsNeededToWin          = 3;    // secrets a team must deliver to win the round
    int secretProgressBeamThresh    = 180;  // secretprogress value that triggers secret-beaming sequence
    int secretProgressSoundThresh   = 20;   // min progress delta to trigger team progress sound
    // Powerup pickup durations (ticks = seconds * 24)
    int jetpackBonusDurationTicks   = 480;  // extra jetpack propellant powerup duration (20s)
    int hackingBonusDurationTicks   = 720;  // double-hacking bonus duration (30s)
    int radarBonusDurationTicks     = 720;  // radar powerup duration (30s)
    // Warp / respawn timing
    int warpDurationTicks           = 40;   // total ticks for one warp animation cycle
    int warpNonCollidableTicks      = 24;   // state_warp <= this → entity non-collidable
    int warpTeleportTick            = 12;   // state_warp == this → player x/y set to destination
    int deadAutoRespawnTick         = 48;   // ticks in DEAD state before auto-respawn triggers
    int deployAnimationTicks        = 8;    // ticks of DEPLOYING beam-in animation after deployWait completes
    // Movement physics
    int walkAcceleration            = 3;    // px/tick added per tick when walking left/right
    int standingShootDecel          = 4;    // px/tick bled off xv each tick in STANDINGSHOOT state
    int aiDisguiseInterval          = 50;   // AI: 1-in-N chance per hack-state tick to press disguise
    int aiHackInterval              = 3;    // AI: 1-in-N chance per tick at terminal to press activate
    int aiLadderJumpUpInterval      = 12;   // AI: 1-in-N chance per tick to jump while climbing up
    int aiLadderJumpDownInterval    = 5;    // AI: 1-in-N chance per tick to jump while descending
    int aiArrivalThreshold          = 8;    // AI: px distance considered close enough to target
    int deathDropXVRange            = 4;    // ±px/tick random horizontal velocity of dropped pickups
    int deathDropYV                 = 15;   // upward launch yv of dropped pickups (stored positive)
    int govtKillPlasmaXVRange       = 8;    // govt-kill plasma bolt xv scatter range (% (2*range+1) - range)
    int govtKillPlasmaYVRange       = 37;   // govt-kill plasma bolt yv scatter range (% range downward)
    int ladderSpeedReduction        = 4;    // px/tick subtracted from run speed on ladders
    int disguisedDecelSpeed         = 4;    // xv snap when decelerating while disguised
    int disguisedDecelSpeedSecret   = 2;    // xv snap when decelerating while disguised + carrying secret
    // Hacking powerup
    double hackingPowerupBonus      = 1.0;  // hacking speed bonus multiplier when powerup active
    // ---- Hittable impact sounds ------------------------------------------------
    std::string soundImpactBlaster1     = "strike03.wav";
    std::string soundImpactBlaster2     = "strike04.wav";
    std::string soundImpactLaserShield1 = "strike01.wav";
    std::string soundImpactLaserShield2 = "strike02.wav";
    std::string soundImpactLaser1       = "strike03.wav";
    std::string soundImpactLaser2       = "strike04.wav";
    std::string soundImpactFlamer       = "s_flmc01.wav";
    std::string soundShieldDown         = "shlddn1.wav";
    // ---- Player action sounds --------------------------------------------------
    std::string soundGrunt              = "grunt2a.wav";
    std::string soundDisguise           = "disguise.wav";
    std::string soundJackout            = "jackout.wav";
    std::string soundJetpack            = "jetpak2a.wav";
    std::string soundMenuSelect         = "cliksel2.wav";
    std::string soundWeaponCharged      = "charged.wav";
    std::string soundAlertWarn          = "alwarn.wav";
    std::string soundAlertInvestigate   = "alinvest.wav";
    std::string soundAmmo1              = "ammo01.wav";
    std::string soundAmmo2              = "ammo02.wav";
    std::string soundAmmo3              = "ammo03.wav";
    std::string soundAmmo4              = "ammo05.wav";
    // ---- UI / team / game sounds ------------------------------------------------
    std::string soundUIClick            = "whoom.wav";    // button/menu click
    std::string soundTeamJoin           = "select2.wav";  // player joins team
    std::string soundTeamHQ             = "cathdoor.wav"; // HQ door access
    std::string soundTeamHeal           = "if15.wav";     // teammate heals player
    std::string soundTeamHack           = "typerev6.wav"; // team hack event echo
    std::string soundRoundCountdown     = "grndown.wav";  // round countdown tick
    // ---- Player sounds (remaining) ------------------------------------------
    std::string soundUndeploy           = "transrev.wav"; // undeploy / uncloak / warp-end
    std::string soundBreath             = "breath2.wav";  // outdoor idle breath
    std::string soundFootstepCrouchL   = "futstonl.wav"; // crouching footstep left
    std::string soundFootstepCrouchR   = "futstonr.wav"; // crouching footstep right
    std::string soundFootstepStairL    = "stostep1.wav"; // stair footstep left
    std::string soundFootstepStairR    = "stostepr.wav"; // stair footstep right
    std::string soundBaseAlarm         = "alarm3a.wav";  // base intrusion alarm
    std::string soundIntrude           = "intrude.wav";  // intrusion detected
    std::string soundSecurityPass      = "portpas2.wav"; // security card swipe
    std::string soundRoll              = "roll2.wav";    // roll / dodge
    std::string soundPickup            = "juunewne.wav"; // item pickup
    std::string soundJackIn            = "jackin.wav";   // hacking jack-in
    std::string soundHackAmbient       = "ambloop5.wav"; // hacking ambient loop
    std::string soundType1             = "type1.wav";    // typing SFX variant 1
    std::string soundType2             = "type2.wav";    // typing SFX variant 2
    std::string soundType3             = "type3.wav";    // typing SFX variant 3
    std::string soundType4             = "type4.wav";    // typing SFX variant 4
    std::string soundType5             = "type5.wav";    // typing SFX variant 5
    std::string soundRepair            = "repair.wav";   // resurrection / repair
    std::string soundHurtA             = "s_hita01.wav"; // player hurt variant A
    std::string soundHurtB             = "s_hitb01.wav"; // player hurt variant B
    std::string soundLandCrouch        = "land11.wav";   // landing while crouching
    std::string soundReload            = "reload2.wav";  // weapon reload / buy
    std::string soundJetpackLoop       = "jetpak1.wav";  // jetpack looping engine
    std::string soundLand              = "land1.wav";    // standard landing
    std::string soundFall              = "fall2b.wav";   // fall impact / wall bounce
    std::string soundLadder1           = "ladder1.wav";  // ladder step 1
    std::string soundLadder2           = "ladder2.wav";  // ladder step 2
    std::string soundPowerUp           = "power11.wav";  // powerup pickup
    // ---- World physics --------------------------------------------------------
    int worldGravity      = 3;   // gravitational acceleration (px/tick²)
    int worldMaxYVelocity = 45;  // terminal falling velocity cap (px/tick)
    // ---- Network / collision --------------------------------------------------
    int playerHeight      = 50;  // collision box height (px)
    int snapshotInterval  = 24;  // network snapshot frequency (every N ticks)
    // ---- Hittable / shrapnel -------------------------------------------------
    int   shieldShrapnelThreshold = 60;  // shield HP below which shrapnel bursts spawn
    int   shrapnelCount           = 8;   // shrapnel projectiles per burst
    float shrapnelSpeed           = 4.0f;// shrapnel velocity (px/tick)
    int   flamerSoundInterval     = 4;   // play flamer impact sound every N ticks
    int   shieldEffectTicks       = 48;  // duration of shield-hit flash effect (ticks)
    int   hitSoundCooldownTicks   = 10;  // min ticks between player hit sounds
    int   fallingNudgeMax         = 8;   // max falling nudge magnitude (clamped ±)
    int   fallingNudgeXvDivisor   = 2;   // divides nudge before adding to xv
    int   hackingSoundIntervalBase  = 6; // base interval (ticks) for hacking bonus sound
    int   hackingSoundIntervalRandom = 4;// random range added to base (0..N-1)
};

// ---- Weapon ----------------------------------------------------------------

struct SpreadVector {
    int xv = 0;
    int yv = 0;
};

struct WeaponDef {
    std::string id;
    int healthDamage      = 0;
    int shieldDamage      = 0;
    // Plasma only: secondary damage values when projectile is in "large" (attached) state.
    int healthDamageLarge = 0;
    int shieldDamageLarge = 0;
    // Player weapon fire delay in ticks (blaster=7, laser=11, rocket=21, flamer=2).
    int fireDelay         = 0;
    // Grenade/bomb: throw speed and explosion timing (ticks).
    int throwSpeedStanding  = 0;  // xv when player is standing
    int throwSpeedMoving    = 0;  // xv when player is moving
    int throwSpeedRunning   = 0;  // base xv when player is running (abs(player.xv) added)
    int throwYvDown         = 5;  // yv when throwing straight down
    int throwXvDownDiag     = 25; // xv when throwing down-diagonal
    int throwYvDownDiag     = 10; // yv when throwing down-diagonal
    int throwXvUp           = 5;  // xv when throwing straight up
    int throwYvUp           = 30; // yv when throwing up (applied negative)
    int throwXvUpDiag       = 25; // xv when throwing up-diagonal
    int throwYvUpDiag       = 20; // yv when throwing up-diagonal (applied negative)
    int throwXvCrouch       = 20; // xv when throwing from crouch
    int throwYvCrouch       = 10; // yv when throwing from crouch (applied negative)
    int explosionTick       = 0;  // state_i when first explosion fires
    int secondaryTick       = 0;  // state_i when secondary shrapnel fires
    int destroyTick         = 0;  // state_i when non-special grenade is destroyed
    int neutronDestroyTick  = 0;  // state_i when neutron bomb is destroyed
    int flareDuration       = 0;  // state_i when flare/poisonflare is destroyed (30 + 168)
    int flareSpawnInterval  = 0;  // ticks between flare projectile spawns when airborne (0 = default 3)
    // Projectile physics
    int velocity            = 0;  // travel speed (px per move step)
    int moveAmount          = 0;  // collision steps per tick
    int radius              = 0;  // hit detection radius
    // Detonator / neutron bomb
    int detonatorLaunchYv   = 0;  // yv=−15 on deploy (stored positive, applied negative)
    int neutronTraceTime    = 0;  // tracetime set when neutron bomb arm completes
    float rocketSlowInitial = 0.2f; // rocket: velocity multiplier on launch (tick 0)
    int   rocketHoverTick   = 100;  // rocket: state_i value that triggers hover mode
    float rocketSlowHover   = 0.3f; // rocket: velocity multiplier when entering hover
    int   plasmaGravity     = 2;    // plasma: yv increment per tick
    int   plasmaLifeNormal  = 20;   // plasma: ticks before small plasma is destroyed
    int   plasmaLifeLarge   = 7;    // plasma: ticks before large plasma is destroyed

    // ---- Sprite banks (8-directional: up,upR,right,downR,down,downL,left,upL) ----
    // Index 0xFF means no sprite assigned. Empty vector = use compiled-in fallback.
    std::vector<int> spriteBanks;   // 8 entries when set
    int hitOverlayBank = -1;        // overlay bank on impact (-1 = none)

    // ---- Sounds (empty string = use compiled-in fallback) ----------------------
    std::string soundFire;          // on firing
    std::string soundHit1;          // on hit, variant 1
    std::string soundHit2;          // on hit, variant 2
    std::string soundLoop;          // looping (e.g. rocket engine)
    std::string soundExplosion;     // explosion
    std::string soundLand;          // landing / bounce
    std::string soundThrow;         // throw (grenade)
    std::string soundWarn;          // warning / trace alert (neutron bomb)
    int audioFadePropulsionMs = 100; // fade duration when stopping loop sound on impact (ms)

    // ---- Projectile type (for generic factory, Phase 3) -----------------------
    // Values: "physics", "arcing", "flamer", "plasma", "rocket", "wall", "grenade"
    std::string projectileType;

    // ---- Ammo (supplements items.json spawnAmmo/maxAmmo/pickupAmmo) -----------
    int ammoCapacity  = 0;   // max ammo this weapon can hold (0 = use items.json)
    int reloadTicks   = 0;   // ticks to reload one unit (0 = instant)

    // ---- Projectile physics (tunable rendering/feel) --------------------------
    int   projectileLife  = 0;    // bolt lifetime ticks after launch phase (0 = compiled default)
    int   emitOffset      = 0;    // pixel offset from player when spawning (0 = use compiled default)
    int   exhaustPlumes   = 0;    // rocket exhaust plume count (0 = use compiled default)
    int   rocketExplosionPlumeSpeed = 15; // rocket: radial speed of explosion plumes (px/tick)
    float bounceDamping   = 0.0f; // velocity multiplier on wall/floor bounce (0 = use compiled default)
    int   trailPlumes     = 0;    // flamer/flare trail plume count (0 = use compiled default)
    // ---- Grenade fan counts ------------------------------------------------
    int   primaryCount    = 0;    // shaped/plasma bomb: projectiles on first burst (0 = compiled default)
    int   secondaryCount  = 0;    // shaped/plasma bomb: projectiles on second burst (0 = compiled default)
    std::vector<SpreadVector> primaryVectors;   // per-projectile velocity for primary burst
    std::vector<SpreadVector> secondaryVectors; // per-projectile velocity for secondary burst
    // ---- Poison ------------------------------------------------------------
    int   poisonRate          = 0;    // poison amount applied per hit (0 = use compiled default 1)
    int   poisonMax           = 0;    // max poison stack (0 = use compiled default 3)
    int   poisonCheckInterval = 0;    // ticks between poison applications (0 = use compiled default 6)
    // ---- Network -----------------------------------------------------------
    int snapshotInterval  = 0;   // projectile network snapshot freq; 0 = use compiled default (6)
};

// ---- Item ------------------------------------------------------------------

struct ItemDef {
    std::string id;
    int         enumId               = 0;
    std::string name;
    int         price                = 0;
    int         repairPrice          = 0;
    int         spriteBank           = 0;
    int         spriteIndex          = 0;
    int         techChoice           = 0;   // bitmask
    int         techSlots            = 0;
    int         agencyRestriction    = -1;  // -1 = no restriction, else Team::* int
    std::string description;
    // Spawn loadout — ammo or inventory items granted at respawn if tech is unlocked.
    int         spawnAmmo            = 0;   // ammo granted (laser/rocket/flamer)
    int         spawnInventoryCount  = 0;   // inventory items granted (consumables)
    // Ammo pickup from inventory station
    int         pickupAmmo           = 0;   // ammo per purchase
    int         maxAmmo              = 0;   // ammo cap
    // Item effects
    int         healAmount           = 0;   // health restored (healthpack)
    int         poisonDose           = 0;   // poison units applied per use
};

// ---- Enemy -----------------------------------------------------------------

struct GuardLookBox {
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

struct EnemyDef {
    std::string id;
    int health = 0;
    int shield = 0;
    int speed  = 0;
    int weapon = 0;  // guard weapon variant: 0=blaster, 1=laser, 2=rocket
    // Guard AI
    int shotCooldown    = 48;   // ticks between shots (cooldowntime)
    int chaseRangeClose = 60;   // px — within this range Look5 skips shooting (too close)
    int chaseRangeStop  = 80;   // px — within this distance guard stops chasing
    int chaseRangeMax   = 90;   // px — beyond this guard actively walks toward target
    // Robot AI
    int searchTicks         = 600;  // ticks to search before returning to spawn
    int meleeCheckInterval  = 40;   // check melee every N ticks
    // Civilian tract weapon
    int tractHealthDamage = 0;  // tract projectile health damage (civilian only)
    int tractShieldDamage = 0;  // tract projectile shield damage (civilian only)
    int respawnSeconds    = 0;    // seconds before enemy respawns after death
    int ladderCooldown    = 120;  // guard: ticks between ladder re-climbs
    int meleeDamageHealth = 60;   // robot: melee health damage
    int meleeDamageShield = 60;   // robot: melee shield damage
    int returnProximity   = 20;   // robot: px distance to spawn to consider "returned"
    int sleepTicks        = 100;  // robot: ticks idle at spawn before resuming patrol
    // Guard AI thresholds
    int meleeCycleTicks       = 32;  // melee attack state_hit modulus
    int meleeDelayTicks       = 10;  // minimum state_hit within cycle to allow attack
    int targetStandingHeight  = 50;  // target AABB height >= this => standing (else crouched)
    int ladderYThreshold      = 48;  // abs(ydiff) > this to attempt ladder climb
    int ladderXTolerance      = 8;   // abs(center-x) <= this to align with ladder
    int patrolReturnProximity = 20;  // abs(x-originalx) <= this to consider returned to post
    // Civilian variant speeds
    int speedAlt              = 0;   // actortype=1 civilian speed override
    int runSpeedBonus         = 0;   // civilian: xv = speed + runSpeedBonus when fleeing
    int threatDetectX         = 200; // civilian: threat detection AABB half-width
    int threatDetectY         = 100; // civilian: threat detection AABB half-height
    int shootCooldownCap      = 50;  // robot: shootcooldown threshold for attack loop check
    int deathDropFiles        = 0;   // robot: quantity of FILES pickup spawned on death
    int ladderClimbSpeed      = 5;   // guard/robot: abs(yv) when climbing a ladder
    int rocketLaunchXv        = 25;  // robot: horizontal velocity of fired rocket projectile
    int rocketOffsetX         = 70;  // robot: horizontal distance from center for rocket spawn
    int rocketOffsetY         = 60;  // robot: height above actor for rocket spawn
    int deathDropYV           = 15;  // upward launch yv of death-drop pickup (stored positive)
    int patrolTurnInterval    = 3;   // 1-in-N chance to walk after standing (0 = never)
    int deathDropXVRange      = 4;   // ±px/tick random horizontal velocity of death-drop pickup
    int meleeHitDuration      = 24;  // robot: ticks the damaging state persists per hit
    int ammoDropQuantity      = 0;   // guard: ammo quantity dropped on death (0 = no drop)
    // Robot look-range AABB (Look() detection box)
    int lookDefaultMinX = 70;   // default detection box: near edge (x offset from robot)
    int lookDefaultMaxX = 500;  // default detection box: far edge
    int lookDefaultY    = -60;  // default detection box: y1=y2 (top/bottom of box)
    int lookDirMinX     = 70;   // directional (dir 1/2): near edge (x offset, mirrored for dir 2)
    int lookDirMaxX     = 200;  // directional: far edge
    int lookDirY1       = -10;  // directional: top of box
    int lookDirY2       = -100; // directional: bottom of box
    // ---- Enemy sounds -----------------------------------------------------------
    std::string soundFire       = "";   // ranged attack shot
    std::string soundActivate   = "";   // activation/alert sound
    std::string soundAmbient    = "";   // ambient loop (robots)
    std::string soundMelee      = "";   // melee attack swing
    std::string soundMoveRight  = "";   // footstep / movement right
    std::string soundMoveLeft   = "";   // footstep / movement left
    std::string soundDeath      = "";   // death/explosion
    std::string soundHurt1      = "";   // pain sound variant 1
    std::string soundHurt2      = "";   // pain sound variant 2
    std::string soundHurt3      = "";   // pain sound variant 3
    std::string soundAlert1     = "theres3.wav";  // guard spotted-target voice 1
    std::string soundAlert2     = "stop4.wav";    // guard spotted-target voice 2
    std::string soundAlert3     = "freeze3.wav";  // guard spotted-target voice 3
    std::string soundAlert4     = "freezrt1.wav"; // guard spotted-target voice 4
    std::string soundAlert5     = "drop4.wav";    // guard spotted-target voice 5
    int searchTimeoutTicks      = 600; // ticks guard searches before giving up (0 = never)
    int speakCooldownTicks      = 240; // guard: min ticks between alert voice lines
    int standingDurationTicks   = 48;  // guard: ticks in STANDING state before resuming patrol
    int walkingDurationTicks    = 240; // guard: ticks in WALKING state before LOOKING
    int chaseProximityX         = 60;  // guard: x-distance at which guard holds position on player
    int ambientSoundIntervalTicks = 360; // robot: mean ticks between random ambient vocalisation
    int deathExplosionDelayTicks  = 96;  // robot: ticks after DYING before death-explosion fires
    int audioFadeAmbientMs = 800; // robot: ambient loop fade duration on death/deactivate (ms)
    // ---- Network / state timers ------------------------------------------------
    int snapshotInterval  = 48;   // network snapshot frequency (0 = object default)
    int warpTeleportTick  = 12;   // state_warp value at which warp completes
    int runDurationTicks  = 150;  // civilian: ticks in RUNNING state before reverting
    int deadRespawnTicks  = 100;  // civilian: ticks in DEAD state before respawning
    std::map<int, GuardLookBox> lookBoxes; // guard: vision AABB per direction index
};

// ---- Ability ---------------------------------------------------------------

struct AbilityDef {
    std::string id;
    std::string displayName;
    int         creditCost  = 0;
    int         cooldownMs  = 0;
    std::string effectType;
};

// ---- Effect ----------------------------------------------------------------

struct EffectDef {
    std::string       id;
    std::string       name;
    std::string       description;
    int               bank      = 0;
    std::vector<int>  frames;        // frame indices in playback order
    int               fps       = 12; // playback speed (informational — no C++ consumer yet)
    bool              loop      = false;
    bool              pingPong  = false;
};

// ---- Light -----------------------------------------------------------------

struct LightDef {
    std::string id;
    std::string name;
    std::string description;
    int   bank      = 222;  // sprite bank (222 = env halo)
    int   frame     = 0;    // sprite frame index (maps to MapActor.type)
    int   radius    = 128;  // approximate visual radius in pixels (informational)
    float intensity = 1.0f; // relative brightness multiplier (informational)
};

// ---- Load errors -----------------------------------------------------------

// Surfaced via the `gas reload` control-socket op; shape matches the
// GASError type emitted by the shared TS validator
// (shared/gas-validation/errors.ts) so an agent's remediation loop is
// platform-agnostic — same {file, instancePath, code, message} regardless
// of which side caught the problem.
struct GASLoadError {
    std::string file;          // e.g. "weapons.json"
    std::string instancePath;  // RFC 6901 JSON Pointer; "" for whole-file errors
    std::string code;          // OPEN_FAILED | PARSE_ERROR | FIELD_ERROR
    std::string message;
};

// ---- Game object -----------------------------------------------------------

struct GameObjectDef {
    std::string id;
    int cooldownTicks = 0;
    int health        = 0;
    int shield        = 0;
    int shieldMax     = 0;
    int healthMax     = 0;
    int healthRegen   = 0;
    // TechStation
    int techHealth    = 0;
    int techShield    = 0;
    int techPlumeYV   = 25; // techStation: upward velocity of destruction plumes (applied negative)
    int refireReadyTick = 12;  // wall defense: state_i to trigger shot
    int reloadTick      = 60;  // wall defense: state_i to reset from DEAD state
    int innerRange      = 70;  // fixed cannon: near edge of detection box (x offset)
    int outerRange      = 300; // fixed cannon: far edge of detection box (x offset)
    int detectionRange  = 600; // wall defense: AABB half-extent for player detection
    std::string soundDeploy;   // fixedCannon: init/activate sound
    std::string soundFire;     // fixedCannon/wallDefense: shot sound
    std::string soundDestroy;  // fixedCannon/wallDefense/techStation: death/explosion sound
    std::string soundPurchase; // creditMachine: purchase confirmation
    std::string soundHeal;     // healMachine: heal sound
    std::string soundAmbient;  // vent/baseExit: ambient loop
    std::string soundOpen;     // baseDoor: door open sound
    // ---- Vent visuals -------------------------------------------------------
    int ventPlumeCount     = 4;   // plumes spawned per tick while active
    int ventActiveDuration = 18;  // ticks of plume emission per activation
    int ventCycleTicks     = 20;  // total cycle length before reset
    int ventSpreadX        = 80;  // horizontal spread width (px)
    int ventSpreadY        = 8;   // vertical position jitter (px)
    int ventYOffset        = 3;   // Y spawn offset from vent center (px)
    int ventBaseYV         = 30;  // base upward velocity (px/tick, applied negative)
    int ventYVRange        = 20;  // random additional upward velocity range
    // ---- BaseDoor detection -------------------------------------------------
    int detectionWidth     = 320; // half-width of player-detection AABB (px)
    int detectionHeight    = 240; // half-height of player-detection AABB (px)
    int downIdleTicks      = 24;  // fixedCannon: ticks in DOWN state before raising
};

// ---- Terminal ---------------------------------------------------------------

struct TerminalDef {
    std::string id;          // "big" or "small"
    int juice        = 0;    // ticks to complete hack
    int files        = 0;    // files awarded on completion
    int secretInfo   = 0;    // secret info awarded on completion
    int traceTimeBase     = 90;   // trace timer when 0 secrets hacked
    int traceTimeMedium   = 120;  // trace timer when 1 secret hacked
    int traceTimeExtended = 150;  // trace timer when 2+ secrets hacked
    int beaconTimeSecs    = 65;   // team objective: seconds for beacon countdown when this terminal is selected
    std::string soundAmbient;     // terminal: ambient hum loop ("ambloop4.wav")
    std::string soundHack;        // terminal: hacking key sound ("typerev6.wav")
    int snapshotInterval = 24;    // network snapshot frequency
    int audioFadeMs      = 1000;  // fade duration when stopping ambient sound (ms)
};

// ---- World -----------------------------------------------------------------

struct WorldDef {
    // ---- Ambience channels (played simultaneously as spatial background mix)
    std::string soundAmbience1 = "wndloopb.wav"; // outdoor wind A
    std::string soundAmbience2 = "cphum11.wav";  // indoor CPU hum
    std::string soundAmbience3 = "wndloop1.wav"; // outdoor wind B
    // ---- Audio range
    int audioRange          = 500;  // px radius for spatial audio volume update
    // ---- Network visibility / sync ranges
    int networkSyncRangeX   = 500;  // object included in snapshot within this X distance
    int networkSyncRangeY   = 450;  // object included in snapshot within this Y distance
    int grenadesyncRangeX   = 300;  // grenade/detonator sync range X
    int grenadesyncRangeY   = 300;  // grenade/detonator sync range Y
    // ---- Illumination
    int illuminateLevel     = 15;   // flare illumination level (0..15)
    // ---- Terminal activation on map load
    float terminalActivatePercent = 0.35f; // fraction of terminals to activate
    int terminalBigBeamMin        = 10;    // big terminal: min beaming seconds
    int terminalBigBeamRange      = 26;    // big terminal: random seconds added (0..range-1)
    int terminalSmallBeamMin      = 1;     // small terminal: min beaming seconds
    int terminalSmallBeamRange    = 10;    // small terminal: random seconds added (0..range-1)
    int   minWallDistance         = 35;   // min px to platform end before NPC turns around
    // Body part death spawn physics
    int bodyPartSpawnYOffset      = 50;    // px above object center where body parts spawn
    int bodyPartLaunchYV          = 20;    // upward velocity applied to body parts on death
    int bodyPartVelocityRange     = 16;    // ±px/tick random xv/yv for body part scatter
};

// ---- Game Engine -----------------------------------------------------------

struct GameEngineDef {
    int tickIntervalMs       = 42;   // ms per game tick (~24fps)
    int ticksPerSecond       = 24;   // ticks in one second (used for time-based counters)
    int audioStopAllFadeMs   = 200;  // fade duration for global StopAll() on scene change
    int nopeersTimeoutTicks  = 240;  // dedicated server: ticks with no peers before shutdown
    int heartbeatIntervalTicks = 100;// dedicated server: ticks between heartbeat sends to lobby
    int maxStaleSnapshots    = 50;   // max stale object sync packets per tick
    int chatDisplayTicks     = 255;  // ticks chat overlay is visible after a new message
    int chatMaxLines         = 5;    // max lines kept in chat history
    int snapshotQueueShrinkTicks = 73;  // ticks between automatic snapshot queue max-size reductions
    int snapshotQueueMinSize     = 1;   // minimum snapshot queue size (fixed floor)
    int snapshotQueueInitMaxSize = 2;   // initial snapshot queue max size on world start
    int snapshotQueueMaxCap      = 4;   // upper cap the adaptive max size can reach
    int pingIntervalMs       = 1000; // ms between ping packets on replica peers
    int shrapnelLifeNormal   = 20;   // ticks before normal shrapnel self-destructs
    int shrapnelLifeLaser    = 13;   // ticks before laser shrapnel (bank 110) self-destructs
};

// ---------------------------------------------------------------------------
// GASLoader singleton
// ---------------------------------------------------------------------------

class GASLoader {
public:
    static GASLoader& Get();

    // Load all JSON files from gasDir (path to shared/assets/gas/).
    // Returns true if all files parsed without error.
    // Files that are absent or malformed leave the corresponding list at
    // its compiled-in defaults — game behaviour is unchanged.
    bool Load(const std::string& gasDir);

    // Safe to call between map loads.
    void Reload(const std::string& gasDir);

    // Lookup helpers — return nullptr when id not found.
    const AgencyDef*     GetAgencyDef(int id) const;
    const WeaponDef*     GetWeaponDef(const std::string& id) const;
    const ItemDef*       GetItemDef(const std::string& id) const;
    const EnemyDef*      GetEnemyDef(const std::string& id) const;
    const AbilityDef*    GetAbilityDef(const std::string& id) const;
    const GameObjectDef* GetGameObjectDef(const std::string& id) const;
    const TerminalDef*   GetTerminalDef(const std::string& id) const;
    const EffectDef*     GetEffectDef(const std::string& id) const;
    const LightDef*      GetLightDef(const std::string& id) const;

    PlayerDef                player;
    WorldDef                 world;
    GameEngineDef            gameengine;
    std::vector<AgencyDef>   agencies;
    std::vector<WeaponDef>   weapons;
    std::vector<ItemDef>     items;
    std::vector<EnemyDef>    enemies;
    std::vector<AbilityDef>  abilities;
    std::vector<GameObjectDef> gameObjects;
    std::vector<TerminalDef>   terminals;
    std::vector<EffectDef>     effects;
    std::vector<LightDef>      lights;

    // Errors from the most recent Load() / Reload(). Cleared at the
    // start of each load. Read-only consumers should treat
    // `lastLoadErrors.empty()` as "clean".
    std::vector<GASLoadError> lastLoadErrors;

    bool loaded = false;

private:
    GASLoader() = default;
    GASLoader(const GASLoader&) = delete;
    GASLoader& operator=(const GASLoader&) = delete;
};
