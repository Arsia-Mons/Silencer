#ifndef UI_EDITOR_PREVIEW_SCREEN_H
#define UI_EDITOR_PREVIEW_SCREEN_H

#include "screen.h"
#include "ui_editor_preview_model.h"

namespace silencer::client_ui {

class UiEditorPreviewScreen : public Screen {
public:
	explicit UiEditorPreviewScreen(silencer::ui::UiEditorPreviewDocument document);

	void SetDocument(silencer::ui::UiEditorPreviewDocument document);
	const silencer::ui::UiEditorPreviewDocument& Document() const { return document_; }

	void Build(ScreenContext& ctx) override;
	void Tick(ScreenContext& ctx) override;
	void BuildUi(ScreenContext& ctx,
	             Surface& dst,
	             float frametime,
	             silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext& ctx) override;
	bool HandleUiIntent(ScreenContext& ctx, const silencer::ui::UiAction& action) override;

private:
	silencer::ui::UiEditorPreviewDocument document_;
};

}  // namespace silencer::client_ui

#endif
