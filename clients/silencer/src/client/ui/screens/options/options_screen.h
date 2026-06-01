#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "client/ui/retained/RetainedFrame.h"
#include "screen.h"

class OptionsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8 hudPhase, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
