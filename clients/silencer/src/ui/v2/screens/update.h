#ifndef SILENCER_UI_V2_SCREENS_UPDATE_H
#define SILENCER_UI_V2_SCREENS_UPDATE_H

#include <functional>

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

Node BuildUpdate(const Context & ctx, const UpdateHandlers & handlers = {});

}  // namespace v2
}  // namespace ui

#endif
