#include "clipboard.h"

namespace ui {
namespace {

ClipboardWriteFn g_write = nullptr;
ClipboardReadFn g_read = nullptr;

} // namespace

void set_clipboard_handlers(ClipboardWriteFn write, ClipboardReadFn read) {
  g_write = write;
  g_read = read;
}

void clipboard_write(const char *utf8) {
  if (g_write && utf8)
    g_write(utf8);
}

bool clipboard_read(std::string &out) {
  if (!g_read)
    return false;
  return g_read(out);
}

} // namespace ui
