#ifndef PLATFORM_H
#define PLATFORM_H

#include "shared.h"
#include <memory>
#include "platformset.h"

enum class PhysicsMaterial : uint8_t {
    // Modern & Urban
    Concrete        =  0,
    Asphalt         =  1,
    MetalSolid      =  2,
    MetalGrate      =  3,
    Glass           =  4,
    Tile            =  5,
    Carpet          =  6,
    Linoleum        =  7,
    // Natural & Outdoor
    GrassDry        =  8,
    GrassLush       =  9,
    Dirt            = 10,
    Mud             = 11,
    Sand            = 12,
    Gravel          = 13,
    Rock            = 14,
    // Water & Weather
    WaterShallow    = 15,
    WaterDeep       = 16,
    SnowPowder      = 17,
    SnowCrust       = 18,
    Ice             = 19,
    Puddle          = 20,
    // Historic & Indoor Traditional
    WoodSolid       = 21,
    WoodCreaky      = 22,
    Marble          = 23,
    Brick           = 24,
    // Sci-Fi & Fantasy
    FleshOrganic    = 25,
    EnergyForcefield = 26,
    MagmaAsh        = 27,
};

class Platform
{
public:
    Platform(Uint8 type, Uint16 id, int x1, int y1, int x2, int y2);
	void GetTopSegment(int & x1, int & y1, int & x2, int & y2);
	int XtoY(int x);
	void GetNormal(int x, int y, float * xn, float * yn);
	int GetLength(void);

	static const Uint8 RECTANGLE = 1 << 0;
	static const Uint8 STAIRSUP = 1 << 1;
	static const Uint8 STAIRSDOWN = 1 << 2;
	static const Uint8 LADDER = 1 << 3;
	static const Uint8 TRACK = 1 << 4;
	static const Uint8 OUTSIDEROOM = 1 << 5;
	static const Uint8 SPECIFICROOM = 1 << 6;
    Uint8 type;
    int x1, x2, y1, y2;
	Uint16 id;
	PhysicsMaterial physicsMaterial = PhysicsMaterial::Concrete;
	Platform * adjacentl;
	Platform * adjacentr;
	class PlatformSet * set;
};

#endif