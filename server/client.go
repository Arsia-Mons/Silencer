package main

import (
	"bufio"
	"io"
	"log"
	"net"
	"strings"
	"sync"
	"time"
)

type Client struct {
	conn net.Conn
	br   *bufio.Reader
	hub  *Hub

	mu        sync.Mutex
	accountID uint32
	user      *User
	channel   string
	gameID    uint32 // 0 = main lobby; protected by Hub.mu once authed
	// gameStatus: 0 = main lobby (gameID must be 0), 1 = pregame
	// (game-specific lobby, connected to dedicated but match not started),
	// 2 = playing. Protected by Hub.mu once authed.
	gameStatus uint8
	closed     bool
}

func (c *Client) displayName() string {
	if c.user != nil && c.user.Name != "" {
		return c.user.Name
	}
	return "Player"
}

func serveClient(conn net.Conn, hub *Hub, version string, manifest *UpdateManifest) {
	defer conn.Close()
	c := &Client{
		conn:    conn,
		br:      bufio.NewReader(conn),
		hub:     hub,
		channel: "Lobby",
	}
	log.Printf("[conn] %s connected", conn.RemoteAddr())

	pingStop := make(chan struct{})
	go c.pingLoop(pingStop)
	defer close(pingStop)

	for {
		_ = conn.SetReadDeadline(time.Now().Add(30 * time.Second))
		frame, err := readFrame(c.br)
		if err != nil {
			if err != io.EOF {
				log.Printf("[conn] %s read: %v", conn.RemoteAddr(), err)
			}
			break
		}
		if err := c.handleFrame(frame, version, manifest); err != nil {
			log.Printf("[conn] %s handle: %v", conn.RemoteAddr(), err)
			break
		}
	}
	c.hub.Leave(c)
	log.Printf("[conn] %s disconnected", conn.RemoteAddr())
}

func (c *Client) pingLoop(stop chan struct{}) {
	t := time.NewTicker(10 * time.Second)
	defer t.Stop()
	for {
		select {
		case <-stop:
			return
		case <-t.C:
			c.send([]byte{opPing})
		}
	}
}

func (c *Client) send(payload []byte) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return
	}
	_ = c.conn.SetWriteDeadline(time.Now().Add(5 * time.Second))
	if err := writeFrame(c.conn, payload); err != nil {
		c.closed = true
		_ = c.conn.Close()
	}
}

func (c *Client) handleFrame(frame []byte, expectedVersion string, manifest *UpdateManifest) error {
	r := newReader(frame)
	op, err := r.u8()
	if err != nil {
		return err
	}
	switch op {
	case opVersion:
		// Everything after the opcode is the payload for decodeVersionRequest.
		req, err := decodeVersionRequest(frame[1:])
		if err != nil {
			return err
		}
		c.handleVersion(req, expectedVersion, manifest)
	case opAuth:
		return c.handleAuth(r)
	case opChat:
		return c.handleChat(r)
	case opNewGame:
		return c.handleNewGame(r)
	case opUserInfo:
		return c.handleUserInfo(r)
	case opPing:
		// client ack; nothing to do
	case opUpgradeStat:
		return c.handleUpgradeStat(r)
	case opRegisterStats:
		return c.handleRegisterStats(r)
	case opSetGame:
		return c.handleSetGame(r)
	default:
		log.Printf("[op] %s unknown opcode %d", c.conn.RemoteAddr(), op)
	}
	return nil
}

func (c *Client) handleVersion(req VersionRequest, expectedVersion string, manifest *UpdateManifest) {
	ok := expectedVersion == "" || req.Version == expectedVersion

	rep := VersionReply{OK: ok}
	if !ok && manifest != nil {
		if manifest.Version != expectedVersion {
			log.Printf("[lobby-update] manifest stale: manifest version %q != lobby version %q; sending bare reject",
				manifest.Version, expectedVersion)
		} else {
			switch req.Platform {
			case PlatformMacOSARM64:
				rep.URL = manifest.MacOSURL
				rep.SHA256 = manifest.MacOSSHA256
			case PlatformWindowsX64:
				rep.URL = manifest.WindowsURL
				rep.SHA256 = manifest.WindowsSHA256
			default:
				log.Printf("[lobby-update] client %s sent unknown platform %d; sending bare reject",
					c.conn.RemoteAddr(), req.Platform)
			}
		}
	}

	log.Printf("[lobby-update] version check %s: client=%q expected=%q ok=%v update_url_len=%d",
		c.conn.RemoteAddr(), req.Version, expectedVersion, ok, len(rep.URL))

	payload := append([]byte{opVersion}, encodeVersionReply(rep)...)
	c.send(payload)
}

func (c *Client) handleAuth(r *reader) error {
	name, err := r.cstr(17)
	if err != nil {
		return err
	}
	hash, err := r.bytes(20)
	if err != nil {
		return err
	}
	u, ok := c.hub.store.Login(name, hash)
	if !ok {
		msg := "Incorrect password for " + name
		w := &writer{}
		w.u8(opAuth)
		w.u8(0)
		w.cstr(msg)
		c.send(w.b)
		return nil
	}
	c.mu.Lock()
	c.user = u
	c.accountID = u.AccountID
	c.mu.Unlock()

	w := &writer{}
	w.u8(opAuth)
	w.u8(1)
	w.u32(u.AccountID)
	c.send(w.b)

	c.sendMOTD()
	c.sendChannel(c.channel)
	c.hub.Join(c)
	return nil
}

func (c *Client) sendMOTD() {
	const chunkMax = 200
	for i := 0; i < len(c.hub.motd); i += chunkMax {
		end := i + chunkMax
		if end > len(c.hub.motd) {
			end = len(c.hub.motd)
		}
		text := c.hub.motd[i:end]
		// The client strcat's from offset 1, so the "status" byte is actually the
		// first printable character of the text. Any non-zero byte means "more".
		w := &writer{}
		w.u8(opMOTD)
		w.cstr(text) // text then null terminator
		c.send(w.b)
	}
	// Terminator: status=0.
	c.send([]byte{opMOTD, 0})
}

func (c *Client) sendChannel(name string) {
	w := &writer{}
	w.u8(opChannel)
	w.cstr(name)
	c.send(w.b)
}

func (c *Client) sendChat(channel, msg string) {
	w := &writer{}
	w.u8(opChat)
	w.cstr(channel)
	w.cstr(msg)
	c.send(w.b)
}

func (c *Client) sendNewGame(status uint8, g *LobbyGame) {
	w := &writer{}
	w.u8(opNewGame)
	w.u8(status)
	g.Encode(w)
	c.send(w.b)
}

func (c *Client) sendDelGame(id uint32) {
	w := &writer{}
	w.u8(opDelGame)
	w.u32(id)
	c.send(w.b)
}

// action: 0 = add/upsert, 1 = remove.
// status: 0 = lobby, 1 = pregame, 2 = playing.
func (c *Client) sendPresence(action uint8, accountID, gameID uint32, status uint8, name string) {
	w := &writer{}
	w.u8(opPresence)
	w.u8(action)
	w.u32(accountID)
	w.u32(gameID)
	w.u8(status)
	w.lenStr(name)
	c.send(w.b)
}

func (c *Client) handleChat(r *reader) error {
	channel, err := r.cstr(64)
	if err != nil {
		return err
	}
	msg, err := r.cstr(maxFrame)
	if err != nil {
		return err
	}
	if strings.HasPrefix(msg, "/join ") {
		newChan := strings.TrimSpace(msg[len("/join "):])
		if newChan == "" {
			newChan = "Lobby"
		}
		c.mu.Lock()
		c.channel = newChan
		c.mu.Unlock()
		c.sendChannel(newChan)
		return nil
	}
	c.hub.Chat(c, channel, msg)
	return nil
}

func (c *Client) handleNewGame(r *reader) error {
	if c.accountID == 0 {
		return nil
	}
	g := &LobbyGame{}
	if err := g.Decode(r); err != nil {
		return err
	}
	g.AccountID = c.accountID
	g.Players = 1
	g.State = 0
	c.hub.RequestCreateGame(c, g)
	return nil
}

func (c *Client) handleUserInfo(r *reader) error {
	id, err := r.u32()
	if err != nil {
		return err
	}
	// Bots: id in top-24 range is handled locally by the client.
	if id >= 0xFFFFFFFF-24 {
		return nil
	}
	u := c.hub.store.ByAccountID(id)
	if u == nil {
		// Client expects a response; send a stub so it doesn't spin.
		u = &User{AccountID: id, Name: "Unknown"}
	}
	w := &writer{}
	w.u8(opUserInfo)
	encodeUser(w, u)
	c.send(w.b)
	return nil
}

func (c *Client) handleUpgradeStat(r *reader) error {
	if c.accountID == 0 {
		return nil
	}
	agency, err := r.u8()
	if err != nil {
		return err
	}
	stat, err := r.u8()
	if err != nil {
		return err
	}
	if c.hub.store.UpgradeStat(c.accountID, agency, stat) {
		c.send([]byte{opUpgradeStat})
	}
	return nil
}

func (c *Client) handleSetGame(r *reader) error {
	if c.accountID == 0 {
		return nil
	}
	gameID, err := r.u32()
	if err != nil {
		return err
	}
	status, err := r.u8()
	if err != nil {
		return err
	}
	c.hub.SetClientGame(c, gameID, status)
	return nil
}

func (c *Client) handleRegisterStats(r *reader) error {
	if _, err := r.u32(); err != nil { // gameid
		return err
	}
	if _, err := r.u8(); err != nil { // teamnumber
		return err
	}
	acct, err := r.u32()
	if err != nil {
		return err
	}
	statsagency, err := r.u8()
	if err != nil {
		return err
	}
	won, err := r.u8()
	if err != nil {
		return err
	}
	xp, err := r.u32()
	if err != nil {
		return err
	}
	// remaining bytes = Stats::Serialize blob, ignored.
	c.hub.store.UpdateStats(acct, statsagency, won != 0, xp)
	return nil
}

func boolU8(b bool) uint8 {
	if b {
		return 1
	}
	return 0
}

func itoa(n uint16) string {
	if n == 0 {
		return "0"
	}
	var buf [8]byte
	i := len(buf)
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	return string(buf[i:])
}
