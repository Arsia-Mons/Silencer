#ifndef UI_EDITOR_PREVIEW_MODEL_H
#define UI_EDITOR_PREVIEW_MODEL_H

#include <string>
#include <vector>

namespace silencer::ui {

struct UiEditorSize {
	enum class Mode {
		Fit,
		Grow,
		Fixed,
	};

	Mode mode = Mode::Fit;
	float value = 0.0f;
};

struct UiEditorStyle {
	UiEditorSize width;
	UiEditorSize height;
	std::string direction;
	std::string align;
	std::string justify;
	int padding = 0;
	int gap = 0;
	int radius = 0;
	int backgroundPalette = -1;
	int borderPalette = -1;
	int textPalette = 0;
	std::string font;
};

struct UiEditorNode {
	std::string id;
	std::string kind;
	std::string name;
	std::string text;
	std::string placeholder;
	std::string action;
	UiEditorStyle style;
	std::vector<UiEditorNode> children;
};

struct UiEditorPreviewDocument {
	std::string surface;
	int viewportWidth = 640;
	int viewportHeight = 480;
	UiEditorNode root;
};

}  // namespace silencer::ui

#endif
