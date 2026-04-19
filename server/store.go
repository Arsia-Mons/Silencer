package main

import (
	"crypto/sha1"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"sync"
)

type Agency struct {
	Wins          uint16 `json:"w"`
	Losses        uint16 `json:"l"`
	XPToNextLevel uint16 `json:"x"`
	Level         uint8  `json:"lv"`
	Endurance     uint8  `json:"e"`
	Shield        uint8  `json:"s"`
	Jetpack       uint8  `json:"j"`
	TechSlots     uint8  `json:"t"`
	Hacking       uint8  `json:"h"`
	Contacts      uint8  `json:"c"`
}

type User struct {
	AccountID uint32    `json:"id"`
	Name      string    `json:"name"`
	PassHash  string    `json:"pw"` // hex of sha1
	Agency    [5]Agency `json:"a"`
}

type Store struct {
	path    string
	mu      sync.Mutex
	NextID  uint32           `json:"next"`
	ByName  map[string]*User `json:"users"`
	dirty   bool
	saveErr error
}

func NewStore(path string) (*Store, error) {
	s := &Store{
		path:   path,
		NextID: 1,
		ByName: map[string]*User{},
	}
	data, err := os.ReadFile(path)
	if errors.Is(err, os.ErrNotExist) {
		return s, s.save()
	}
	if err != nil {
		return nil, err
	}
	if err := json.Unmarshal(data, s); err != nil {
		return nil, err
	}
	if s.ByName == nil {
		s.ByName = map[string]*User{}
	}
	if s.NextID == 0 {
		s.NextID = 1
	}
	return s, nil
}

func (s *Store) save() error {
	tmp := s.path + ".tmp"
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(s.path), 0o755); err != nil {
		return err
	}
	if err := os.WriteFile(tmp, data, 0o644); err != nil {
		return err
	}
	return os.Rename(tmp, s.path)
}

// Login returns the user, creating it on first auth.
// Returns (user, true) on success, (nil, false) on password mismatch.
func (s *Store) Login(name string, sha1sum []byte) (*User, bool) {
	s.mu.Lock()
	defer s.mu.Unlock()
	hash := hex.EncodeToString(sha1sum)
	u, ok := s.ByName[name]
	if !ok {
		u = &User{
			AccountID: s.NextID,
			Name:      name,
			PassHash:  hash,
		}
		for i := range u.Agency {
			u.Agency[i] = defaultAgency()
		}
		s.ByName[name] = u
		s.NextID++
		_ = s.save()
		return u, true
	}
	if u.PassHash != hash {
		return nil, false
	}
	return u, true
}

func (s *Store) ByAccountID(id uint32) *User {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, u := range s.ByName {
		if u.AccountID == id {
			return u
		}
	}
	return nil
}

func (s *Store) UpdateStats(accountID uint32, agencyIdx uint8, won bool, xpGained uint32) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if agencyIdx >= 5 {
		return
	}
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		a := &u.Agency[agencyIdx]
		if won {
			a.Wins++
		} else {
			a.Losses++
		}
		// simple leveling: bar is 100 * (level+1). rolls over any number of levels.
		x := uint32(a.XPToNextLevel) + xpGained
		for {
			next := uint32(100) * uint32(a.Level+1)
			if x < next || a.Level >= 99 {
				break
			}
			x -= next
			a.Level++
		}
		a.XPToNextLevel = uint16(x)
		_ = s.save()
		return
	}
}

func (s *Store) UpgradeStat(accountID uint32, agencyIdx uint8, stat uint8) bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	if agencyIdx >= 5 {
		return false
	}
	for _, u := range s.ByName {
		if u.AccountID != accountID {
			continue
		}
		a := &u.Agency[agencyIdx]
		const (
			statEndurance = iota
			statShield
			statJetpack
			statTechSlots
			statHacking
			statContacts
		)
		// cap at 5 (8 for techslots) — matches client's User static maxima.
		max := uint8(5)
		if stat == statTechSlots {
			max = 8
		}
		bump := func(p *uint8) bool {
			if *p >= max {
				return false
			}
			*p++
			return true
		}
		var ok bool
		switch stat {
		case statEndurance:
			ok = bump(&a.Endurance)
		case statShield:
			ok = bump(&a.Shield)
		case statJetpack:
			ok = bump(&a.Jetpack)
		case statTechSlots:
			ok = bump(&a.TechSlots)
		case statHacking:
			ok = bump(&a.Hacking)
		case statContacts:
			ok = bump(&a.Contacts)
		}
		if ok {
			_ = s.save()
		}
		return ok
	}
	return false
}

// wireUser converts a store User to the wire-format User for encoding.
func wireUser(u *User) *User { return u }

func hashPassword(plain string) []byte {
	h := sha1.Sum([]byte(plain))
	return h[:]
}

// defaultAgency mirrors src/user.cpp starting values (non-bonus fields only —
// the server-side record tracks purchased upgrades; the client adds its agency
// perks locally).
func defaultAgency() Agency {
	return Agency{TechSlots: 3}
}
