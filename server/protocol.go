package main

import (
	"encoding/binary"
	"errors"
	"io"
)

const (
	opAuth          = 0
	opMOTD          = 1
	opChat          = 2
	opNewGame       = 3
	opDelGame       = 4
	opChannel       = 5
	opConnect       = 6
	opVersion       = 7
	opUserInfo      = 8
	opPing          = 9
	opUpgradeStat   = 10
	opRegisterStats = 11
	opPresence      = 12
	opSetGame       = 13
)

const maxFrame = 255
const maxUpdateURL = 200 // leaves room for [framelen][op][success][urllen u16][sha256] in a 255-byte frame

func readFrame(r io.Reader) ([]byte, error) {
	var sz [1]byte
	if _, err := io.ReadFull(r, sz[:]); err != nil {
		return nil, err
	}
	n := int(sz[0])
	if n == 0 {
		return nil, errors.New("zero-length frame")
	}
	buf := make([]byte, n)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

func writeFrame(w io.Writer, payload []byte) error {
	if len(payload) == 0 || len(payload) > maxFrame {
		return errors.New("bad frame size")
	}
	buf := make([]byte, 1+len(payload))
	buf[0] = byte(len(payload))
	copy(buf[1:], payload)
	_, err := w.Write(buf)
	return err
}

// bit-aligned little-endian reader matching the client's Serializer.
// All lobby fields are byte-aligned so we can treat them as raw LE.

type reader struct {
	b   []byte
	off int
}

func newReader(b []byte) *reader { return &reader{b: b} }

func (r *reader) u8() (uint8, error) {
	if r.off >= len(r.b) {
		return 0, io.ErrUnexpectedEOF
	}
	v := r.b[r.off]
	r.off++
	return v, nil
}

func (r *reader) u16() (uint16, error) {
	if r.off+2 > len(r.b) {
		return 0, io.ErrUnexpectedEOF
	}
	v := binary.LittleEndian.Uint16(r.b[r.off:])
	r.off += 2
	return v, nil
}

func (r *reader) u32() (uint32, error) {
	if r.off+4 > len(r.b) {
		return 0, io.ErrUnexpectedEOF
	}
	v := binary.LittleEndian.Uint32(r.b[r.off:])
	r.off += 4
	return v, nil
}

func (r *reader) bytes(n int) ([]byte, error) {
	if r.off+n > len(r.b) {
		return nil, io.ErrUnexpectedEOF
	}
	v := r.b[r.off : r.off+n]
	r.off += n
	return v, nil
}

func (r *reader) cstr(max int) (string, error) {
	for i := r.off; i < len(r.b) && i-r.off < max; i++ {
		if r.b[i] == 0 {
			s := string(r.b[r.off:i])
			r.off = i + 1
			return s, nil
		}
	}
	return "", errors.New("unterminated string")
}

func (r *reader) lenBytes() (string, error) {
	n, err := r.u8()
	if err != nil {
		return "", err
	}
	b, err := r.bytes(int(n))
	if err != nil {
		return "", err
	}
	return string(b), nil
}

type writer struct{ b []byte }

func (w *writer) u8(v uint8)   { w.b = append(w.b, v) }
func (w *writer) u16(v uint16) { w.b = binary.LittleEndian.AppendUint16(w.b, v) }
func (w *writer) u32(v uint32) { w.b = binary.LittleEndian.AppendUint32(w.b, v) }
func (w *writer) raw(b []byte) { w.b = append(w.b, b...) }
func (w *writer) cstr(s string) {
	w.b = append(w.b, []byte(s)...)
	w.b = append(w.b, 0)
}
func (w *writer) lenStr(s string) {
	if len(s) > 255 {
		s = s[:255]
	}
	w.b = append(w.b, byte(len(s)))
	w.b = append(w.b, []byte(s)...)
}

// LobbyGame wire layout (matches src/lobbygame.cpp::Serialize).
type LobbyGame struct {
	ID            uint32
	AccountID     uint32
	Name          string
	Password      string
	Hostname      string // "ip,port"
	MapName       string
	MapHash       [20]byte
	Players       uint8
	State         uint8
	SecurityLevel uint8
	MinLevel      uint8
	MaxLevel      uint8
	MaxPlayers    uint8
	MaxTeams      uint8
	Extra         uint8
	Port          uint16
}

func (g *LobbyGame) Decode(r *reader) error {
	var err error
	if g.ID, err = r.u32(); err != nil {
		return err
	}
	if g.AccountID, err = r.u32(); err != nil {
		return err
	}
	if g.Name, err = r.lenBytes(); err != nil {
		return err
	}
	if g.Password, err = r.lenBytes(); err != nil {
		return err
	}
	if g.Hostname, err = r.lenBytes(); err != nil {
		return err
	}
	if g.MapName, err = r.lenBytes(); err != nil {
		return err
	}
	h, err := r.bytes(20)
	if err != nil {
		return err
	}
	copy(g.MapHash[:], h)
	if g.Players, err = r.u8(); err != nil {
		return err
	}
	if g.State, err = r.u8(); err != nil {
		return err
	}
	if g.SecurityLevel, err = r.u8(); err != nil {
		return err
	}
	if g.MinLevel, err = r.u8(); err != nil {
		return err
	}
	if g.MaxLevel, err = r.u8(); err != nil {
		return err
	}
	if g.MaxPlayers, err = r.u8(); err != nil {
		return err
	}
	if g.MaxTeams, err = r.u8(); err != nil {
		return err
	}
	if g.Extra, err = r.u8(); err != nil {
		return err
	}
	if g.Port, err = r.u16(); err != nil {
		return err
	}
	return nil
}

func (g *LobbyGame) Encode(w *writer) {
	w.u32(g.ID)
	w.u32(g.AccountID)
	w.lenStr(g.Name)
	w.lenStr(g.Password)
	w.lenStr(g.Hostname)
	w.lenStr(g.MapName)
	w.raw(g.MapHash[:])
	w.u8(g.Players)
	w.u8(g.State)
	w.u8(g.SecurityLevel)
	w.u8(g.MinLevel)
	w.u8(g.MaxLevel)
	w.u8(g.MaxPlayers)
	w.u8(g.MaxTeams)
	w.u8(g.Extra)
	w.u16(g.Port)
}

// User wire layout (matches src/user.cpp::Serialize).
func encodeUser(w *writer, u *User) {
	w.u32(u.AccountID)
	for i := 0; i < 5; i++ {
		a := &u.Agency[i]
		w.u16(a.Wins)
		w.u16(a.Losses)
		w.u16(a.XPToNextLevel)
		w.u8(a.Level)
		w.u8(a.Endurance)
		w.u8(a.Shield)
		w.u8(a.Jetpack)
		w.u8(a.TechSlots)
		w.u8(a.Hacking)
		w.u8(a.Contacts)
	}
	w.lenStr(u.Name)
}

// Platform byte appended to MSG_VERSION request by updater-capable clients.
// Absent from pre-updater clients.
type Platform uint8

const (
	PlatformUnknown    Platform = 0
	PlatformMacOSARM64 Platform = 1
	PlatformWindowsX64 Platform = 2
)

type VersionRequest struct {
	Version  string
	Platform Platform
}

type VersionReply struct {
	OK     bool
	URL    string   // empty unless reject + manifest available
	SHA256 [32]byte // zero unless URL non-empty
}

// decodeVersionRequest parses an opVersion payload (the opcode byte has
// already been consumed by the dispatcher). Payload layout:
//
//	[version cstring][optional platform u8]
//
// Pre-updater clients omit the platform byte; we treat that as
// PlatformUnknown for graceful handling.
func decodeVersionRequest(payload []byte) (VersionRequest, error) {
	r := newReader(payload)
	ver, err := r.cstr(64)
	if err != nil {
		return VersionRequest{}, err
	}
	req := VersionRequest{Version: ver, Platform: PlatformUnknown}
	if r.off < len(r.b) {
		p, err := r.u8()
		if err != nil {
			return VersionRequest{}, err
		}
		req.Platform = Platform(p)
	}
	return req, nil
}

// encodeVersionReply produces the payload to send back (without the
// leading opcode byte — callers prepend opVersion).
//
//	[success u8]
//	if !success AND URL != "":
//	  [url_len u16 LE][url bytes][sha256 32 bytes]
func encodeVersionReply(rep VersionReply) []byte {
	w := &writer{}
	if rep.OK {
		w.u8(1)
		return w.b
	}
	w.u8(0)
	if rep.URL != "" {
		if len(rep.URL) > maxUpdateURL {
			// This is a manifest misconfiguration, not a runtime condition —
			// fail loudly so the operator catches it.
			panic("encodeVersionReply: URL exceeds maxUpdateURL")
		}
		w.u16(uint16(len(rep.URL)))
		w.raw([]byte(rep.URL))
		w.raw(rep.SHA256[:])
	}
	return w.b
}
