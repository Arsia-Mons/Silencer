#ifndef MODAL_H
#define MODAL_H

#include "screen.h"

// Overlay pushed on top of whatever's active. Blocks input below. The screen
// stack draws the Screen underneath a Modal first, so the Modal renders on
// top instead of replacing the view. Each Modal subclass invokes its onClose
// callback before requesting pop.
class Modal : public Screen
{
public:
	bool IsOverlay() const override { return true; }
};

#endif
