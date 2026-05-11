package main

import (
	"errors"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strconv"
	"sync"
)

type procManager struct {
	binary    string
	lobbyPort int

	gamePortBase  int
	gamePortCount int

	// Stage 7 (WASM spectator project): one server-side `silencer --relay`
	// child per spectatable INGAME match, spawned on first browser ask.
	// Port assignment: relayPortBase + (gameID % relayPortCount). Tracked
	// separately from `procs` so a dedicated server and its relay can
	// coexist and be killed independently. The relay's public WS URL is
	// constructed from publicAddr at StartRelay time.
	relayPortBase  int
	relayPortCount int
	publicAddr     string
	relayMu        sync.Mutex
	relays         map[uint32]*relayChild

	onExit func(gameID uint32)

	mu    sync.Mutex
	procs map[uint32]*exec.Cmd
}

type relayChild struct {
	cmd  *exec.Cmd
	url  string
	port int
}

func newProcManager(binary string, lobbyPort, gamePortBase, gamePortCount int) *procManager {
	return &procManager{
		binary:        binary,
		lobbyPort:     lobbyPort,
		gamePortBase:  gamePortBase,
		gamePortCount: gamePortCount,
		procs:         map[uint32]*exec.Cmd{},
		relays:        map[uint32]*relayChild{},
		relayPortBase: 25174,
		relayPortCount: 100,
		publicAddr:    "127.0.0.1",
	}
}

// ConfigureRelay overrides the relay-port range + public address. Called
// once at startup from main.go.
func (p *procManager) ConfigureRelay(base, count int, publicAddr string) {
	p.relayPortBase = base
	p.relayPortCount = count
	if publicAddr != "" {
		p.publicAddr = publicAddr
	}
}

// StartRelay spawns a `silencer --relay <peerHost> <peerPort> <gameID>
// --ws-port=N` process for the given game, or returns the URL of an
// already-running relay. The browser opens a WebSocket to that URL and
// the relay forwards AUTHORITY UDP snapshots verbatim over it. Idempotent:
// callers can invoke StartRelay on every spectate request without
// spawning duplicates.
func (p *procManager) StartRelay(gameID uint32, peerHost string, peerPort uint16) (string, error) {
	if p.binary == "" {
		return "", errors.New("-game-binary not configured")
	}
	if peerHost == "" || peerPort == 0 {
		return "", fmt.Errorf("game %d has no AUTHORITY heartbeat yet", gameID)
	}

	p.relayMu.Lock()
	if existing := p.relays[gameID]; existing != nil {
		url := existing.url
		p.relayMu.Unlock()
		return url, nil
	}
	relayPort := p.relayPortBase + int(gameID)%p.relayPortCount
	url := fmt.Sprintf("ws://%s:%d/", p.publicAddr, relayPort)

	args := []string{
		"--relay",
		peerHost,
		strconv.FormatUint(uint64(peerPort), 10),
		strconv.FormatUint(uint64(gameID), 10),
		fmt.Sprintf("--ws-port=%d", relayPort),
	}
	cmd := exec.Command(p.binary, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		p.relayMu.Unlock()
		return "", err
	}
	p.relays[gameID] = &relayChild{cmd: cmd, url: url, port: relayPort}
	p.relayMu.Unlock()

	log.Printf("[proc] spawned relay for game %d on port %d (pid=%d)", gameID, relayPort, cmd.Process.Pid)
	go func() {
		err := cmd.Wait()
		p.relayMu.Lock()
		if r := p.relays[gameID]; r != nil && r.cmd == cmd {
			delete(p.relays, gameID)
		}
		p.relayMu.Unlock()
		if err != nil {
			log.Printf("[proc] relay for game %d exited: %v", gameID, err)
		} else {
			log.Printf("[proc] relay for game %d exited cleanly", gameID)
		}
	}()
	return url, nil
}

// StopRelay kills the relay child for gameID, if any. Called when the
// dedicated server for that game exits (procManager.onExit) so we don't
// leak relay processes after match end.
func (p *procManager) StopRelay(gameID uint32) {
	p.relayMu.Lock()
	r := p.relays[gameID]
	delete(p.relays, gameID)
	p.relayMu.Unlock()
	if r != nil && r.cmd != nil && r.cmd.Process != nil {
		_ = r.cmd.Process.Kill()
	}
}

func (p *procManager) Start(gameID, accountID uint32) error {
	if p.binary == "" {
		return errors.New("-game-binary not configured")
	}
	// Spawned dedicated servers always reach the lobby over loopback —
	// they're the same process tree on the same box. The dedicated-server
	// UDP heartbeat path in the C++ binary uses inet_addr() which only
	// parses dotted-decimal, so passing a hostname (e.g. -public-addr) here
	// would silently misroute heartbeats and the lobby would time out the
	// pending create.
	args := []string{
		"-s",
		"127.0.0.1",
		strconv.Itoa(p.lobbyPort),
		strconv.FormatUint(uint64(gameID), 10),
		strconv.FormatUint(uint64(accountID), 10),
	}
	if p.gamePortBase > 0 {
		gamePort := p.gamePortBase + int(gameID)%p.gamePortCount
		args = append(args, strconv.Itoa(gamePort))
	}
	cmd := exec.Command(p.binary, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		return err
	}
	p.mu.Lock()
	p.procs[gameID] = cmd
	p.mu.Unlock()
	log.Printf("[proc] spawned dedicated server for game %d (pid=%d)", gameID, cmd.Process.Pid)
	go func() {
		err := cmd.Wait()
		p.mu.Lock()
		if p.procs[gameID] == cmd {
			delete(p.procs, gameID)
		}
		p.mu.Unlock()
		if err != nil {
			log.Printf("[proc] dedicated server for game %d exited: %v", gameID, err)
		} else {
			log.Printf("[proc] dedicated server for game %d exited cleanly", gameID)
		}
		if p.onExit != nil {
			p.onExit(gameID)
		}
	}()
	return nil
}

func (p *procManager) Stop(gameID uint32) {
	p.mu.Lock()
	cmd := p.procs[gameID]
	delete(p.procs, gameID)
	p.mu.Unlock()
	if cmd != nil && cmd.Process != nil {
		_ = cmd.Process.Kill()
	}
}

func (p *procManager) StopAll() {
	p.mu.Lock()
	ids := make([]uint32, 0, len(p.procs))
	for id := range p.procs {
		ids = append(ids, id)
	}
	p.mu.Unlock()
	for _, id := range ids {
		p.Stop(id)
	}
	p.relayMu.Lock()
	rids := make([]uint32, 0, len(p.relays))
	for id := range p.relays {
		rids = append(rids, id)
	}
	p.relayMu.Unlock()
	for _, id := range rids {
		p.StopRelay(id)
	}
}
