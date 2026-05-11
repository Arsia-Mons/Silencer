#ifndef SILENCER_UI_V2_SCREENS_UPDATE_H
#define SILENCER_UI_V2_SCREENS_UPDATE_H

#include "runtime.h"

#include <functional>
#include <string>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Context;

struct UpdateHandlers {
	std::function<void()> on_update;
	std::function<void()> on_cancel;
	std::function<void()> on_retry;
	std::function<void()> on_download;
};

// Live engine state derived from Updater::GetState() each frame.
struct UpdateState {
	enum class LeftButton { None, Update, Retry, Download };
	LeftButton left = LeftButton::None;
	bool show_cancel = true;
	std::string status_text;
	std::string progress_text;
};

// Emits the update screen's CLAY() tree directly. Caller drives
// Clay_BeginLayout / EndLayout; `handlers` must outlive Clay_EndLayout —
// Clay_OnHover captures pointers into it as callback userData.
void RenderUpdate(const Context & ctx, const UpdateHandlers & handlers,
                  const UpdateState & state);

// Engine-side runtime for GameState::UPDATING. Reads Updater state
// each frame; Tick() handles the STAGING -> UpdaterStage2::Launch
// transition that legacy UpdateScreen::Tick owned.
class UpdateRuntime : public Runtime
{
public:
	UpdateRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;
	void Tick() override;

private:
	World &         world_;
	ScreenContext & sctx_;
};

}  // namespace v2
}  // namespace ui

#endif
