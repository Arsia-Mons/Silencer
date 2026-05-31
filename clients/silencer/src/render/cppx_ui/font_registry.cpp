#include "font_registry.h"

namespace silencer::cppx_ui {

namespace {
// Indexed by FaceId. shared/fonts/silencer-*.otf (generated from the legacy
// bitmap banks 133/134/136/132 — exact in-game identity, per SIL-6).
const char *kFaceFile[FontRegistry::FaceCount] = {
    "silencer-ui.otf",       // Body
    "silencer-ui-large.otf", // Large
    "silencer-title.otf",    // Title
    "silencer-tiny.otf",     // Tiny
};
} // namespace

FontRegistry::~FontRegistry() { shutdown(); }

bool FontRegistry::load_faces(const char *font_dir) {
  bool ok = true;
  for (int i = 0; i < FaceCount; ++i) {
    char path[1024];
    SDL_snprintf(path, sizeof(path), "%s/%s", font_dir, kFaceFile[i]);
    // Open at a nominal base; the real size is set per-query/draw via
    // TTF_SetFontSize so one face serves every size.
    faces_[i] = TTF_OpenFont(path, 16.0f);
    if (!faces_[i]) {
      SDL_Log("FontRegistry: TTF_OpenFont(%s) failed: %s", path, SDL_GetError());
      ok = false;
    }
  }
  return ok;
}

void FontRegistry::shutdown() {
  for (int i = 0; i < FaceCount; ++i) {
    if (faces_[i]) {
      TTF_CloseFont(faces_[i]);
      faces_[i] = nullptr;
    }
  }
}

TTF_Font *FontRegistry::face(uint16_t font_id) const {
  uint16_t idx = font_id < FaceCount ? font_id : Body;
  return faces_[idx] ? faces_[idx] : faces_[Body];
}

} // namespace silencer::cppx_ui
