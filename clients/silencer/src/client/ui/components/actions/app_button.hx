#pragma once

#include "ui/components/common.h"

#include <functional>

namespace silencer {
namespace client_ui {

enum class AppButtonVariant {
	Primary,
	Secondary,
	Ghost,
};

enum class AppButtonSize {
	Md,
	Sm,
	MainMenu,
	Chrome,
	FitContent,
};

struct AppButtonProps {
	const char * key = nullptr;
	const char * control_id = nullptr;
	AppButtonVariant variant = AppButtonVariant::Secondary;
	AppButtonSize size = AppButtonSize::Md;
	bool disabled = false;
	bool default_focused = false;
	const char * label = nullptr;
	std::function<void()> on_press = {};
	const char * accessibility_label = nullptr;
	::ui::UiChildren children = {};
};

::ui::UiElement AppButton(const AppButtonProps& props);

}  // namespace client_ui
}  // namespace silencer
