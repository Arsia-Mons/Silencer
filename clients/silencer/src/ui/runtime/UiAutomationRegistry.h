#pragma once

#include <string>
#include <vector>

namespace silencer {
namespace ui {

enum class UiElementKind {
	Container,
	Button,
	Text,
	TextField,
	ListItem,
	Tab,
	Slider,
	Progress,
};

struct UiRect {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

struct UiElementMetadata {
	std::string id;
	UiElementKind kind = UiElementKind::Container;
	std::string label;
	std::string value;
	UiRect bounds;
	bool enabled = true;
	bool focused = false;
	bool selected = false;
};

class UiAutomationRegistry {
public:
	void BeginFrame();
	void Register(UiElementMetadata metadata);
	const std::vector<UiElementMetadata>& Elements() const;
	const UiElementMetadata* FindById(const std::string& id) const;
	const UiElementMetadata* FindByLabel(const std::string& label) const;

private:
	std::vector<UiElementMetadata> elements_;
};

}  // namespace ui
}  // namespace silencer
