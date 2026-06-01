#pragma once

#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One purchasable row of the active buy/tech station (doc §6). `label` is the
// item name, `detail` the price/tech-slot summary, `affordable` whether the
// viewed agent can currently afford it.
struct TechItem {
  std::string label = {};
  std::string detail = {};
  bool affordable = false;
};

// The buy/tech station model (doc §6): the rows of the station the viewed agent
// has open (mutually exclusive buy XOR tech) plus the purchase/close intents
// over the public Player seam (`Player::BuyItem/RepairItem/VirusItem`). Both
// flags false when no station is open. `purchase(index)` buys the row; `close()`
// dismisses the station. The selection cursor lives in [[use_buy_tech]].
struct Tech {
  bool buy_active = false;
  bool tech_active = false;
  std::vector<TechItem> items = {};

  std::function<void(int)> purchase = {};
  std::function<void()> close = {};
};

Tech use_tech();

} // namespace client::ui
