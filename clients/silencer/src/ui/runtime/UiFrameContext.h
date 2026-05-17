#pragma once

namespace silencer {
namespace ui {

// Owns the per-frame reset of every UI-runtime arena (Text, Button,
// Box, ScrollList, ScrollTextBox, Toggle, TextInput). Production code calls
// BeginFrame() exactly once per Clay layout, before any primitive
// declaration. The individual *BeginFrame() functions remain exported so
// isolated primitive unit tests can keep driving them directly.
class UiFrameContext {
public:
	UiFrameContext();
	~UiFrameContext();

	void BeginFrame(float animationDeltaSeconds = 1.0f / 24.0f,
	                float animationStepSeconds = 1.0f / 24.0f);
};

}  // namespace ui
}  // namespace silencer
