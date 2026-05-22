#ifndef UI_EDITOR_PREVIEW_SCREEN_H
#define UI_EDITOR_PREVIEW_SCREEN_H

#include "screen.h"

#include <string>
#include <vector>

namespace silencer::client_ui {

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

class UiEditorPreviewScreen : public Screen {
public:
	explicit UiEditorPreviewScreen(UiEditorPreviewDocument document);

	void SetDocument(UiEditorPreviewDocument document);
	const UiEditorPreviewDocument& Document() const { return document_; }

	void Build(ScreenContext& ctx) override;
	void Tick(ScreenContext& ctx) override;
	void BuildUi(ScreenContext& ctx,
	             Surface& dst,
	             float frametime,
	             silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext& ctx) override;
	bool HandleUiIntent(ScreenContext& ctx, const silencer::ui::UiAction& action) override;

private:
	void BuildNode(const UiEditorNode& node,
	               silencer::ui::UiInteractionRegistry& interactions);

	UiEditorPreviewDocument document_;
};

}  // namespace silencer::client_ui

#endif
