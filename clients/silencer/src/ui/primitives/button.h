#ifndef SILENCER_UI_PRIMITIVES_BUTTON_H
#define SILENCER_UI_PRIMITIVES_BUTTON_H

#include "clay/clay.h"
#include "primitives/text.h"
#include "shared.h"

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::ui::primitives {

enum class ButtonVariant : Uint8 {
	Oval,
	Chrome,
	LegacyRow,
	Text,
	Ghost,
};

enum class ButtonSize : Uint8 {
	Sm,
	Md,
	Lg,
	Compact,
	Auto,
};

struct ButtonOpts {
	ButtonVariant variant = ButtonVariant::Chrome;
	ButtonSize size = ButtonSize::Md;
	bool disabled = false;
	bool selected = false;
	bool alignLeft = false;
	TextEffect textEffect = TextEffect::Default();
	int minWidth = 0;
	int maxWidth = 0;
	int paddingX = 0;
	int paddingY = 0;
	bool wrapText = false;
};

struct ButtonHandle {
	bool * hoveredOut = nullptr;
	const char * actionId = nullptr;
	UiInteractionRegistry * interactions = nullptr;
};

void ButtonBeginFrame(float animationDeltaSeconds = 1.0f / 24.0f,
                      float animationStepSeconds = 1.0f / 24.0f);

void Button(Clay_String id,
            Clay_String label,
            ButtonOpts opts = ButtonOpts{},
            ButtonHandle handle = ButtonHandle{});

}  // namespace silencer::ui::primitives

#endif
