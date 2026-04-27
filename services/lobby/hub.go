package main

import (
	"log"
	"net"
	"sync"
	"time"
)

type Hub struct {
	store      *Store
	motd       string
	publicAddr string
	proc       *procManager
	events     *EventPublisher // nil if RabbitMQ not configured

	mu      sync.Mutex
	nextGID uint32
	games   map[uint32]*LobbyGame
	pending map[uint32]*pendingGame
	clients map[*Client]struct{}

	// Demo mode (`-demo` flag) seed data, populated by SeedDemoData.
	demoMode      bool
	demoPresences []demoPresence
	demoChatLines []demoChatLine
}

type pendingGame struct {
	game  *LobbyGame
	owner *Client
	timer *time.Timer
}

const pendingTimeout = 30 * time.Second

func NewHub(store *Store, motd, publicAddr string, proc *procManager, events *EventPublisher) *Hub {
	return &Hub{
		store:      store,
		motd:       motd,
		publicAddr: publicAddr,
		proc:       proc,
		events:     events,
		nextGID:    1,
		games:      map[uint32]*LobbyGame{},
		pending:    map[uint32]*pendingGame{},
		clients:    map[*Client]struct{}{},
	}
}

func (h *Hub) Join(c *Client) {
	h.mu.Lock()
	h.clients[c] = struct{}{}
	games := make([]*LobbyGame, 0, len(h.games))
	for _, g := range h.games {
		games = append(games, g)
	}
	type snap struct {
		acct   uint32
		gameID uint32
		status uint8
		name   string
	}
	others := make([]snap, 0, len(h.clients))
	peers := make([]*Client, 0, len(h.clients))
	for other := range h.clients {
		if other == c || other.accountID == 0 {
			continue
		}
		others = append(others, snap{other.accountID, other.gameID, other.gameStatus, other.displayName()})
		peers = append(peers, other)
	}
	selfSnap := snap{c.accountID, c.gameID, c.gameStatus, c.displayName()}
	h.mu.Unlock()

	for _, g := range games {
		c.sendNewGame(1, g)
	}
	if c.accountID == 0 {
		return
	}
	for _, o := range others {
		c.sendPresence(0, o.acct, o.gameID, o.status, o.name)
	}
	for _, p := range peers {
		p.sendPresence(0, selfSnap.acct, selfSnap.gameID, selfSnap.status, selfSnap.name)
	}
	c.sendPresence(0, selfSnap.acct, selfSnap.gameID, selfSnap.status, selfSnap.name)

	h.SendDemoExtras(c)

	if h.events != nil && c.accountID != 0 {
		ip, _, _ := net.SplitHostPort(c.conn.RemoteAddr().String())
		var agencies [5]AgencyEvent
		if c.user != nil {
			for i, a := range c.user.Agency {
				agencies[i] = agencyToEvent(a)
			}
		}
		h.events.Publish("player.login", playerLoginEvent{
			AccountID: c.accountID, Name: c.displayName(),
			IP: ip, Agencies: agencies, Timestamp: time.Now().UnixMilli(),
		})
	}
}

func (h *Hub) Leave(c *Client) {
	h.mu.Lock()
	delete(h.clients, c)

	var dropReady []uint32
	for id, g := range h.games {
		if c.accountID != 0 && g.AccountID == c.accountID {
			dropReady = append(dropReady, id)
		}
	}
	for _, id := range dropReady {
		delete(h.games, id)
	}

	var dropPending []uint32
	for id, pg := range h.pending {
		if pg.owner == c {
			dropPending = append(dropPending, id)
			if pg.timer != nil {
				pg.timer.Stop()
			}
		}
	}
	for _, id := range dropPending {
		delete(h.pending, id)
	}

	others := make([]*Client, 0, len(h.clients))
	for other := range h.clients {
		others = append(others, other)
	}
	leavingAcct := c.accountID
	h.mu.Unlock()

	for _, id := range append(dropReady, dropPending...) {
		h.proc.Stop(id)
	}
	for _, id := range dropReady {
		for _, other := range others {
			other.sendDelGame(id)
		}
	}
	if leavingAcct != 0 {
		for _, other := range others {
			other.sendPresence(1, leavingAcct, 0, 0, "")
		}
	}

	if h.events != nil && leavingAcct != 0 {
		now := time.Now().UnixMilli()
		for _, id := range append(dropReady, dropPending...) {
			h.events.Publish("game.ended", gameEndedEvent{GameID: id, Timestamp: now})
		}
		h.events.Publish("player.logout", playerLogoutEvent{
			AccountID: leavingAcct, Name: c.displayName(), Timestamp: now,
		})
	}
}

// SetClientGame updates a client's gameID+status and announces the change.
// status: 0 = main lobby, 1 = pregame (game-specific lobby), 2 = playing.
// gameID=0 requires status=0. Unknown non-zero IDs are rejected.
func (h *Hub) SetClientGame(c *Client, gameID uint32, status uint8) {
	h.mu.Lock()
	if c.accountID == 0 {
		h.mu.Unlock()
		return
	}
	if gameID == 0 {
		status = 0
	} else {
		_, inGames := h.games[gameID]
		_, inPending := h.pending[gameID]
		if !inGames && !inPending {
			h.mu.Unlock()
			log.Printf("[hub] client %d set-game for unknown game %d; ignoring", c.accountID, gameID)
			return
		}
		if status != 1 && status != 2 {
			status = 1
		}
	}
	c.gameID = gameID
	c.gameStatus = status
	acct := c.accountID
	name := c.displayName()
	peers := make([]*Client, 0, len(h.clients))
	for other := range h.clients {
		peers = append(peers, other)
	}
	h.mu.Unlock()

	for _, p := range peers {
		p.sendPresence(0, acct, gameID, status, name)
	}

	if h.events != nil {
		h.events.Publish("player.presence", playerPresenceEvent{
			AccountID: acct, Name: name, GameID: gameID, GameStatus: status,
			Online: true, Timestamp: time.Now().UnixMilli(),
		})
	}
}

// RequestCreateGame spawns a dedicated server and defers the reply to the
// owner until the first UDP heartbeat lands.
func (h *Hub) RequestCreateGame(owner *Client, g *LobbyGame) {
	h.mu.Lock()

	// Drop any existing game already owned by this player
	var dropExisting []uint32
	for id, eg := range h.games {
		if owner.accountID != 0 && eg.AccountID == owner.accountID {
			dropExisting = append(dropExisting, id)
		}
	}
	for _, id := range dropExisting {
		delete(h.games, id)
	}
	var dropPendingExisting []uint32
	for id, pg := range h.pending {
		if pg.owner == owner {
			dropPendingExisting = append(dropPendingExisting, id)
			if pg.timer != nil {
				pg.timer.Stop()
			}
		}
	}
	for _, id := range dropPendingExisting {
		delete(h.pending, id)
	}
	others := make([]*Client, 0, len(h.clients))
	for other := range h.clients {
		others = append(others, other)
	}

	gid := h.nextGID
	h.nextGID++
	g.ID = gid
	pg := &pendingGame{game: g, owner: owner}
	h.pending[gid] = pg
	h.mu.Unlock()

	// Notify other clients of removed games and stop their processes
	allDrop := append(dropExisting, dropPendingExisting...)
	for _, id := range allDrop {
		h.proc.Stop(id)
		for _, other := range others {
			other.sendDelGame(id)
		}
	}
	if h.events != nil && len(allDrop) > 0 {
		now := time.Now().UnixMilli()
		for _, id := range allDrop {
			h.events.Publish("game.ended", gameEndedEvent{GameID: id, Timestamp: now})
		}
	}

	if err := h.proc.Start(gid, owner.accountID); err != nil {
		log.Printf("[hub] spawn failed for game %d: %v", gid, err)
		h.failPending(gid, "Could not spawn dedicated server: "+err.Error())
		return
	}
	if h.events != nil {
		h.events.Publish("game.created", gameCreatedEvent{
			GameID: gid, AccountID: owner.accountID, Name: g.Name, MapName: g.MapName,
			Timestamp: time.Now().UnixMilli(),
		})
	}
	pg.timer = time.AfterFunc(pendingTimeout, func() {
		h.failPending(gid, "Dedicated server did not start in time")
	})
}

func (h *Hub) failPending(gid uint32, reason string) {
	h.mu.Lock()
	pg, ok := h.pending[gid]
	if !ok {
		h.mu.Unlock()
		return
	}
	delete(h.pending, gid)
	h.mu.Unlock()

	if pg.timer != nil {
		pg.timer.Stop()
	}
	h.proc.Stop(gid)
	log.Printf("[hub] game %d failed: %s", gid, reason)
	// status=2: not 1 (success), not 0 (initial), not 100 (pending) — triggers
	// the client's "Could not create game" dialog (see src/game.cpp:798).
	pg.owner.sendNewGame(2, pg.game)
}

// OnHeartbeat is called from the UDP listener when a dedicated server pings.
func (h *Hub) OnHeartbeat(gameID uint32, sourceIP string, port uint16, state uint8) {
	h.mu.Lock()
	if pg, ok := h.pending[gameID]; ok {
		delete(h.pending, gameID)
		if pg.timer != nil {
			pg.timer.Stop()
		}
		host := h.publicAddr
		if host == "" {
			host = sourceIP
		}
		pg.game.Hostname = host + "," + itoa(port)
		pg.game.Port = port
		pg.game.State = state
		h.games[gameID] = pg.game

		others := make([]*Client, 0, len(h.clients))
		for c := range h.clients {
			if c != pg.owner {
				others = append(others, c)
			}
		}
		h.mu.Unlock()

		pg.owner.sendNewGame(1, pg.game)
		for _, c := range others {
			c.sendNewGame(1, pg.game)
		}
		log.Printf("[hub] game %d ready at %s:%d", gameID, host, port)
		if h.events != nil {
			h.events.Publish("game.ready", gameReadyEvent{
				GameID: gameID, AccountID: pg.game.AccountID, Name: pg.game.Name,
				MapName: pg.game.MapName, Hostname: host, Port: port,
				Timestamp: time.Now().UnixMilli(),
			})
		}
		return
	}

	g, ok := h.games[gameID]
	if !ok {
		h.mu.Unlock()
		log.Printf("[udp] heartbeat for unknown game %d", gameID)
		return
	}
	changed := g.State != state
	g.State = state
	snapshot := *g
	peers := make([]*Client, 0, len(h.clients))
	if changed {
		for c := range h.clients {
			peers = append(peers, c)
		}
	}
	h.mu.Unlock()
	for _, c := range peers {
		c.sendNewGame(1, &snapshot)
	}
}

// Chat scopes messages to clients in the same channel.
func (h *Hub) Chat(from *Client, channel, msg string) {
	h.mu.Lock()
	peers := make([]*Client, 0, len(h.clients))
	for c := range h.clients {
		if c.channel == channel {
			peers = append(peers, c)
		}
	}
	h.mu.Unlock()
	line := from.displayName() + ": " + msg
	for _, c := range peers {
		c.sendChat(channel, line)
	}
	log.Printf("[chat #%s] %s", channel, line)
}

// KickAccountID disconnects a player's lobby connection (if present) and
// sends a UDP kick command to any dedicated game server they are currently in.
func (h *Hub) KickAccountID(accountID uint32) {
	h.mu.Lock()
	var target *Client
	for c := range h.clients {
		if c.accountID == accountID {
			target = c
			break
		}
	}
	var gamePort uint16
	if target != nil && target.gameID != 0 {
		if g, ok := h.games[target.gameID]; ok {
			gamePort = g.Port
		}
	}
	h.mu.Unlock()

	if target != nil {
		log.Printf("[hub] kicking banned account %d from lobby", accountID)
		target.conn.Close()
	}

	if gamePort != 0 {
		addr := net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: int(gamePort)}
		conn, err := net.DialUDP("udp", nil, &addr)
		if err == nil {
			msg := make([]byte, 5)
			msg[0] = 22 // MSG_KICK ordinal in C++ enum
			msg[1] = byte(accountID)
			msg[2] = byte(accountID >> 8)
			msg[3] = byte(accountID >> 16)
			msg[4] = byte(accountID >> 24)
			_, _ = conn.Write(msg)
			conn.Close()
			log.Printf("[hub] sent UDP kick for account %d to game port %d", accountID, gamePort)
		}
	}
}
