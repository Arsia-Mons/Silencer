#ifndef SURFACE_H
#define SURFACE_H

#include "shared.h"
#include <vector>

class Surface
{
public:
	Surface();
	Surface(int w, int h, Uint8 clearcolor = 0);
	void Clear(Uint8 color);
	Uint8 * GetPixels(void);
	int w;
	int h;
	std::vector<Uint8> pixels;
	std::vector<Uint8> rlepixels;

	// Scissor stack — used by the ui/v2 Scroll container to clip blits to a
	// sub-rect. Empty stack ⇒ no clipping (the default everywhere else, so
	// byte-identity of every migrated screen is preserved).
	// `BlitSurface` clips its `dstrect` against the stack top before drawing.
	// Pushed rects are intersected with the previous top so nested clips
	// only shrink, never grow.
	struct ScissorRect { int x1, y1, x2, y2; };
	std::vector<ScissorRect> scissorstack;
	void PushScissor(int x, int y, int w, int h);
	void PopScissor(void);
	// Returns true if the stack is non-empty and fills `out` with the
	// intersection rect; false means "no clipping active".
	bool ActiveScissor(ScissorRect & out) const;
};

#endif