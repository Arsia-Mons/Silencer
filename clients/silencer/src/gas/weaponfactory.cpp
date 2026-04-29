#include "weaponfactory.h"
#include "gasloader.h"
#include "../actors/objecttypes.h"

int WeaponFactory::ProjectileType(const std::string& weaponId) {
    const WeaponDef* w = GASLoader::Get().GetWeaponDef(weaponId);
    if (!w) return -1;
    const std::string& pt = w->projectileType;
    if (pt == "physics") {
        // "physics" is the generic hit-scan / directional bolt — use weapon ID
        // to distinguish blaster vs laser vs wall (all share projectileType=physics
        // but have different ObjectTypes).
        if (weaponId == "laser") return ObjectTypes::LASERPROJECTILE;
        if (weaponId == "wall")  return ObjectTypes::WALLPROJECTILE;
        return ObjectTypes::BLASTERPROJECTILE;
    }
    if (pt == "rocket")  return ObjectTypes::ROCKETPROJECTILE;
    if (pt == "flamer")  return ObjectTypes::FLAMERPROJECTILE;
    if (pt == "plasma")  return ObjectTypes::PLASMAPROJECTILE;
    if (pt == "arcing")  return ObjectTypes::FLAREPROJECTILE;
    if (pt == "grenade") return ObjectTypes::GRENADE;
    return -1;
}
