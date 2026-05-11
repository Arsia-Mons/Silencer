package main

// WebSocket spectator facade — Stage 3 of the WASM browser spectator
// project (docs/plans/2026-05-10-wasm-spectator.md).
//
// Browsers can't speak the lobby's raw TCP byte protocol, so this
// endpoint re-emits a JSON envelope of the same lifecycle events
// (initial game list + new/del pushes). It does NOT carry the full
// chat / presence / auth surface — spectators are anonymous and
// read-only. The native lobby path on the TCP listener is untouched.
//
// Wire format: JSON envelopes, one per WebSocket text frame. Browser
// → server commands are simple objects:
//
//   { "type": "spectate", "gameid": 12 }
//
// Server → browser messages:
//
//   { "type": "hello", "motd": "...", "games": [LobbyGameJSON, ...] }
//   { "type": "newgame", "game": LobbyGameJSON }
//   { "type": "delgame", "id": 12 }
//   { "type": "spectate_url", "url": "ws://..." }       // success
//   { "type": "error", "code": "...", "message": "..." } // any failure
//
// LobbyGameJSON includes only the fields a spectator needs (no
// password). `spectatable` mirrors the native bit; `can_rejoin` is
// always false for anonymous spectators (they have no account to
// match against ParkedAccountIDs).

import (
	"context"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"sync"
	"time"
)

// SpectatorFacade is the HTTP+WebSocket-facing wrapper around Hub.
// Listens on its own address (default :15173); separate from the
// native TCP lobby port so we can run them side by side.
type SpectatorFacade struct {
	hub *Hub
	// procManager is used to spawn a `silencer --relay` per game on
	// first spectate request (Stage 7). Optional: if nil, handleSpectate
	// falls back to the legacy `relayBase` configured URL hint.
	proc      *procManager
	relayBase string // pre-Stage-7 fallback (e.g. ws://lobby.example.com:15174/)
}

// LobbyGameJSON is the publicly-safe projection of LobbyGame. We strip
// the password and keep everything else byte-compatible with what the
// native client receives.
type LobbyGameJSON struct {
	ID            uint32 `json:"id"`
	AccountID     uint32 `json:"account_id"`
	Name          string `json:"name"`
	Hostname      string `json:"hostname"`
	MapName       string `json:"map_name"`
	MapHash       string `json:"map_hash"` // hex-encoded SHA-1
	Players       uint8  `json:"players"`
	State         uint8  `json:"state"`
	SecurityLevel uint8  `json:"security_level"`
	MinLevel      uint8  `json:"min_level"`
	MaxLevel      uint8  `json:"max_level"`
	MaxPlayers    uint8  `json:"max_players"`
	MaxTeams      uint8  `json:"max_teams"`
	Extra         uint8  `json:"extra"`
	Spectatable   uint8  `json:"spectatable"`
	Port          uint16 `json:"port"`
}

func toLobbyGameJSON(g *LobbyGame) LobbyGameJSON {
	return LobbyGameJSON{
		ID:            g.ID,
		AccountID:     g.AccountID,
		Name:          g.Name,
		Hostname:      g.Hostname,
		MapName:       g.MapName,
		MapHash:       hex.EncodeToString(g.MapHash[:]),
		Players:       g.Players,
		State:         g.State,
		SecurityLevel: g.SecurityLevel,
		MinLevel:      g.MinLevel,
		MaxLevel:      g.MaxLevel,
		MaxPlayers:    g.MaxPlayers,
		MaxTeams:      g.MaxTeams,
		Extra:         g.Extra,
		Spectatable:   g.Spectatable,
		Port:          g.Port,
	}
}

// envelope is the universal server→client and client→server JSON
// shape. Concrete fields beyond Type are pulled from the variant
// structs below via json.Marshal/Unmarshal on the same struct.
type envelope struct {
	Type    string          `json:"type"`
	Game    *LobbyGameJSON  `json:"game,omitempty"`
	Games   []LobbyGameJSON `json:"games,omitempty"`
	ID      uint32          `json:"id,omitempty"`
	GameID  uint32          `json:"gameid,omitempty"`
	URL     string          `json:"url,omitempty"`
	MOTD    string          `json:"motd,omitempty"`
	Code    string          `json:"code,omitempty"`
	Message string          `json:"message,omitempty"`
}

// wsClient implements Hub.Subscriber by JSON-encoding deltas and
// pushing them through the WebSocket. Writes are serialised by wsConn's
// own mutex.
type wsClient struct {
	conn *wsConn
	// closed is set when SendClose has been called. Subsequent OnNewGame /
	// OnDelGame become no-ops so the connection isn't double-written.
	mu     sync.Mutex
	closed bool
}

func (c *wsClient) send(env envelope) {
	c.mu.Lock()
	if c.closed {
		c.mu.Unlock()
		return
	}
	c.mu.Unlock()
	b, err := json.Marshal(env)
	if err != nil {
		log.Printf("[wsfacade] marshal: %v", err)
		return
	}
	if err := c.conn.SendText(b); err != nil {
		c.markClosed()
	}
}

func (c *wsClient) markClosed() {
	c.mu.Lock()
	c.closed = true
	c.mu.Unlock()
}

func (c *wsClient) OnNewGame(g *LobbyGame) {
	j := toLobbyGameJSON(g)
	c.send(envelope{Type: "newgame", Game: &j})
}

func (c *wsClient) OnDelGame(id uint32) {
	c.send(envelope{Type: "delgame", ID: id})
}

// StartSpectatorFacade brings up the HTTP server hosting `/spectate`
// (WebSocket upgrade). Returns immediately; the server runs in a
// goroutine. `proc` (Stage 7) lets handleSpectate spawn a relay binary
// on demand per game; if nil we fall back to round-tripping the
// configured `relayBase` URL hint.
func StartSpectatorFacade(addr string, hub *Hub, proc *procManager, relayBase string) {
	fac := &SpectatorFacade{hub: hub, proc: proc, relayBase: relayBase}
	mux := http.NewServeMux()
	mux.HandleFunc("/spectate", fac.handle)
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "ok\n")
	})
	srv := &http.Server{
		Addr:              addr,
		Handler:           mux,
		ReadHeaderTimeout: 15 * time.Second,
	}
	log.Printf("[wsfacade] listening on %s (relay base=%q)", addr, relayBase)
	go func() {
		if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Printf("[wsfacade] listen: %v", err)
		}
	}()
}

func (f *SpectatorFacade) handle(w http.ResponseWriter, r *http.Request) {
	ws := wsAccept(w, r)
	if ws == nil {
		return
	}
	log.Printf("[wsfacade] %s connected", r.RemoteAddr)

	client := &wsClient{conn: ws}

	// Snapshot initial state THEN subscribe. If we subscribed first, we
	// could miss a brand-new game whose broadcast races our snapshot
	// (delivered before we add ourselves) — or worse, double-deliver a
	// game that appeared between snapshot and subscribe.
	games := f.hub.Snapshot()
	jsonGames := make([]LobbyGameJSON, len(games))
	for i := range games {
		jsonGames[i] = toLobbyGameJSON(&games[i])
	}
	client.send(envelope{Type: "hello", MOTD: f.hub.MOTD(), Games: jsonGames})
	f.hub.AddSubscriber(client)
	defer f.hub.RemoveSubscriber(client)

	defer func() {
		client.markClosed()
		_ = ws.SendClose()
	}()

	// Set a generous per-message read deadline; the spectator might
	// idle for minutes browsing the list.
	_, cancel := context.WithCancel(r.Context())
	defer cancel()

	for {
		payload, err := ws.Recv()
		if err != nil {
			if err != io.EOF {
				log.Printf("[wsfacade] recv: %v", err)
			}
			return
		}
		var cmd envelope
		if err := json.Unmarshal(payload, &cmd); err != nil {
			client.send(envelope{Type: "error", Code: "BAD_JSON", Message: err.Error()})
			continue
		}
		switch cmd.Type {
		case "spectate":
			f.handleSpectate(client, cmd.GameID)
		default:
			client.send(envelope{Type: "error", Code: "UNKNOWN_TYPE", Message: cmd.Type})
		}
	}
}

// handleSpectate returns a relay URL for the given gameid. With Stage 7
// wired (procManager non-nil), we ensure a `silencer --relay` process is
// running for this game and return its real WS URL. Without procManager
// (or when binary isn't configured), we fall back to round-tripping the
// configured `relayBase` URL hint.
func (f *SpectatorFacade) handleSpectate(c *wsClient, gameID uint32) {
	if gameID == 0 {
		c.send(envelope{Type: "error", Code: "BAD_GAMEID", Message: "gameid required"})
		return
	}
	// Look up the game to verify it's live and spectatable.
	games := f.hub.Snapshot()
	var match *LobbyGame
	for i := range games {
		if games[i].ID == gameID {
			match = &games[i]
			break
		}
	}
	if match == nil {
		c.send(envelope{Type: "error", Code: "NO_SUCH_GAME", Message: fmt.Sprintf("game %d not live", gameID)})
		return
	}
	if match.Spectatable == 0 {
		c.send(envelope{Type: "error", Code: "NOT_SPECTATABLE", Message: "game is not currently spectatable"})
		return
	}

	// Spawn-on-demand relay. StartRelay is idempotent — calling it for
	// an already-running game returns the existing URL.
	if f.proc != nil {
		// LobbyGame.Hostname is "ip,port" (legacy concat from heartbeats).
		// Split on the comma so the relay's inet_addr gets dotted-decimal.
		peerHost := match.Hostname
		if i := strings.IndexByte(peerHost, ','); i >= 0 {
			peerHost = peerHost[:i]
		}
		url, err := f.proc.StartRelay(gameID, peerHost, match.Port)
		if err != nil {
			log.Printf("[wsfacade] StartRelay game=%d: %v", gameID, err)
			c.send(envelope{Type: "error", Code: "NO_RELAY", Message: err.Error()})
			return
		}
		c.send(envelope{Type: "spectate_url", URL: url, GameID: gameID})
		return
	}

	if f.relayBase == "" {
		c.send(envelope{Type: "error", Code: "NO_RELAY", Message: "relay endpoint not configured"})
		return
	}
	url := fmt.Sprintf("%s?gameid=%d", f.relayBase, gameID)
	c.send(envelope{Type: "spectate_url", URL: url, GameID: gameID})
}
