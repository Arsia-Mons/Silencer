#ifndef WORLD_OBJECT_REGISTRY_H
#define WORLD_OBJECT_REGISTRY_H

#include "objecttypes.h"
#include "shared.h"
#include <list>
#include <map>
#include <vector>

class World;
class Object;

class WorldObjectRegistry
{
	friend class World;

public:
	static const unsigned int maxobjects = 32000;

	explicit WorldObjectRegistry(World & world);

	Object * CreateObject(Uint8 type, Uint16 id = 0);
	Object * GetObjectFromId(Uint16 id);
	void MarkDestroyObject(Uint16 id);
	void DestroyMarkedObjects();
	void DestroyObject(Uint16 id);
	void DestroyAllObjects();
	const std::vector<Uint16> & GetObjectsByType(Uint8 type) const { return objectsbytype[type]; }
	bool TestAABB(int x1, int y1, int x2, int y2, Object * object, std::vector<Uint8> & types, bool onlycollidable = true);
	std::vector<Object *> TestAABB(int x1, int y1, int x2, int y2, std::vector<Uint8> & types, Uint16 except = 0, Uint16 teamid = 0, bool onlycollidable = true);
	Object * TestIncr(int x1, int y1, int x2, int y2, int * xv, int * yv, std::vector<Uint8> & types, Uint16 except = 0, Uint16 teamid = 0);

private:
	World & world;
	std::list<Object *> objectlist;
	std::list<Object *> tobjectlist;
	std::map<Uint16, Object *> objectidlookup;
	std::list<Uint16> objectdestroylist;
	std::vector<Uint16> objectsbytype[ObjectTypes::MAX_OBJECT_TYPE];
	unsigned int currentid;
	Uint8 illuminate;
};

#endif
