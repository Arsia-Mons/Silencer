#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include "client/ui/retained/RetainedFrame.h"
#include "screen.h"

namespace silencer {
namespace client_ui {
struct Navigation;
class UpdateModel;
}
}

class UpdateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState& input, Uint8 hudPhase, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	void CancelUpdate(const silencer::client_ui::UpdateModel & update,
	                  const silencer::client_ui::Navigation & navigation) const;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
