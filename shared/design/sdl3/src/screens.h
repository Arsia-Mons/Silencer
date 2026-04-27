#pragma once

#include "dump_runner.h"
#include "sprite.h"

namespace silencer {

// Per-screen Compose<Name> functions live in screens/<name>.cpp. Each takes
// (fb, sprites, palette, active_sub) and paints the steady-state frame
// using primitives from components/. No SDL types cross this boundary.

void ComposeMainMenu(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub);
void ComposeOptions(Framebuffer &fb, const SpriteSet &sprites,
                    const Palette &palette, int active_sub);
void ComposeOptionsAudio(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub);
void ComposeOptionsDisplay(Framebuffer &fb, const SpriteSet &sprites,
                           const Palette &palette, int active_sub);
void ComposeOptionsControls(Framebuffer &fb, const SpriteSet &sprites,
                            const Palette &palette, int active_sub);
void ComposeLobbyConnect(Framebuffer &fb, const SpriteSet &sprites,
                         const Palette &palette, int active_sub);
void ComposeLobby(Framebuffer &fb, const SpriteSet &sprites,
                  const Palette &palette, int active_sub);
void ComposeLobbyGameCreate(Framebuffer &fb, const SpriteSet &sprites,
                            const Palette &palette, int active_sub);
void ComposeLobbyGameJoin(Framebuffer &fb, const SpriteSet &sprites,
                          const Palette &palette, int active_sub);
void ComposeLobbyGameTech(Framebuffer &fb, const SpriteSet &sprites,
                          const Palette &palette, int active_sub);
void ComposeLobbyGameSummary(Framebuffer &fb, const SpriteSet &sprites,
                             const Palette &palette, int active_sub);
void ComposeUpdating(Framebuffer &fb, const SpriteSet &sprites,
                     const Palette &palette, int active_sub);

}  // namespace silencer
