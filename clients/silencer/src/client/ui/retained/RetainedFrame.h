#pragma once

#include "runtime/UiInteractionRegistry.h"
#include "ui/runtime/draw_command.h"
#include "ui/runtime/element.h"
#include "ui/runtime/flex_layout.h"
#include "ui/runtime/tree.h"

#include <functional>

namespace silencer {
namespace client_ui {

class RetainedFrame {
public:
	using BuildRoot = std::function<::ui::UiElement()>;

	bool Build(BuildRoot buildRoot,
	           int width,
	           int height,
	           silencer::ui::UiInteractionRegistry& interactions);

	const ::ui::DrawCommandList& Commands() const { return commands_; }

private:
	void RegisterAutomation(::ui::NodeId id,
	                        silencer::ui::UiInteractionRegistry& interactions) const;

	::ui::UiElementFrame elementFrame_;
	::ui::UiTree tree_;
	::ui::FlexLayoutAdapter layout_;
	::ui::DrawCommandList commands_;
};

}  // namespace client_ui
}  // namespace silencer
