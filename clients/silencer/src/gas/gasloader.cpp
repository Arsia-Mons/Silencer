#include "gasloader.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

GASLoader& GASLoader::Get() {
    static GASLoader instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool OpenJson(const std::string& path, json& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        fprintf(stderr, "[gas] cannot open %s (using compiled-in defaults)\n", path.c_str());
        return false;
    }
    try {
        f >> out;
        return true;
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] parse error in %s: %s (using compiled-in defaults)\n",
                path.c_str(), e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// Per-file parsers
// ---------------------------------------------------------------------------

static void LoadPlayer(const std::string& dir, PlayerDef& out) {
    json j;
    if (!OpenJson(dir + "/player.json", j)) return;
    try {
        out.baseHealth                 = j.value("baseHealth",                 out.baseHealth);
        out.baseShield                 = j.value("baseShield",                 out.baseShield);
        out.baseFuel                   = j.value("baseFuel",                   out.baseFuel);
        out.maxFiles                   = j.value("maxFiles",                   out.maxFiles);
        out.upgradeMultiplierEndurance = j.value("upgradeMultiplierEndurance", out.upgradeMultiplierEndurance);
        out.upgradeMultiplierShield    = j.value("upgradeMultiplierShield",    out.upgradeMultiplierShield);
        out.upgradeMultiplierJetpack   = j.value("upgradeMultiplierJetpack",   out.upgradeMultiplierJetpack);
        out.upgradeMultiplierHacking   = j.value("upgradeMultiplierHacking",   out.upgradeMultiplierHacking);
        out.upgradeMultiplierContacts  = j.value("upgradeMultiplierContacts",  out.upgradeMultiplierContacts);
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] player.json field error: %s\n", e.what());
    }
}

static void LoadAgencies(const std::string& dir, std::vector<AgencyDef>& out) {
    json j;
    if (!OpenJson(dir + "/agencies.json", j)) return;
    try {
        out.clear();
        for (const auto& aj : j.at("agencies")) {
            AgencyDef a;
            a.id             = aj.value("id", 0);
            a.name           = aj.value("name", std::string{});
            a.defaultBonuses = aj.value("defaultBonuses", a.defaultBonuses);
            if (aj.contains("defaultUpgrades")) {
                const auto& du = aj["defaultUpgrades"];
                a.defaultUpgrades.endurance = du.value("endurance", a.defaultUpgrades.endurance);
                a.defaultUpgrades.shield    = du.value("shield",    a.defaultUpgrades.shield);
                a.defaultUpgrades.jetpack   = du.value("jetpack",   a.defaultUpgrades.jetpack);
                a.defaultUpgrades.techslots = du.value("techslots", a.defaultUpgrades.techslots);
                a.defaultUpgrades.hacking   = du.value("hacking",   a.defaultUpgrades.hacking);
                a.defaultUpgrades.contacts  = du.value("contacts",  a.defaultUpgrades.contacts);
            }
            if (aj.contains("upgradeCaps")) {
                const auto& caps = aj["upgradeCaps"];
                a.upgradeCaps.endurance  = caps.value("endurance",  a.upgradeCaps.endurance);
                a.upgradeCaps.shield     = caps.value("shield",     a.upgradeCaps.shield);
                a.upgradeCaps.jetpack    = caps.value("jetpack",    a.upgradeCaps.jetpack);
                a.upgradeCaps.techslots  = caps.value("techslots",  a.upgradeCaps.techslots);
                a.upgradeCaps.hacking    = caps.value("hacking",    a.upgradeCaps.hacking);
                a.upgradeCaps.contacts   = caps.value("contacts",   a.upgradeCaps.contacts);
            }
            out.push_back(std::move(a));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] agencies.json field error: %s\n", e.what());
        out.clear();
    }
}

static void LoadWeapons(const std::string& dir, std::vector<WeaponDef>& out) {
    json j;
    if (!OpenJson(dir + "/weapons.json", j)) return;
    try {
        out.clear();
        for (const auto& wj : j.at("weapons")) {
            WeaponDef w;
            w.id               = wj.value("id",                std::string{});
            w.healthDamage     = wj.value("healthDamage",       0);
            w.shieldDamage     = wj.value("shieldDamage",       0);
            w.healthDamageLarge = wj.value("healthDamageLarge", 0);
            w.shieldDamageLarge = wj.value("shieldDamageLarge", 0);
            out.push_back(std::move(w));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] weapons.json field error: %s\n", e.what());
        out.clear();
    }
}

static void LoadItems(const std::string& dir, std::vector<ItemDef>& out) {
    json j;
    if (!OpenJson(dir + "/items.json", j)) return;
    try {
        out.clear();
        for (const auto& ij : j.at("items")) {
            ItemDef item;
            item.id                = ij.value("id",                std::string{});
            item.enumId            = ij.value("enumId",            0);
            item.name              = ij.value("name",              std::string{});
            item.price             = ij.value("price",             0);
            item.repairPrice       = ij.value("repairPrice",       0);
            item.spriteBank        = ij.value("spriteBank",        0);
            item.spriteIndex       = ij.value("spriteIndex",       0);
            item.techChoice        = ij.value("techChoice",        0);
            item.techSlots         = ij.value("techSlots",         0);
            item.agencyRestriction = ij.value("agencyRestriction", -1);
            item.description       = ij.value("description",      std::string{});
            out.push_back(std::move(item));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] items.json field error: %s\n", e.what());
        out.clear();
    }
}

static void LoadEnemies(const std::string& dir, std::vector<EnemyDef>& out) {
    json j;
    if (!OpenJson(dir + "/enemies.json", j)) return;
    try {
        out.clear();
        for (const auto& ej : j.at("enemies")) {
            EnemyDef e;
            e.id     = ej.value("id",     std::string{});
            e.health = ej.value("health", 0);
            e.speed  = ej.value("speed",  0);
            e.weapon = ej.value("weapon", 0);
            out.push_back(std::move(e));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] enemies.json field error: %s\n", e.what());
        out.clear();
    }
}

static void LoadAbilities(const std::string& dir, std::vector<AbilityDef>& out) {
    json j;
    if (!OpenJson(dir + "/abilities.json", j)) return;
    try {
        out.clear();
        for (const auto& aj : j.at("abilities")) {
            AbilityDef a;
            a.id          = aj.value("id",          std::string{});
            a.displayName = aj.value("displayName", std::string{});
            a.creditCost  = aj.value("creditCost",  0);
            a.cooldownMs  = aj.value("cooldownMs",  0);
            a.effectType  = aj.value("effectType",  std::string{});
            out.push_back(std::move(a));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] abilities.json field error: %s\n", e.what());
        out.clear();
    }
}

static void LoadGameObjects(const std::string& dir, std::vector<GameObjectDef>& out) {
    json j;
    if (!OpenJson(dir + "/gameobjects.json", j)) return;
    try {
        out.clear();
        for (const auto& gj : j.at("gameObjects")) {
            GameObjectDef g;
            g.id            = gj.value("id",            std::string{});
            g.cooldownTicks = gj.value("cooldownTicks",  0);
            g.health        = gj.value("health",         0);
            g.shield        = gj.value("shield",         0);
            g.healthMax     = gj.value("healthMax",      0);
            g.healthRegen   = gj.value("healthRegen",    0);
            out.push_back(std::move(g));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] gameobjects.json field error: %s\n", e.what());
        out.clear();
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool GASLoader::Load(const std::string& gasDir) {
    LoadPlayer(gasDir, player);
    LoadAgencies(gasDir, agencies);
    LoadWeapons(gasDir, weapons);
    LoadItems(gasDir, items);
    LoadEnemies(gasDir, enemies);
    LoadAbilities(gasDir, abilities);
    LoadGameObjects(gasDir, gameObjects);
    loaded = true;
    fprintf(stderr, "[gas] loaded: %zu agencies, %zu weapons, %zu items, %zu enemies, %zu abilities, %zu gameObjects\n",
            agencies.size(), weapons.size(), items.size(),
            enemies.size(), abilities.size(), gameObjects.size());
    return true;
}

void GASLoader::Reload(const std::string& gasDir) {
    agencies.clear();
    weapons.clear();
    items.clear();
    enemies.clear();
    abilities.clear();
    gameObjects.clear();
    loaded = false;
    Load(gasDir);
}

// ---------------------------------------------------------------------------
// Lookup helpers
// ---------------------------------------------------------------------------

const AgencyDef* GASLoader::GetAgencyDef(int id) const {
    for (const auto& a : agencies)
        if (a.id == id) return &a;
    return nullptr;
}

const WeaponDef* GASLoader::GetWeaponDef(const std::string& id) const {
    for (const auto& w : weapons)
        if (w.id == id) return &w;
    return nullptr;
}

const ItemDef* GASLoader::GetItemDef(const std::string& id) const {
    for (const auto& item : items)
        if (item.id == id) return &item;
    return nullptr;
}

const EnemyDef* GASLoader::GetEnemyDef(const std::string& id) const {
    for (const auto& e : enemies)
        if (e.id == id) return &e;
    return nullptr;
}

const AbilityDef* GASLoader::GetAbilityDef(const std::string& id) const {
    for (const auto& a : abilities)
        if (a.id == id) return &a;
    return nullptr;
}

const GameObjectDef* GASLoader::GetGameObjectDef(const std::string& id) const {
    for (const auto& g : gameObjects)
        if (g.id == id) return &g;
    return nullptr;
}
