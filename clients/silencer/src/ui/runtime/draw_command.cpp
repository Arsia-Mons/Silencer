#include "draw_command.h"

namespace ui {

bool DrawCommandList::push(const DrawCommand &command) {
  if (count >= UI_MAX_DRAW_COMMANDS - UI_DRAW_COMMANDS_RESERVE) {
    ++dropped_count;
    return false;
  }
  commands[count++] = command;
  return true;
}

bool DrawCommandList::push_close(const DrawCommand &command) {
  if (count >= UI_MAX_DRAW_COMMANDS) {
    ++dropped_count;
    return false;
  }
  commands[count++] = command;
  return true;
}

bool DrawCommandList::push_text(const char *bytes, uint16_t len,
                                uint32_t *out_off) {
  if (text_len_used + static_cast<int>(len) > UI_DRAW_TEXT_ARENA_BYTES) {
    ++dropped_count;
    return false;
  }
  if (out_off)
    *out_off = static_cast<uint32_t>(text_len_used);
  for (uint16_t i = 0; i < len; ++i)
    text_arena[text_len_used++] = bytes[i];
  return true;
}

bool DrawCommandList::push_stops(const GradientStop *stops, uint8_t n,
                                 uint16_t *out_off) {
  if (grad_count + static_cast<int>(n) > UI_DRAW_GRAD_STOPS) {
    ++dropped_count;
    return false;
  }
  if (out_off)
    *out_off = static_cast<uint16_t>(grad_count);
  for (uint8_t i = 0; i < n; ++i)
    grad_arena[grad_count++] = stops[i];
  return true;
}

void DrawCommandList::reset() {
  count = 0;
  text_len_used = 0;
  grad_count = 0;
  dropped_count = 0;
}

} // namespace ui
