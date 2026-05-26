#include "primitives/focusable.h"

namespace silencer::ui::primitives {

void Focusable(const FocusableProps& props, FocusableRender render) {
	silencer::ui::UiFocusableState state = silencer::ui::ui_focusable({
		props.id,
		props.disabled,
		props.nav,
		props.onConfirm,
		props.onFocus,
	});
	render(state);
}

}  // namespace silencer::ui::primitives
