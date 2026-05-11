#ifndef SILENCER_UI_V2_SCREENS_UPDATE_H
#define SILENCER_UI_V2_SCREENS_UPDATE_H

#include <functional>
#include <string>

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

}  // namespace v2
}  // namespace ui

#endif
