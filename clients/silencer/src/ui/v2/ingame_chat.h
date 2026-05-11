#ifndef UI_V2_INGAME_CHAT_H
#define UI_V2_INGAME_CHAT_H

class World;

namespace ui { namespace v2 {

// In-game chat input overlay. Replaces the legacy spawn-an-Interface-with-
// a-TextInput-Object path that Player::Tick used to drive when the chat key
// was pressed. State lives on World (so the legacy DrawHud chat block in
// Renderer can read it without a Game backref); this class owns the editing
// behavior + activation gating + SendChat handoff. Sibling to ModalStack:
// not a per-state Runtime, just a thin overlay class held by Game.
//
// Lifecycle:
// - Activate() called from Player::Tick when the local player presses the
//   chat key and no buy/tech menu is open (matches legacy gating).
// - DispatchKey / DispatchText called from events.cpp KEY_DOWN / TEXT_INPUT
//   when Game::ingame_chat->Active() and the engine state is INGAME-like.
// - Submit() fires World::SendChat() then deactivates; Cancel() just
//   deactivates. ToggleTeam() flips the prefix between (ALL): / (TEAM):.
class IngameChat
{
public:
	explicit IngameChat(World & world);

	void Activate();
	void Cancel();
	void Submit();
	void ToggleTeam();
	void AppendChar(char ascii);
	void Backspace();

	bool Active() const;
	bool WithTeam() const;
	const char * Text() const;
	const char * Prefix() const { return WithTeam() ? "(TEAM):" : "(ALL):"; }

	// Convenience for events.cpp: returns true if the event was consumed.
	// Handles RETURN (Submit), ESCAPE (Cancel), TAB (ToggleTeam),
	// BACKSPACE (Backspace). Other scancodes return false unconsumed.
	bool DispatchKey(int sdl_scancode);
	// Returns true when active (chars are absorbed even past the maxchars
	// cap so they don't leak to gameplay input).
	bool DispatchText(char ascii);

private:
	World & world_;
	static constexpr int kMaxChars = 100;
};

}}  // namespace ui::v2

#endif
