#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "components/silencer_logo.h"
#include "screen.h"
#include "ui_editor_preview_model.h"

#include <string>

class MainMenuScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	bool tutorialClicked = false;
	bool lobbyClicked = false;
	bool optionsClicked = false;
	bool exitClicked = false;
	bool layoutLoaded_ = false;
	std::string versionText_;
	std::string layoutLoadError_;
	silencer::ui::UiEditorPreviewDocument layoutDocument_;
	silencer::client_ui::main_menu::SilencerLogo logo;
};

#endif
