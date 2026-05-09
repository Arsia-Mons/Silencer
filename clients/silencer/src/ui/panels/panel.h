#ifndef PANEL_H
#define PANEL_H

#include <SDL3/SDL_stdinc.h>

class ScreenContext;
class Interface;

// Sub-component owned by a Screen. Multiple Panels are visible concurrently
// inside one Screen (e.g. LobbyScreen owns CharacterPanel + ChatPanel + a
// swappable main panel). Panels are NOT on the screen stack — the parent
// Screen builds, ticks, swaps, and destroys them.
class Panel
{
public:
	virtual ~Panel() = default;

	// Build widgets and attach to the parent Interface.
	virtual void Build(ScreenContext & ctx, Interface * parent) = 0;

	// Called by the parent Screen's Tick when this panel should update.
	virtual void Tick(ScreenContext & ctx) = 0;

	// Tear down widgets. Called when the parent Screen pops or swaps the panel.
	virtual void Destroy(ScreenContext & ctx) = 0;

	Uint16 interfaceId = 0;
};

#endif
