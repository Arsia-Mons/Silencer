#ifndef SILENCER_UI_CLAY_INSPECTOR_H
#define SILENCER_UI_CLAY_INSPECTOR_H

// Screen-side registry of interactive Clay widgets that the CLI control
// dispatch (inspect / click / set_text) can address. Each Clay panel calls
// Register() inside its Build*Tree per frame for buttons / toggles / text
// inputs / list rows it emits. The registry is per-frame: BeginFrame() at
// the start of every Clay Build pass clears it.
//
// Pointer-stable label / textBuffer lifetime is the responsibility of the
// caller: panel state structs and string literals satisfy this naturally
// (next frame's Build refreshes them in place, with the same pointers).

#include "shared.h"
#include <vector>

namespace silencer::ui::clay_inspector {

enum class WidgetKind : Uint8 {
	Button,
	Toggle,
	TextInput,
	ListRow,
};

struct Widget {
	const char * label = nullptr;  // pointer-stable for at least this frame.
	WidgetKind kind   = WidgetKind::Button;
	int uid = -1;                  // optional legacy automation id.
	int x = 0, y = 0, w = 0, h = 0;

	// Button / Toggle invocation.
	void (*onClick)(void * user) = nullptr;
	void * clickUser = nullptr;

	// ListRow invocation — callback receives the row index.
	void (*onClickRow)(void * user, int index) = nullptr;
	int rowIndex = -1;

	// Toggle visual state (read-only at inspect time).
	bool selected = false;

	// TextInput buffer — writable in place.
	char * textBuffer = nullptr;
	int textBufferLen = 0;   // capacity including null terminator.
	bool isPassword = false;
	bool inactive = false;
	bool numbersOnly = false;
	void (*onEnter)(void * user) = nullptr;
	void * enterUser = nullptr;
};

// Called by Clay screens at the top of every Build pass (immediately after
// the primitives' BeginFrame()s) to discard last frame's registrations.
void BeginFrame();

// Append a widget. Caller owns label + textBuffer pointer lifetimes.
void Register(const Widget & w);

// Read-only view used by the CLI control dispatch.
const std::vector<Widget> & All();

// Case-insensitive label lookup. Returns nullptr if no match (or ambiguous —
// callers that need to distinguish must use All() and count themselves).
const Widget * FindByLabel(const char * label);
const Widget * FindByUid(int uid);

// Shared focus + text mutation for Clay text inputs. Uses the per-frame
// registry as the canonical list of focusable text fields; screens own the
// buffers and refresh them when registering widgets.
bool FocusTextInputAt(int x, int y);
bool FocusTextInputByUid(int uid);
bool IsTextInputFocused(int uid);
void ClearFocus();
bool DispatchTextInput(char ascii);
bool DispatchKeyPress(char ascii);

}  // namespace silencer::ui::clay_inspector

#endif
