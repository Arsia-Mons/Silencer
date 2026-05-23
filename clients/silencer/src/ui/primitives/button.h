#ifndef SILENCER_UI_PRIMITIVES_BUTTON_H
#define SILENCER_UI_PRIMITIVES_BUTTON_H

#include "clay/clay.h"
#include "primitives/text.h"
#include "shared.h"
#include <memory>

namespace silencer::ui {
class UiInteractionRegistry;
}

namespace silencer::ui::primitives {

enum class ButtonVariant : Uint8 {
	Oval,
	Chrome,
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
	int widthOverride = 0;
	int paddingX = 0;
	int paddingY = 0;
	bool paddingOverride = false;
	bool wrapText = false;
};

struct ButtonHandle {
	bool * hoveredOut = nullptr;
	const char * actionId = nullptr;
	UiInteractionRegistry * interactions = nullptr;
};

void ButtonBeginFrame(float animationDeltaSeconds = 1.0f / 24.0f,
                      float animationStepSeconds = 1.0f / 24.0f);

class ButtonVisualStateGuard {
public:
	ButtonVisualStateGuard();
	~ButtonVisualStateGuard();

	ButtonVisualStateGuard(const ButtonVisualStateGuard&) = delete;
	ButtonVisualStateGuard& operator=(const ButtonVisualStateGuard&) = delete;

private:
	struct Snapshot;
	std::unique_ptr<Snapshot> snapshot_;
};

void Button(Clay_String id,
            Clay_String label,
            ButtonOpts opts = ButtonOpts{},
            ButtonHandle handle = ButtonHandle{});

}  // namespace silencer::ui::primitives

#endif
