#ifndef UI_V2_INGAME_TECH_H
#define UI_V2_INGAME_TECH_H

class World;
class Player;

namespace ui { namespace v2 {

// In-game tech menu overlay. Replaces the legacy spawn-an-Interface-with-
// a-SelectBox-Object path that Player::Tick used to drive when
// techstationactive transitioned to true. Cursor state lives on Player
// (techlastitem / techlastscrolled — new fields paralleling buyifacelast*);
// active state is Player::techstationactive (already replicated). This
// class owns the editing behavior + audio + Repair/Virus handoff. Sibling
// to IngameBuy / IngameChat / ModalStack.
//
// Lifecycle:
// - Player::Tick edge-detects techstationactive false->true and the local
//   peer fires Activate() (plays menu-select audio + resets cursor).
// - DispatchKey called from events.cpp KEY_DOWN when Active() (UP / DOWN
//   for selecteditem nav with auto-scroll, RETURN to commit, swallows
//   other keys).
class IngameTech
{
public:
	explicit IngameTech(World & world);

	void Activate();              // edge-triggered from Player::Tick on techstationactive↑
	bool Active() const;          // == local player exists && player->techstationactive

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
