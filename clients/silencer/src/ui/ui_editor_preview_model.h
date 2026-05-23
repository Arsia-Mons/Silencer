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
	float min = 0.0f;
	float max = 0.0f;
	bool hasMin = false;
	bool hasMax = false;
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

struct UiEditorImage {
	bool enabled = false;
	int bank = 0;
	int index = 0;
	std::string mode;
};

struct UiEditorFloating {
	bool enabled = false;
	float offsetX = 0.0f;
	float offsetY = 0.0f;
	int zIndex = 0;
	std::string attachTo;
	std::string elementAttach;
	std::string parentAttach;
	bool pointerPassthrough = false;
};

struct UiEditorNode {
	std::string id;
	std::string kind;
	std::string name;
	std::string text;
	std::string action;
	std::string textBinding;
	std::string component;
	std::string buttonVariant;
	std::string buttonSize;
	UiEditorImage image;
	UiEditorFloating floating;
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
