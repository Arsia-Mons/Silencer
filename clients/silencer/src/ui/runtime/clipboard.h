#pragma once

// The SDL-free clipboard seam. The renderer bridge installs the OS-backed
// handlers once at startup (SDL_SetClipboardText/SDL_GetClipboardText); ui/
// never touches SDL. No handlers installed (hermetic tests) => writes drop,
// reads report empty.

#include <string>

namespace ui {

using ClipboardWriteFn = void (*)(const char *utf8);
using ClipboardReadFn = bool (*)(std::string &out);

void set_clipboard_handlers(ClipboardWriteFn write, ClipboardReadFn read);
void clipboard_write(const char *utf8);
bool clipboard_read(std::string &out);

} // namespace ui
