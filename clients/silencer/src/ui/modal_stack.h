#ifndef UI_V2_MODAL_STACK_H
#define UI_V2_MODAL_STACK_H

#include "ui_state.h"

#include <functional>
#include <string>
#include <vector>

class Renderer;
class Surface;
class World;

namespace ui { namespace v2 {

// Modal stack — overlays the active Runtime's render and intercepts
// mouse / text / key input when non-empty. Lobby panels (P16g) and
// other callers push entries here for confirmation / progress /
// password prompts without owning a legacy Screen subclass. The v2
// tree is rebuilt each frame from the current text / password buffer.
class ModalStack
{
public:
	enum Kind { MESSAGE, PASSWORD };

	struct Entry {
		Kind                                       kind          = MESSAGE;
		std::string                                text;           // MESSAGE: body / PASSWORD: prompt
		bool                                       has_ok        = true;
		std::string                                password_buf;   // PASSWORD only
		std::function<void()>                      on_close;       // MESSAGE on-OK callback
		std::function<void(const std::string &)>   on_submit;      // PASSWORD on-OK callback
	};

	ModalStack(World & world, int & mouse_x, int & mouse_y)
		: world_(world), mouse_x_(mouse_x), mouse_y_(mouse_y) {}

	void ShowMessage(const std::string & text, std::function<void()> on_close = {});
	void ShowProgress(const std::string & text);  // No OK button — caller pops.
	void ShowPassword(std::function<void(const std::string &)> on_submit);
	void SetProgressText(const std::string & text);
	void Pop();

	bool Active() const          { return !stack_.empty(); }
	bool ProgressActive() const;

	// Overlay render: blits the top modal on top of whatever the per-
	// state render already drew. No-op when stack is empty.
	void RenderOverlay(Surface & target, ::Renderer & renderer,
	                   int logical_w, int logical_h, int scale);

	// Event dispatch: return true when the modal absorbed the event so
	// the underlying state's path is skipped.
	bool DispatchClick(int logical_x, int logical_y,
	                   int logical_w, int logical_h, int scale);
	bool DispatchKey(int sdl_scancode);
	bool DispatchText(char ascii);

private:
	World & world_;
	int &   mouse_x_;
	int &   mouse_y_;
	UIState state_;
	std::vector<Entry> stack_;
};

}}  // namespace ui::v2

#endif
