#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "shared.h"
#include "object.h"
#include "sprite.h"
#include <string>

class Overlay : public Object
{
public:
	Overlay();
	void Tick(World & world);
	bool MouseInside(World & world, Uint16 mousex, Uint16 mousey);
	Uint8 state_i;
	std::string text;
	Uint8 textbank;
	Uint8 textwidth;
	bool textcolorramp;
	bool textallownewline;
	int textlineheight;
	bool drawalpha;
	Uint8 uid;
	bool clicked;
	std::vector<Uint8> customsprite;
	int customspritew;
	int customspriteh;
	// Bank-222 environment lights loaded from the map
	bool mapLight;
	Uint8 lightColorR, lightColorG, lightColorB;
	Uint8 lightAnim;       // 0=static, 1=flicker, 2=pulse
	Uint8 lightPulseSpeed; // 0=slow(128t), 1=med(64t), 2=fast(32t)
};

#endif