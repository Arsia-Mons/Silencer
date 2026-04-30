package main

import (
	"errors"
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

	onExit func(gameID uint32)

	mu    sync.Mutex
	procs map[uint32]*exec.Cmd
}

func newProcManager(binary string, lobbyPort, gamePortBase, gamePortCount int) *procManager {
	return &procManager{
		binary:        binary,
		lobbyPort:     lobbyPort,
		gamePortBase:  gamePortBase,
		gamePortCount: gamePortCount,
		procs:         map[uint32]*exec.Cmd{},
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
}
