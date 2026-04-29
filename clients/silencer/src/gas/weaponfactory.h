#pragma once
#include <string>

// Maps a weapon ID (from weapons.json) to the ObjectTypes constant for its
// projectile, returning -1 if unknown. Callers still create the object via
// World::CreateObject(type) and cast as needed.
//
// Example:
//   int t = WeaponFactory::ProjectileType("laser");  // → ObjectTypes::LASERPROJECTILE
//   LaserProjectile* p = (LaserProjectile*)world.CreateObject(t);
class WeaponFactory {
public:
    static int ProjectileType(const std::string& weaponId);
};
