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
        out.maxPoisoned                = j.value("maxPoisoned",                out.maxPoisoned);
        out.runSpeed                   = j.value("runSpeed",                   out.runSpeed);
        out.runSpeedDisguised          = j.value("runSpeedDisguised",          out.runSpeedDisguised);
        out.runSpeedSecret             = j.value("runSpeedSecret",             out.runSpeedSecret);
        out.runSpeedSecretDisguised    = j.value("runSpeedSecretDisguised",    out.runSpeedSecretDisguised);
        out.jetpackXvMax               = j.value("jetpackXvMax",               out.jetpackXvMax);
        out.jetpackXvMaxDisguised      = j.value("jetpackXvMaxDisguised",      out.jetpackXvMaxDisguised);
        out.jetpackYvMax               = j.value("jetpackYvMax",               out.jetpackYvMax);
        out.jumpImpulse                = j.value("jumpImpulse",                out.jumpImpulse);
        out.ladderJumpImpulse          = j.value("ladderJumpImpulse",          out.ladderJumpImpulse);
        out.ladderActivateImpulse      = j.value("ladderActivateImpulse",      out.ladderActivateImpulse);
        out.disguiseActivationTicks    = j.value("disguiseActivationTicks",    out.disguiseActivationTicks);
        out.disguiseThreshold          = j.value("disguiseThreshold",          out.disguiseThreshold);
        out.invisibilityDurationTicks  = j.value("invisibilityDurationTicks",  out.invisibilityDurationTicks);
        out.poisonTickCycle            = j.value("poisonTickCycle",            out.poisonTickCycle);
        out.hackingEffectTicks         = j.value("hackingEffectTicks",         out.hackingEffectTicks);
        out.hackingCompleteThreshold   = j.value("hackingCompleteThreshold",   out.hackingCompleteThreshold);
        out.hackingExitThreshold       = j.value("hackingExitThreshold",       out.hackingExitThreshold);
        out.deployWaitTicks            = j.value("deployWaitTicks",            out.deployWaitTicks);
        out.startingCredits            = j.value("startingCredits",            out.startingCredits);
        out.creditFloor                = j.value("creditFloor",                out.creditFloor);
        out.creditCap                  = j.value("creditCap",                  out.creditCap);
        out.neutronWarnTick            = j.value("neutronWarnTick",            out.neutronWarnTick);
        out.superShieldMultiplier      = j.value("superShieldMultiplier",      out.superShieldMultiplier);
        out.powerupRespawnTicks        = j.value("powerupRespawnTicks",        out.powerupRespawnTicks);
        out.fileConversionBase         = j.value("fileConversionBase",         out.fileConversionBase);
        out.teamGiftCredits            = j.value("teamGiftCredits",            out.teamGiftCredits);
        out.secretDeliveryCredits      = j.value("secretDeliveryCredits",      out.secretDeliveryCredits);
        out.weaponFireCooldownPad      = j.value("weaponFireCooldownPad",      out.weaponFireCooldownPad);
        out.secretsNeededToWin         = j.value("secretsNeededToWin",         out.secretsNeededToWin);
        out.secretProgressBeamThresh   = j.value("secretProgressBeamThresh",   out.secretProgressBeamThresh);
        out.secretProgressSoundThresh  = j.value("secretProgressSoundThresh",  out.secretProgressSoundThresh);
        out.jetpackBonusDurationTicks  = j.value("jetpackBonusDurationTicks",  out.jetpackBonusDurationTicks);
        out.hackingBonusDurationTicks  = j.value("hackingBonusDurationTicks",  out.hackingBonusDurationTicks);
        out.radarBonusDurationTicks    = j.value("radarBonusDurationTicks",    out.radarBonusDurationTicks);
        out.warpDurationTicks          = j.value("warpDurationTicks",          out.warpDurationTicks);
        out.warpNonCollidableTicks     = j.value("warpNonCollidableTicks",     out.warpNonCollidableTicks);
        out.warpTeleportTick           = j.value("warpTeleportTick",           out.warpTeleportTick);
        out.deadAutoRespawnTick        = j.value("deadAutoRespawnTick",        out.deadAutoRespawnTick);
        out.deployAnimationTicks       = j.value("deployAnimationTicks",       out.deployAnimationTicks);
        out.soundImpactBlaster1        = j.value("soundImpactBlaster1",        out.soundImpactBlaster1);
        out.soundImpactBlaster2        = j.value("soundImpactBlaster2",        out.soundImpactBlaster2);
        out.soundImpactLaserShield1    = j.value("soundImpactLaserShield1",    out.soundImpactLaserShield1);
        out.soundImpactLaserShield2    = j.value("soundImpactLaserShield2",    out.soundImpactLaserShield2);
        out.soundImpactLaser1          = j.value("soundImpactLaser1",          out.soundImpactLaser1);
        out.soundImpactLaser2          = j.value("soundImpactLaser2",          out.soundImpactLaser2);
        out.soundImpactFlamer          = j.value("soundImpactFlamer",          out.soundImpactFlamer);
        out.soundShieldDown            = j.value("soundShieldDown",            out.soundShieldDown);
        out.soundGrunt                 = j.value("soundGrunt",                 out.soundGrunt);
        out.soundDisguise              = j.value("soundDisguise",              out.soundDisguise);
        out.soundJackout               = j.value("soundJackout",               out.soundJackout);
        out.soundJetpack               = j.value("soundJetpack",               out.soundJetpack);
        out.soundMenuSelect            = j.value("soundMenuSelect",            out.soundMenuSelect);
        out.soundWeaponCharged         = j.value("soundWeaponCharged",         out.soundWeaponCharged);
        out.soundAlertWarn             = j.value("soundAlertWarn",             out.soundAlertWarn);
        out.soundAlertInvestigate      = j.value("soundAlertInvestigate",      out.soundAlertInvestigate);
        out.soundAmmo1                 = j.value("soundAmmo1",                 out.soundAmmo1);
        out.soundAmmo2                 = j.value("soundAmmo2",                 out.soundAmmo2);
        out.soundAmmo3                 = j.value("soundAmmo3",                 out.soundAmmo3);
        out.soundAmmo4                 = j.value("soundAmmo4",                 out.soundAmmo4);
        out.worldGravity               = j.value("worldGravity",               out.worldGravity);
        out.worldMaxYVelocity          = j.value("worldMaxYVelocity",          out.worldMaxYVelocity);
        out.playerHeight               = j.value("playerHeight",               out.playerHeight);
        out.snapshotInterval           = j.value("snapshotInterval",           out.snapshotInterval);
        out.soundUIClick        = j.value("soundUIClick",        out.soundUIClick);
        out.soundTeamJoin       = j.value("soundTeamJoin",       out.soundTeamJoin);
        out.soundTeamHQ         = j.value("soundTeamHQ",         out.soundTeamHQ);
        out.soundTeamHeal       = j.value("soundTeamHeal",       out.soundTeamHeal);
        out.soundTeamHack       = j.value("soundTeamHack",       out.soundTeamHack);
        out.soundRoundCountdown = j.value("soundRoundCountdown", out.soundRoundCountdown);
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
            a.defaultBonuses      = aj.value("defaultBonuses",      a.defaultBonuses);
            a.maxPlayersPerTeam   = aj.value("maxPlayersPerTeam",   a.maxPlayersPerTeam);
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
            if (aj.contains("weapons") && aj["weapons"].is_array()) {
                for (const auto& wid : aj["weapons"]) {
                    a.weapons.push_back(wid.get<std::string>());
                }
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
            w.fireDelay           = wj.value("fireDelay",           0);
            w.throwSpeedStanding  = wj.value("throwSpeedStanding",  0);
            w.throwSpeedMoving    = wj.value("throwSpeedMoving",    0);
            w.throwSpeedRunning   = wj.value("throwSpeedRunning",   0);
            w.explosionTick       = wj.value("explosionTick",       0);
            w.secondaryTick       = wj.value("secondaryTick",       0);
            w.destroyTick         = wj.value("destroyTick",         0);
            w.neutronDestroyTick  = wj.value("neutronDestroyTick",  0);
            w.flareDuration       = wj.value("flareDuration",       0);
            w.velocity            = wj.value("velocity",            0);
            w.moveAmount          = wj.value("moveAmount",          0);
            w.radius              = wj.value("radius",              0);
            w.detonatorLaunchYv   = wj.value("detonatorLaunchYv",  0);
            w.neutronTraceTime    = wj.value("neutronTraceTime",    0);
            w.rocketSlowInitial   = wj.value("rocketSlowInitial",   w.rocketSlowInitial);
            w.rocketHoverTick     = wj.value("rocketHoverTick",     w.rocketHoverTick);
            w.rocketSlowHover     = wj.value("rocketSlowHover",     w.rocketSlowHover);
            w.plasmaGravity       = wj.value("plasmaGravity",       w.plasmaGravity);
            w.plasmaLifeNormal    = wj.value("plasmaLifeNormal",    w.plasmaLifeNormal);
            w.plasmaLifeLarge     = wj.value("plasmaLifeLarge",     w.plasmaLifeLarge);
            w.projectileType  = wj.value("projectileType", std::string{});
            w.hitOverlayBank  = wj.value("hitOverlayBank", -1);
            w.soundFire       = wj.value("soundFire",      std::string{});
            w.soundHit1       = wj.value("soundHit1",      std::string{});
            w.soundHit2       = wj.value("soundHit2",      std::string{});
            w.soundLoop       = wj.value("soundLoop",      std::string{});
            w.soundExplosion  = wj.value("soundExplosion", std::string{});
            w.soundLand       = wj.value("soundLand",      std::string{});
            w.soundThrow      = wj.value("soundThrow",     std::string{});
            w.soundWarn       = wj.value("soundWarn",      std::string{});
            w.ammoCapacity    = wj.value("ammoCapacity",   0);
            w.reloadTicks     = wj.value("reloadTicks",    0);
            w.projectileLife  = wj.value("projectileLife",  0);
            w.emitOffset      = wj.value("emitOffset",      0);
            w.exhaustPlumes   = wj.value("exhaustPlumes",   0);
            w.bounceDamping   = wj.value("bounceDamping",   0.0f);
            w.trailPlumes     = wj.value("trailPlumes",     0);
            w.primaryCount    = wj.value("primaryCount",    0);
            w.secondaryCount  = wj.value("secondaryCount",  0);
            w.poisonRate      = wj.value("poisonRate",      0);
            w.poisonMax       = wj.value("poisonMax",       0);
            w.snapshotInterval = wj.value("snapshotInterval", 0);
            if (wj.contains("spriteBanks") && wj["spriteBanks"].is_array()) {
                for (const auto& b : wj["spriteBanks"]) {
                    w.spriteBanks.push_back(b.get<int>());
                }
            }
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
            item.spawnAmmo           = ij.value("spawnAmmo",           0);
            item.spawnInventoryCount = ij.value("spawnInventoryCount", 0);
            item.pickupAmmo          = ij.value("pickupAmmo",          0);
            item.maxAmmo             = ij.value("maxAmmo",             0);
            item.healAmount          = ij.value("healAmount",          0);
            item.poisonDose          = ij.value("poisonDose",          0);
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
            e.shield = ej.value("shield", 0);
            e.speed  = ej.value("speed",  0);
            e.weapon = ej.value("weapon", 0);
            e.shotCooldown       = ej.value("shotCooldown",       e.shotCooldown);
            e.chaseRangeClose    = ej.value("chaseRangeClose",    e.chaseRangeClose);
            e.chaseRangeStop     = ej.value("chaseRangeStop",     e.chaseRangeStop);
            e.chaseRangeMax      = ej.value("chaseRangeMax",      e.chaseRangeMax);
            e.searchTicks        = ej.value("searchTicks",        e.searchTicks);
            e.meleeCheckInterval = ej.value("meleeCheckInterval", e.meleeCheckInterval);
            e.tractHealthDamage  = ej.value("tractHealthDamage",  e.tractHealthDamage);
            e.tractShieldDamage  = ej.value("tractShieldDamage",  e.tractShieldDamage);
            e.respawnSeconds     = ej.value("respawnSeconds",     e.respawnSeconds);
            e.ladderCooldown     = ej.value("ladderCooldown",     e.ladderCooldown);
            e.meleeDamageHealth  = ej.value("meleeDamageHealth",  e.meleeDamageHealth);
            e.meleeDamageShield  = ej.value("meleeDamageShield",  e.meleeDamageShield);
            e.returnProximity    = ej.value("returnProximity",    e.returnProximity);
            e.sleepTicks         = ej.value("sleepTicks",         e.sleepTicks);
            e.meleeCycleTicks       = ej.value("meleeCycleTicks",       e.meleeCycleTicks);
            e.meleeDelayTicks       = ej.value("meleeDelayTicks",       e.meleeDelayTicks);
            e.targetStandingHeight  = ej.value("targetStandingHeight",  e.targetStandingHeight);
            e.ladderYThreshold      = ej.value("ladderYThreshold",      e.ladderYThreshold);
            e.ladderXTolerance      = ej.value("ladderXTolerance",      e.ladderXTolerance);
            e.patrolReturnProximity = ej.value("patrolReturnProximity", e.patrolReturnProximity);
            e.speedAlt              = ej.value("speedAlt",              e.speedAlt);
            e.runSpeedBonus         = ej.value("runSpeedBonus",         e.runSpeedBonus);
            e.threatDetectX         = ej.value("threatDetectX",         e.threatDetectX);
            e.threatDetectY         = ej.value("threatDetectY",         e.threatDetectY);
            e.shootCooldownCap      = ej.value("shootCooldownCap",      e.shootCooldownCap);
            e.deathDropFiles        = ej.value("deathDropFiles",        e.deathDropFiles);
            e.ladderClimbSpeed      = ej.value("ladderClimbSpeed",      e.ladderClimbSpeed);
            e.rocketLaunchXv        = ej.value("rocketLaunchXv",        e.rocketLaunchXv);
            e.ammoDropQuantity      = ej.value("ammoDropQuantity",      e.ammoDropQuantity);
            e.lookDefaultMinX       = ej.value("lookDefaultMinX",       e.lookDefaultMinX);
            e.lookDefaultMaxX       = ej.value("lookDefaultMaxX",       e.lookDefaultMaxX);
            e.lookDefaultY          = ej.value("lookDefaultY",          e.lookDefaultY);
            e.lookDirMinX           = ej.value("lookDirMinX",           e.lookDirMinX);
            e.lookDirMaxX           = ej.value("lookDirMaxX",           e.lookDirMaxX);
            e.lookDirY1             = ej.value("lookDirY1",             e.lookDirY1);
            e.lookDirY2             = ej.value("lookDirY2",             e.lookDirY2);
            e.soundFire      = ej.value("soundFire",      std::string{});
            e.soundActivate  = ej.value("soundActivate",  std::string{});
            e.soundAmbient   = ej.value("soundAmbient",   std::string{});
            e.soundMelee     = ej.value("soundMelee",     std::string{});
            e.soundMoveRight = ej.value("soundMoveRight", std::string{});
            e.soundMoveLeft  = ej.value("soundMoveLeft",  std::string{});
            e.soundDeath     = ej.value("soundDeath",     std::string{});
            e.soundHurt1     = ej.value("soundHurt1",     std::string{});
            e.soundHurt2     = ej.value("soundHurt2",     std::string{});
            e.soundHurt3     = ej.value("soundHurt3",     std::string{});
            e.snapshotInterval = ej.value("snapshotInterval", e.snapshotInterval);
            e.warpTeleportTick = ej.value("warpTeleportTick", e.warpTeleportTick);
            e.runDurationTicks = ej.value("runDurationTicks",  e.runDurationTicks);
            e.deadRespawnTicks = ej.value("deadRespawnTicks",  e.deadRespawnTicks);
            if (ej.contains("lookBoxes") && ej["lookBoxes"].is_array()) {
                for (const auto& lb : ej["lookBoxes"]) {
                    int dir = lb.value("dir", -1);
                    if (dir < 0) continue;
                    GuardLookBox box;
                    box.x1 = lb.value("x1", 0);
                    box.x2 = lb.value("x2", 0);
                    box.y1 = lb.value("y1", 0);
                    box.y2 = lb.value("y2", 0);
                    e.lookBoxes[dir] = box;
                }
            }
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

static void LoadGameObjects(const std::string& dir, std::vector<GameObjectDef>& out) {    json j;
    if (!OpenJson(dir + "/gameobjects.json", j)) return;
    try {
        out.clear();
        for (const auto& gj : j.at("gameObjects")) {
            GameObjectDef g;
            g.id            = gj.value("id",            std::string{});
            g.cooldownTicks = gj.value("cooldownTicks",  0);
            g.health        = gj.value("health",         0);
            g.shield        = gj.value("shield",         0);
            g.shieldMax     = gj.value("shieldMax",      0);
            g.healthMax     = gj.value("healthMax",      0);
            g.healthRegen   = gj.value("healthRegen",    0);
            g.techHealth    = gj.value("techHealth",     0);
            g.techShield    = gj.value("techShield",     0);
            g.refireReadyTick = gj.value("refireReadyTick", g.refireReadyTick);
            g.reloadTick      = gj.value("reloadTick",      g.reloadTick);
            g.innerRange      = gj.value("innerRange",      g.innerRange);
            g.outerRange      = gj.value("outerRange",      g.outerRange);
            g.detectionRange  = gj.value("detectionRange",  g.detectionRange);
            g.soundDeploy     = gj.value("soundDeploy",     std::string{});
            g.soundFire       = gj.value("soundFire",       std::string{});
            g.soundDestroy    = gj.value("soundDestroy",    std::string{});
            g.soundPurchase   = gj.value("soundPurchase",   std::string{});
            g.soundHeal       = gj.value("soundHeal",       std::string{});
            g.soundAmbient    = gj.value("soundAmbient",    std::string{});
            g.soundOpen       = gj.value("soundOpen",       std::string{});
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

static void LoadTerminals(const std::string& dir, std::vector<TerminalDef>& out) {
    json j;
    if (!OpenJson(dir + "/gameobjects.json", j)) return;
    if (!j.contains("terminals")) return;
    try {
        out.clear();
        for (const auto& tj : j.at("terminals")) {
            TerminalDef t;
            t.id              = tj.value("id",              std::string{});
            t.juice           = tj.value("juice",           0);
            t.files           = tj.value("files",           0);
            t.secretInfo      = tj.value("secretInfo",      0);
            t.traceTimeBase      = tj.value("traceTimeBase",      t.traceTimeBase);
            t.traceTimeMedium    = tj.value("traceTimeMedium",    t.traceTimeMedium);
            t.traceTimeExtended  = tj.value("traceTimeExtended",  t.traceTimeExtended);
            t.beaconTimeSecs     = tj.value("beaconTimeSecs",     t.beaconTimeSecs);
            t.soundAmbient       = tj.value("soundAmbient",       std::string{});
            t.soundHack          = tj.value("soundHack",          std::string{});
            t.snapshotInterval   = tj.value("snapshotInterval",   t.snapshotInterval);
            out.push_back(std::move(t));
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "[gas] gameobjects.json terminals error: %s\n", e.what());
        out.clear();
    }
}

// ---------------------------------------------------------------------------

bool GASLoader::Load(const std::string& gasDir) {
    LoadPlayer(gasDir, player);
    LoadAgencies(gasDir, agencies);
    LoadWeapons(gasDir, weapons);
    LoadItems(gasDir, items);
    LoadEnemies(gasDir, enemies);
    LoadAbilities(gasDir, abilities);
    LoadGameObjects(gasDir, gameObjects);
    LoadTerminals(gasDir, terminals);
    loaded = true;
    fprintf(stderr, "[gas] loaded: %zu agencies, %zu weapons, %zu items, %zu enemies, %zu abilities, %zu gameObjects, %zu terminals\n",
            agencies.size(), weapons.size(), items.size(),
            enemies.size(), abilities.size(), gameObjects.size(), terminals.size());
    return true;
}

void GASLoader::Reload(const std::string& gasDir) {
    agencies.clear();
    weapons.clear();
    items.clear();
    enemies.clear();
    abilities.clear();
    gameObjects.clear();
    terminals.clear();
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

const TerminalDef* GASLoader::GetTerminalDef(const std::string& id) const {
    for (const auto& t : terminals)
        if (t.id == id) return &t;
    return nullptr;
}
