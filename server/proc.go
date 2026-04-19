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
	lobbyHost string
	lobbyPort int

	mu    sync.Mutex
	procs map[uint32]*exec.Cmd
}

func newProcManager(binary, lobbyHost string, lobbyPort int) *procManager {
	return &procManager{
		binary:    binary,
		lobbyHost: lobbyHost,
		lobbyPort: lobbyPort,
		procs:     map[uint32]*exec.Cmd{},
	}
}

func (p *procManager) Start(gameID, accountID uint32) error {
	if p.binary == "" {
		return errors.New("-game-binary not configured")
	}
	cmd := exec.Command(p.binary,
		"-s",
		p.lobbyHost,
		strconv.Itoa(p.lobbyPort),
		strconv.FormatUint(uint64(gameID), 10),
		strconv.FormatUint(uint64(accountID), 10),
	)
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
