package main

import "log"

// Demo-mode seed data. Enabled via the `-demo` flag in main.go.
// Used by shared/design/sdl3 QA dumps so the LOBBY screen has visible
// chat / presence / game-list / character-stat content instead of
// empty panels.
//
// SAFE TO REMOVE if the design-system spec ever moves to mocked data on
// the hydration side. Currently the hydration is "spec-only", so the
// reference dump's content has to come from a real lobby — hence this.

type demoPresence struct {
	accountID uint32
	gameID    uint32
	status    uint8 // 0 lobby, 1 pregame, 2 playing
	name      string
}

type demoChatLine struct {
	from string // empty == server-style line (no "name: " prefix)
	body string
}

func (h *Hub) SeedDemoData() {
	h.mu.Lock()
	defer h.mu.Unlock()

	h.demoMode = true

	games := []*LobbyGame{
		{Name: "Casual Match #1", MapName: "Mars Outpost",       Hostname: "10.0.0.1,15170", Port: 15170, Players: 8,  State: 1, SecurityLevel: 1, MinLevel: 0,  MaxLevel: 99, MaxPlayers: 24, MaxTeams: 6},
		{Name: "Veterans Only",   MapName: "Arsia Mons Station", Hostname: "10.0.0.2,15171", Port: 15171, Players: 4,  State: 1, SecurityLevel: 2, MinLevel: 50, MaxLevel: 99, MaxPlayers: 16, MaxTeams: 4},
		{Name: "Tutorial",        MapName: "Training Grounds",   Hostname: "10.0.0.3,15172", Port: 15172, Players: 1,  State: 1, SecurityLevel: 0, MinLevel: 0,  MaxLevel: 5,  MaxPlayers: 8,  MaxTeams: 2},
		{Name: "Capture the Tag", MapName: "Old Cydonia",        Hostname: "10.0.0.4,15173", Port: 15173, Players: 12, State: 1, SecurityLevel: 1, MinLevel: 10, MaxLevel: 99, MaxPlayers: 24, MaxTeams: 4},
	}
	for _, g := range games {
		g.ID = h.nextGID
		h.nextGID++
		h.games[g.ID] = g
	}

	h.demoPresences = []demoPresence{
		{accountID: 1001, gameID: 0, status: 0, name: "Vector"},
		{accountID: 1002, gameID: 0, status: 0, name: "Solace"},
		{accountID: 1003, gameID: 1, status: 2, name: "Krieg"},
		{accountID: 1004, gameID: 0, status: 0, name: "Ember"},
		{accountID: 1005, gameID: 4, status: 1, name: "Quill"},
		{accountID: 1006, gameID: 0, status: 0, name: "Halcyon"},
	}

	h.demoChatLines = []demoChatLine{
		{from: "Vector", body: "anyone up for a round?"},
		{from: "Solace", body: "still waiting on Krieg's match to finish"},
		{from: "Ember", body: "we got 4 in casual #1"},
		{from: "Vector", body: "joining"},
		{from: "Halcyon", body: "gg everyone"},
	}

	log.Printf("[demo] seeded %d games, %d presences, %d chat lines", len(games), len(h.demoPresences), len(h.demoChatLines))
}

// SeedDemoUser overwrites the freshly-authenticated user's agency
// stats with non-trivial fixed values so the LOBBY character panel
// renders level/wins/losses overlays with real numbers. Called after
// each successful auth when demo mode is on.
func (h *Hub) SeedDemoUser(u *User) {
	if !h.demoMode || u == nil {
		return
	}
	u.Agency[0] = Agency{Wins: 47, Losses: 12, XPToNextLevel: 220, Level: 8, Endurance: 3, Shield: 2, Jetpack: 1, TechSlots: 2, Hacking: 0, Contacts: 1}
	u.Agency[1] = Agency{Wins: 6, Losses: 9, XPToNextLevel: 30, Level: 2, Endurance: 1, Shield: 1, Jetpack: 0, TechSlots: 1, Hacking: 0, Contacts: 0}
	u.Agency[2] = Agency{Wins: 19, Losses: 5, XPToNextLevel: 80, Level: 4, Endurance: 2, Shield: 1, Jetpack: 1, TechSlots: 1, Hacking: 1, Contacts: 0}
	u.Agency[3] = Agency{}
	u.Agency[4] = Agency{Wins: 33, Losses: 22, XPToNextLevel: 410, Level: 6, Endurance: 2, Shield: 2, Jetpack: 1, TechSlots: 2, Hacking: 1, Contacts: 1}
}

// SendDemoExtras pushes the seeded presence + chat into a freshly-joined
// client. Called from Hub.Join after the standard flow when demoMode is on.
func (h *Hub) SendDemoExtras(c *Client) {
	if !h.demoMode {
		return
	}
	for _, p := range h.demoPresences {
		c.sendPresence(0, p.accountID, p.gameID, p.status, p.name)
	}
	channel := c.channel
	if channel == "" {
		channel = "Lobby"
	}
	for _, line := range h.demoChatLines {
		body := line.body
		if line.from != "" {
			body = line.from + ": " + body
		}
		c.sendChat(channel, body)
	}
}
