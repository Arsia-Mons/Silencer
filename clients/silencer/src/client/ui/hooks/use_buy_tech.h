#pragma once

#include <functional>

namespace client::ui {

// The buy/tech selection cursor (doc §6, golden `use_loadout` idiom). The cursor
// is the replicated `Player::buyifacelastitem`/`techifacelastitem` field surfaced
// read-only as `selected_index`; `select(index)` moves it (queued). The overlay
// rows fire `select` from their focus callback so keyboard nav drives the cursor.
// Item rows + the purchase/close actions live in [[use_tech]].
struct BuyTech {
  int selected_index = 0;
  std::function<void(int)> select = {};
};

BuyTech use_buy_tech();

} // namespace client::ui
