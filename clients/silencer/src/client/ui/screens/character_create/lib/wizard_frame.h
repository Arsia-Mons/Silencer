#pragma once

// Pure layout/style helpers for the CharacterCreate wizard's shared two-pane
// frame, extracted from character_create.cppx. These build LayoutStyle/
// StylePatch values (not component bodies), so they live in a plain header.

#include "client/ui/components/tokens.h"
#include "client/ui/hooks/use_chrome.h" // ChromeTextures
#include "client/ui/screens/character_create_data.h" // kAgencyCount
#include "ui/runtime/tree.h"
#include "ui/style/style_patch.h"

namespace client::ui {

inline int clamp_agency_index(int index) {
  return index >= 0 && index < kAgencyCount ? index : 0;
}

// The shared wizard chrome: every step sits in the same two-pane frame
// (chrome_panel sprite, bank 7 idx 5) over the starfield; LEFT pane is the
// 354-wide list column, RIGHT pane is step-specific (or empty — the Mars
// backdrop reads through). The sprite bakes the double borders + divider.
inline ::ui::LayoutStyle wizard_frame_layout() {
  return {.direction = ::ui::FlexDirection::Row,
          .align_items = ::ui::AlignItems::Stretch,
          .width = ::ui::Length::points(942.0f),
          .height = ::ui::Length::points(661.5f),
          .margin = {0.0f, 4.0f, 0.0f, 0.0f},
          .padding = {82.5f, 33.0f, 27.0f, 51.0f},
          .gap = 117.0f};
}

inline ::ui::StylePatch wizard_frame_style(const ChromeTextures &chrome) {
  return chrome.chrome_panel
             ? silencer::tokens::image_patch(chrome.chrome_panel)
             : silencer::tokens::panel_patch(silencer::tokens::kSurfaceHeroPanel,
                                             silencer::tokens::kBorderHeroPanel);
}

inline ::ui::LayoutStyle wizard_left_pane_layout() {
  return {.direction = ::ui::FlexDirection::Column,
          .align_items = ::ui::AlignItems::Stretch,
          .width = ::ui::Length::points(354.0f),
          .gap = 7.5f};
}

inline ::ui::LayoutStyle wizard_right_spacer_layout() {
  return {.min_width = ::ui::Length::points(0.0f),
          .flex_basis = ::ui::Length::points(0.0f),
          .flex_grow = ::ui::StyleFloat::points(1.0f)};
}

} // namespace client::ui
