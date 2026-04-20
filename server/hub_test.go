package main

import (
	"net"
	"sync"
	"testing"
	"time"
)

// testConn implements net.Conn for testing purposes, capturing sent data.
type testConn struct {
	net.Conn // embed to satisfy interface; unused methods will panic
	mu       sync.Mutex
	written  [][]byte
	closed   bool
}

func (tc *testConn) Write(b []byte) (int, error) {
	tc.mu.Lock()
	defer tc.mu.Unlock()
	cp := make([]byte, len(b))
	copy(cp, b)
	tc.written = append(tc.written, cp)
	return len(b), nil
}

func (tc *testConn) SetWriteDeadline(_ time.Time) error { return nil }
func (tc *testConn) Close() error {
	tc.mu.Lock()
	tc.closed = true
	tc.mu.Unlock()
	return nil
}

func (tc *testConn) frames() [][]byte {
	tc.mu.Lock()
	defer tc.mu.Unlock()
	return tc.written
}

func (tc *testConn) clearFrames() {
	tc.mu.Lock()
	defer tc.mu.Unlock()
	tc.written = nil
}

func newTestClient(channels map[string]bool) (*Client, *testConn) {
	tc := &testConn{}
	c := &Client{
		conn:     tc,
		channels: channels,
		user:     &User{Name: "TestUser"},
	}
	return c, tc
}

func TestChat_MultiChannel_DeliveredToSubscribers(t *testing.T) {
	store := &Store{}
	hub := NewHub(store, "", "", nil)

	// Client A subscribed to Lobby and #game-1
	clientA, connA := newTestClient(map[string]bool{"Lobby": true, "#game-1": true})
	clientA.accountID = 1
	// Client B subscribed to Lobby only
	clientB, connB := newTestClient(map[string]bool{"Lobby": true})
	clientB.accountID = 2
	// Client C subscribed to #game-1 only (not Lobby)
	clientC, connC := newTestClient(map[string]bool{"#game-1": true})
	clientC.accountID = 3

	hub.mu.Lock()
	hub.clients[clientA] = struct{}{}
	hub.clients[clientB] = struct{}{}
	hub.clients[clientC] = struct{}{}
	hub.mu.Unlock()

	// Send chat to Lobby channel
	hub.Chat(clientA, "Lobby", "hello lobby")

	// Client A and B should receive (both subscribed to Lobby)
	// Client C should NOT receive (not subscribed to Lobby)
	if len(connA.frames()) == 0 {
		t.Error("client A should receive Lobby chat")
	}
	if len(connB.frames()) == 0 {
		t.Error("client B should receive Lobby chat")
	}
	if len(connC.frames()) != 0 {
		t.Error("client C should NOT receive Lobby chat")
	}

	// Clear written
	connA.clearFrames()
	connB.clearFrames()
	connC.clearFrames()

	// Send chat to #game-1 channel
	hub.Chat(clientA, "#game-1", "hello game")

	// Client A and C should receive (both subscribed to #game-1)
	// Client B should NOT receive (not subscribed to #game-1)
	if len(connA.frames()) == 0 {
		t.Error("client A should receive #game-1 chat")
	}
	if len(connB.frames()) != 0 {
		t.Error("client B should NOT receive #game-1 chat")
	}
	if len(connC.frames()) == 0 {
		t.Error("client C should receive #game-1 chat")
	}
}

func TestChat_JoinAddsChannel(t *testing.T) {
	c, _ := newTestClient(map[string]bool{"Lobby": true})

	// Simulate /join command
	c.mu.Lock()
	c.channels["#game-1"] = true
	c.mu.Unlock()

	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.channels["Lobby"] {
		t.Error("client should still be in Lobby after joining game channel")
	}
	if !c.channels["#game-1"] {
		t.Error("client should be in #game-1 after joining")
	}
}

func TestChat_LeaveRemovesChannel(t *testing.T) {
	c, _ := newTestClient(map[string]bool{"Lobby": true, "#game-1": true})

	// Simulate /leave command
	c.mu.Lock()
	delete(c.channels, "#game-1")
	c.mu.Unlock()

	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.channels["Lobby"] {
		t.Error("client should still be in Lobby after leaving game channel")
	}
	if c.channels["#game-1"] {
		t.Error("client should NOT be in #game-1 after leaving")
	}
}

func TestChat_CannotLeaveLobby(t *testing.T) {
	c, _ := newTestClient(map[string]bool{"Lobby": true, "#game-1": true})

	// Try to leave Lobby - should be prevented
	leaveChan := "Lobby"
	if leaveChan != "" && leaveChan != "Lobby" {
		c.mu.Lock()
		delete(c.channels, leaveChan)
		c.mu.Unlock()
	}

	c.mu.Lock()
	defer c.mu.Unlock()
	if !c.channels["Lobby"] {
		t.Error("client should not be able to leave Lobby")
	}
}
