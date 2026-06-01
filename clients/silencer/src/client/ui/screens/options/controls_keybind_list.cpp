#include "controls_keybind_list.h"

#include <algorithm>

namespace silencer::client_ui::options {

namespace controls_keybind_list_detail {

constexpr int kPresetRowH = 43;
constexpr int kRowH = 43;
constexpr int kRowGap = 10;
constexpr int kSectionGap = 8;
constexpr int kActionTopGap = 16;
constexpr int kActionRowH = 33;

}  // namespace controls_keybind_list_detail

int KeybindListVisibleRowsForContentHeight(int contentHeight) {
	const int viewportHeight = contentHeight
	                         - controls_keybind_list_detail::kPresetRowH
	                         - controls_keybind_list_detail::kActionTopGap
	                         - controls_keybind_list_detail::kActionRowH
	                         - controls_keybind_list_detail::kSectionGap * 3;
	if(viewportHeight <= 0) return 1;
	// Whole rows that fit at the design row height. The retained rows stretch
	// to absorb leftover viewport space, so flooring keeps the last row from
	// clipping while the list still fills the panel.
	const int rows = (viewportHeight + controls_keybind_list_detail::kRowGap)
	               / (controls_keybind_list_detail::kRowH
	                  + controls_keybind_list_detail::kRowGap);
	return std::max(1, rows);
}

}  // namespace silencer::client_ui::options
