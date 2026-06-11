#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One visible row of the active buy/tech station, projected per origin's
// BuyTechOverlayView (HudView.cpp): display name, price column text
// ("N"/"UP"/"DOWN"), the replicated selection + its wall-clock pulse
// brightness, and the frame-resolved icon texture (brightness applied).
struct TechItem {
  int index = 0; // absolute item index (scroll window of 5 shows a slice)
  std::string label = {};
  std::string price = {};
  bool selected = false;
  uint8_t brightness = 128;
  uint32_t icon = 0;
  uint16_t icon_w = 0, icon_h = 0;
  int16_t icon_ox = 0, icon_oy = 0;
};

// The buy/tech station model (doc §6): the visible rows of the station the
// viewed agent has open (mutually exclusive buy XOR tech), the footer line
// ("Available Credits: N" / "Viruses Available: N"), plus the purchase/close
// intents over the public Player seam. Both flags false when no station is
// open. The selection cursor lives in [[use_buy_tech]].
struct Tech {
  bool buy_active = false;
  bool tech_active = false;
  std::vector<TechItem> items = {};
  std::string footer = {};

  std::function<void(int)> purchase = {};
  std::function<void()> close = {};
};

Tech use_tech();

} // namespace client::ui
