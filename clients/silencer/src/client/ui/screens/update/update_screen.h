#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include "screen.h"

#include <functional>

class UpdateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	std::function<void()> consentUpdate;
	std::function<void()> cancelUpdate;
	std::function<void()> retryUpdate;
	std::function<void()> openDownload;
};

#endif
