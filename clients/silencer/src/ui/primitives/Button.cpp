#include "primitives/Button.h"

namespace silencer {
namespace ui {

void RegisterButton(const ButtonProps& props, UiAutomationRegistry& automation) {
	UiElementMetadata metadata;
	metadata.id = props.id;
	metadata.kind = UiElementKind::Button;
	metadata.label = props.label;
	metadata.bounds = props.bounds;
	metadata.enabled = props.enabled;
	automation.Register(metadata);
}

void QueueButtonActivation(const ButtonProps& props, UiActionQueue& actions) {
	if(!props.enabled) return;
	actions.Push(UiAction{ UiActionKind::Activate, props.id, props.label });
}

}  // namespace ui
}  // namespace silencer
