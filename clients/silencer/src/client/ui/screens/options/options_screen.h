#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"
#include "ui_editor_preview_model.h"

#include <string>

class OptionsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	bool goBackClicked = false;
	bool controlsClicked = false;
	bool displayClicked = false;
	bool audioClicked = false;
	bool layoutLoaded_ = false;
	std::string layoutLoadError_;
	silencer::ui::UiEditorPreviewDocument layoutDocument_;
};

#endif
