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
};

// ---- Weapon ----------------------------------------------------------------

struct WeaponDef {
    std::string id;
    int healthDamage = 0;
    int shieldDamage = 0;
};

// ---- Item ------------------------------------------------------------------

struct ItemDef {
    std::string id;
    int         enumId            = 0;
    std::string name;
    int         price             = 0;
    int         repairPrice       = 0;
    int         spriteBank        = 0;
    int         spriteIndex       = 0;
    int         techChoice        = 0;   // bitmask
    int         techSlots         = 0;
    int         agencyRestriction = -1;  // -1 = no restriction, else Team::* int
    std::string description;
};

// ---- Enemy -----------------------------------------------------------------

struct EnemyDef {
    std::string id;
    int health = 0;
    int speed  = 0;
    int weapon = 0;  // guard weapon variant: 0=blaster, 1=laser, 2=rocket
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
