#ifndef SILENCER_RENDER_CLAY_UI_TESTS_CLAY_UI_CHECKS_H
#define SILENCER_RENDER_CLAY_UI_TESTS_CLAY_UI_CHECKS_H

#include "shared.h"

class Game;

namespace silencer::clay_bridge {

// Button hover/click parity check. Runs click + hover logic against a
// deterministic pointer-state timeline and reports raw counters plus sizing
// probes. No PNG is produced.
struct ButtonCheckResult {
	int clicksFiredOnPress;     // Expect 1 action on the press edge.
	int clicksFiredWhenHeld;    // Expect 0: held frames do not re-fire.
	int chromeBrightnessHover;  // Expect 136 when hovered.
	int chromeBrightnessIdle;   // Expect 128 when idle.
	int chromeSpriteIndexHover; // Expect 24: Chrome keeps its static face.
	int ovalHoverSpriteIndices[5];
	int ovalHoverBrightness[5];
	int ovalUnhoverSpriteIndices[5];
	int ovalUnhoverBrightness[5];
	int ovalFocusSpriteIndex;
	int ovalFocusBrightness;
	int ovalWallClockPartialSpriteIndex;
	int ovalWallClockPartialBrightness;
	int ovalWallClockNextSpriteIndex;
	int ovalWallClockNextBrightness;
	int compactWidth;
	int compactHeight;
	int chromeAutoWidth;
	int chromeAutoHeight;
	int textCompactWidth;
	int textCompactHeight;
	int textCompactTextXOffset;
	int textCompactTextWidth;
	int textCompactTextYOffset;
	int autoShortWidth;
	int autoLongWidth;
	int autoMultilineHeight;
};
bool RunButtonCheck(::Game & game, ButtonCheckResult & out);

struct ToggleCheckResult {
	int clicksToggle0;
	int clicksToggle1;
	int clicksToggle2;
	int selectedBrightness;
	int unselectedBrightness;
};
bool RunToggleCheck(::Game & game, ToggleCheckResult & out);

struct ScrollListCheckResult {
	int selectActions;
	int lastSelectedIndex;
	int noOverflowScrollbarCount;
	int overflowScrollbarCount;
	int overflowScrollbarBboxX;
	int overflowScrollbarBboxY;
	int overflowScrollbarBboxW;
	int overflowScrollbarBboxH;
};
bool RunScrollListCheck(::Game & game, ScrollListCheckResult & out);

struct ScrollTextBoxCheckResult {
	Uint16 atBottom_prevPos;
	Uint16 notAtBottom_prevPos;
	Uint16 atBottomOverflow_prevPos;
};
bool RunScrollTextBoxCheck(::Game & game, ScrollTextBoxCheckResult & out);

struct TextInputCheckResult {
	int submitActionsForEnter;
	int submitActionsForText;
	int passwordMaskAppliedLen;
	int overflowTailAppliedLen;
	int overflowTailMatches;
};
bool RunTextInputCheck(::Game & game, TextInputCheckResult & out);

}  // namespace silencer::clay_bridge

#endif
