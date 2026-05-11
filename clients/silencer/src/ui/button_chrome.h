#ifndef SILENCER_UI_V2_BUTTON_CHROME_H
#define SILENCER_UI_V2_BUTTON_CHROME_H

#include "shared.h"
#include "node.h"

namespace ui {
namespace v2 {

// Per-ButtonType chrome facts — sprite slot, dimensions, text font / placement.
// Mirrors clients/silencer/src/ui/components/button.cpp (`Button::SetType` +
// `Button::GetTextOffset`). Kept here as data so both the render path and the
// hit-test / dispatch path read from one source of truth.
struct ButtonChrome {
	Uint8 bank;          // 0xFF = no chrome (B52x21)
	Uint8 base_index;    // INACTIVE chrome frame; ACTIVE = base_index + 4
	int   width;
	int   height;
	Uint8 text_bank;
	Uint8 text_width;
	int   text_yoff;
	int   text_xoff_extra;
};

inline ButtonChrome ChromeFor(ButtonType type) {
	switch(type){
		case ButtonType::B112x33: return {6,    28, 112, 33, 135, 11, 8, 0};
		case ButtonType::B220x33: return {6,    23, 220, 33, 135, 11, 8, 0};
		case ButtonType::B196x33: return {6,     7, 196, 33, 135, 11, 8, 0};
		case ButtonType::B236x27: return {6,     2, 236, 27, 135, 11, 8, 0};
		case ButtonType::B52x21:  return {0xFF,  0,  52, 21, 133,  7, 8, 1};
		case ButtonType::B156x21: return {7,    24, 156, 21, 134,  8, 4, 0};
	}
	return {6, 7, 196, 33, 135, 11, 8, 0};
}

}  // namespace v2
}  // namespace ui

#endif
