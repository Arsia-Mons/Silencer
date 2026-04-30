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
    int jetpackXvMax                = 14;   // jetpack horizontal max
    int jetpackXvMaxDisguised       = 12;   // jetpack horizontal max while disguised (unused currently)
    int jetpackYvMax                = 9;    // jetpack upward velocity cap (stored positive, applied negative)
    // Jump impulses (applied as negative yv; stored positive)
    int jumpImpulse                 = 17;   // normal jump
    int ladderJumpImpulse           = 29;   // jump from ladder (no directional input)
    int ladderActivateImpulse       = 8;    // jump from ladder with activate held
    // Ability timers (ticks)
    int disguiseActivationTicks     = 112;  // ticks to reach fully-disguised state
    int disguiseThreshold           = 100;  // value at which player is considered fully disguised
    int invisibilityDurationTicks   = 720;  // duration of invisibility powerup (30 * 24)
    int poisonTickCycle             = 24;   // ticks per poison damage cycle
    int hackingEffectTicks          = 5;    // ticks for hacking visual/audio effect
    int hackingCompleteThreshold    = 15;   // state_i value when hack completes
    int hackingExitThreshold        = 17;   // state_i value when player regains movement
    int deployWaitTicks             = 60;   // ticks before deployed item becomes active
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
    // ---- World physics --------------------------------------------------------
    int worldGravity      = 3;   // gravitational acceleration (px/tick²)
    int worldMaxYVelocity = 45;  // terminal falling velocity cap (px/tick)
    // ---- Network / collision --------------------------------------------------
    int playerHeight      = 50;  // collision box height (px)
    int snapshotInterval  = 24;  // network snapshot frequency (every N ticks)
};

// ---- Weapon ----------------------------------------------------------------

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
    int explosionTick       = 0;  // state_i when first explosion fires
    int secondaryTick       = 0;  // state_i when secondary shrapnel fires
    int destroyTick         = 0;  // state_i when non-special grenade is destroyed
    int neutronDestroyTick  = 0;  // state_i when neutron bomb is destroyed
    int flareDuration       = 0;  // state_i when flare/poisonflare is destroyed (30 + 168)
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
    float bounceDamping   = 0.0f; // velocity multiplier on wall/floor bounce (0 = use compiled default)
    int   trailPlumes     = 0;    // flamer/flare trail plume count (0 = use compiled default)
    // ---- Grenade fan counts ------------------------------------------------
    int   primaryCount    = 0;    // shaped/plasma bomb: projectiles on first burst (0 = compiled default)
    int   secondaryCount  = 0;    // shaped/plasma bomb: projectiles on second burst (0 = compiled default)
    // ---- Poison ------------------------------------------------------------
    int   poisonRate      = 0;    // poison amount applied per hit (0 = use compiled default 1)
    int   poisonMax       = 0;    // max poison stack (0 = use compiled default 3)
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

    PlayerDef                player;
    std::vector<AgencyDef>   agencies;
    std::vector<WeaponDef>   weapons;
    std::vector<ItemDef>     items;
    std::vector<EnemyDef>    enemies;
    std::vector<AbilityDef>  abilities;
    std::vector<GameObjectDef> gameObjects;
    std::vector<TerminalDef>   terminals;

    bool loaded = false;

private:
    GASLoader() = default;
    GASLoader(const GASLoader&) = delete;
    GASLoader& operator=(const GASLoader&) = delete;
};
