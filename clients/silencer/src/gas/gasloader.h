#pragma once
#include <string>
#include <vector>
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
    AgencyDefaultUpgrades defaultUpgrades;
    AgencyUpgradeCaps    upgradeCaps;
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

    PlayerDef                player;
    std::vector<AgencyDef>   agencies;
    std::vector<WeaponDef>   weapons;
    std::vector<ItemDef>     items;
    std::vector<EnemyDef>    enemies;
    std::vector<AbilityDef>  abilities;
    std::vector<GameObjectDef> gameObjects;

    bool loaded = false;

private:
    GASLoader() = default;
    GASLoader(const GASLoader&) = delete;
    GASLoader& operator=(const GASLoader&) = delete;
};
