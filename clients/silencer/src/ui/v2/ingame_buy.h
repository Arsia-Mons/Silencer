#ifndef UI_V2_INGAME_BUY_H
#define UI_V2_INGAME_BUY_H

class World;
class Player;

namespace ui { namespace v2 {

// In-game buy menu overlay. Replaces the legacy spawn-an-Interface-with-
// a-SelectBox-Object path that Player::Tick used to drive when isbuying
// transitioned to true. Cursor state lives on Player (buyifacelastitem /
// buyifacelastscrolled — already on the actor, repurposed from "last
// position between opens" to live cursor); active state is Player::isbuying
// (already replicated). This class owns the editing behavior + audio +
// SendBuy handoff. Sibling to IngameChat / ModalStack: not a per-state
// Runtime, just a thin overlay class held by Game.
//
// Lifecycle:
// - Player::Tick edge-detects isbuying false->true and calls Activate() on
//   the local player's overlay (plays menu-select audio).
// - DispatchKey called from events.cpp KEY_DOWN when Active() (UP / DOWN
//   for selecteditem nav with auto-scroll, RETURN to buy, swallows other
//   keys so movement doesn't leak through). LEFT / RIGHT do nothing
//   (mirrors legacy Interface::Left/RightPressed branch for SELECTBOX).
class IngameBuy
{
public:
	explicit IngameBuy(World & world);

	void Activate();              // edge-triggered from Player::Tick on isbuying↑
	bool Active() const;          // == local player exists && player->isbuying

	// Returns true if the event was consumed. Handles UP/DOWN (nav),
	// RETURN (Submit), and swallows everything else while active.
	bool DispatchKey(int sdl_scancode);

private:
	void MoveSelection(int delta);
	void Submit();

	World & world_;
};

}}  // namespace ui::v2

#endif
