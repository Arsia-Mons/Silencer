#ifndef SILENCER_UI_V2_SCREENS_UPDATE_H
#define SILENCER_UI_V2_SCREENS_UPDATE_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>
#include <string>

class World;
class ScreenContext;

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct UpdateHandlers {
	std::function<void()> on_update;
	std::function<void()> on_cancel;
	std::function<void()> on_retry;
	std::function<void()> on_download;
};

// Live engine state derived from Updater::GetState() each frame. When
// `state == nullptr` BuildUpdate emits the post-Build pre-Tick layout
// (all four buttons stacked + empty status/progress overlays) — that is
// the byte-identical preview gate target. When non-null, only the
// visible button(s) + status/progress labels render.
struct UpdateState {
	enum class LeftButton { None, Update, Retry, Download };
	LeftButton left = LeftButton::None;
	bool show_cancel = true;
	std::string status_text;
	std::string progress_text;
};

Node BuildUpdate(const Context & ctx, const UpdateHandlers & handlers = {},
                 const UpdateState * state = nullptr);

// Engine-side runtime for GameState::UPDATING. Reads Updater state
// each frame; Tick() handles the STAGING -> UpdaterStage2::Launch
// transition that legacy UpdateScreen::Tick owned.
class UpdateRuntime : public Runtime
{
public:
	UpdateRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y) override;
	void Tick() override;

private:
	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;
};

}  // namespace v2
}  // namespace ui

#endif
