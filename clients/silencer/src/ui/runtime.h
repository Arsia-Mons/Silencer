#ifndef UI_V2_RUNTIME_H
#define UI_V2_RUNTIME_H

class Renderer;
class Surface;

namespace ui { namespace v2 {

// One Runtime per active engine state (MAINMENU, OPTIONS, LOBBY, etc.).
// Owns its own state struct, builds its own Node tree per frame, drives
// its own render + dispatch through the v2 library. Game holds a
// unique_ptr<Runtime> and swaps it on state transition. Modals overlay
// any active runtime and are owned separately (see ModalStack).
class Runtime
{
public:
	virtual ~Runtime() = default;

	// Per-frame: build the Node tree, run Layout, Render into target.
	// dt is wallclock seconds since last call. mouse_{x,y} are logical
	// coords (-1 if no mouse this frame). logical_w/h/scale are the
	// current responsive UI dimensions (Path B); the caller computes
	// them via Renderer::ComputeUIDims and passes them in.
	virtual void Render(Surface & target, Renderer & renderer,
	                    int mouse_x, int mouse_y, float dt,
	                    int logical_w, int logical_h, int scale) = 0;

	// Per-event: return true if the event was consumed (Game stops
	// routing it further). Mouse coords are logical; for keyboard /
	// text input the runtime decides what it cares about.
	virtual bool DispatchMouseDown(int mouse_x, int mouse_y,
	                               int logical_w, int logical_h, int scale) {
		(void)mouse_x; (void)mouse_y;
		(void)logical_w; (void)logical_h; (void)scale;
		return false;
	}
	virtual bool DispatchKeyDown(int sdl_scancode)            { (void)sdl_scancode; return false; }
	virtual bool DispatchTextInput(char ascii)                { (void)ascii; return false; }

	// Per-frame Tick (state-machine / network / animation advancement).
	virtual void Tick() {}
};

}}  // namespace ui::v2

#endif
