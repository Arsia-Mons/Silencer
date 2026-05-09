#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include "shared.h"
#include "object.h"
#include "sprite.h"
#include "interface.h"

class ScrollBar : public Object
{
public:
	ScrollBar();
	bool MouseInside(World & world, Uint16 mousex, Uint16 mousey);
	bool MouseInsideUp(World & world, Uint16 mousex, Uint16 mousey);
	bool MouseInsideDown(World & world, Uint16 mousex, Uint16 mousey);
	// Returns true if the mouse is within the configured wheel hover region.
	// When the region is unset (w==0 or h==0), defers to MouseInside on the
	// scrollbar sprite itself so legacy single-scrollbar Interfaces are
	// unchanged.
	bool MouseInsideWheelRegion(World & world, Uint16 mousex, Uint16 mousey);
	void ScrollUp(void);
	void ScrollDown(void);
	bool draw;
	Uint8 barres_index;
	Uint16 scrollposition;
	Uint16 scrollmax;
	Uint8 scrollpixels;
	// Wheel-hover region: when an Interface owns multiple scrollbars, set
	// these so wheel events route to whichever scrollbar's region the mouse
	// is over. Default (0,0,0,0) = unbounded (legacy behavior).
	Sint16 scrollregionx;
	Sint16 scrollregiony;
	Uint16 scrollregionw;
	Uint16 scrollregionh;
};

#endif