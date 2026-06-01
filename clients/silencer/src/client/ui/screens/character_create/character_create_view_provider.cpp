#include "client/ui/screens/character_create/character_create_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext CharacterCreateContext = {};
const CharacterCreate kEmptyCharacterCreate = {};
}  // namespace

const CharacterCreate& UseCharacterCreate() {
	const auto * value = static_cast<const CharacterCreate *>(
		::use_context(&CharacterCreateContext));
	if(value) return *value;
	::react_report_error("client/ui/character-create: missing CharacterCreateProvider for UseCharacterCreate\n");
	return kEmptyCharacterCreate;
}

::ui::UiElement CharacterCreateScreenView(const CharacterCreateScreenViewProps& props) {
	const CharacterCreate * stored = ::ui::copy_value(
		props.character ? *props.character : kEmptyCharacterCreate);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"CharacterCreateProvider",
		&CharacterCreateContext,
		const_cast<CharacterCreate *>(stored),
		::ui::children({
			::ui::component("CharacterCreateFrame",
			                CharacterCreateFrameProps{ .key = "view" },
			                CharacterCreateFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
