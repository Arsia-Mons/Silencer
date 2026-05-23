#ifndef UI_EDITOR_PREVIEW_SCREEN_H
#define UI_EDITOR_PREVIEW_SCREEN_H

#include "main_menu/components/silencer_logo.h"
#include "screen.h"
#include "ui_editor_preview_model.h"

#include <string>

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
	std::string versionText_;
	silencer::ui::UiEditorPreviewDocument document_;
	silencer::client_ui::main_menu::SilencerLogo mainMenuLogo_;
};

}  // namespace silencer::client_ui

#endif
