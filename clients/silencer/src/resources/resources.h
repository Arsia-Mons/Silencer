#ifndef RESOURCES_H
#define RESOURCES_H

#include "shared.h"
#include "palette.h"
#include "surface.h"
#include "actordef.h"
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Resources
{
public:
	Resources();
	// `game` may be null — when null, the progress callback is suppressed.
	// The demo target (tools/clay-demo/) passes null to avoid pulling in a
	// full Game instance just to load sprites.
	bool Load(class Game * game, bool dedicatedserver = false);
	bool LoadSprites(class Game * game, bool dedicatedserver = false);
	bool LoadTiles(class Game * game, bool dedicatedserver = false);
	bool LoadSounds(class Game * game, bool dedicatedserver = false);
	void UnloadSounds(void);
	std::vector<std::vector<std::shared_ptr<Surface> > > spritebank;
	std::vector<std::vector<std::shared_ptr<Surface> > > tilebank;
	std::vector<std::vector<std::shared_ptr<Surface> > > tileflippedbank;
	std::vector<std::vector<int> > spriteoffsetx;
	std::vector<std::vector<int> > spriteoffsety;
	std::vector<std::vector<unsigned int> > spritewidth;
	std::vector<std::vector<unsigned int> > spriteheight;
	std::map<std::string, Mix_Chunk *> soundbank;
	Mix_Music * menumusic;
	Mix_Music * gamemusic;
	std::unordered_map<std::string, ActorDef> actordefs;

private:
	void MirrorY(Surface * surface);
	void RLESurface(Surface * surface);
	int progress;
	int totalprogressitems;
};

#endif