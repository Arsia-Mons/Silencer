package main

import (
	"net"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"
)

func writeTemp(t *testing.T, name, body string) string {
	t.Helper()
	p := filepath.Join(t.TempDir(), name)
	if err := os.WriteFile(p, []byte(body), 0644); err != nil {
		t.Fatalf("write: %v", err)
	}
	return p
}

func TestLoadManifest_Valid(t *testing.T) {
	path := writeTemp(t, "update.json", `{
        "version":        "00024",
        "macos_url":      "https://example.com/mac.zip",
        "macos_sha256":   "0101010101010101010101010101010101010101010101010101010101010101",
        "windows_url":    "https://example.com/win.zip",
        "windows_sha256": "0202020202020202020202020202020202020202020202020202020202020202"
    }`)
	m, err := LoadManifest(path)
	if err != nil {
		t.Fatalf("load: %v", err)
	}
	if m.Version != "00024" {
		t.Errorf("version: %q", m.Version)
	}
	if m.MacOSSHA256[0] != 0x01 || m.MacOSSHA256[31] != 0x01 {
		t.Errorf("macos sha not decoded from hex")
	}
	if m.WindowsSHA256[0] != 0x02 {
		t.Errorf("windows sha not decoded from hex")
	}
}

func TestLoadManifest_Missing(t *testing.T) {
	_, err := LoadManifest("/nonexistent/update.json")
	if err == nil {
		t.Fatal("expected error on missing file")
	}
}

func TestLoadManifest_Malformed(t *testing.T) {
	path := writeTemp(t, "update.json", `not json`)
	_, err := LoadManifest(path)
	if err == nil {
		t.Fatal("expected error on malformed json")
	}
}

func TestLoadManifest_BadSHALength(t *testing.T) {
	path := writeTemp(t, "update.json", `{
        "version":"00024",
        "macos_url":"x","macos_sha256":"aa",
        "windows_url":"y","windows_sha256":"bb"
    }`)
	_, err := LoadManifest(path)
	if err == nil {
		t.Fatal("expected error on short sha")
	}
}

// fakeConn captures send() calls without opening real sockets.
type fakeConn struct {
	mu  sync.Mutex
	out []byte
}

func (f *fakeConn) Read(b []byte) (int, error)         { return 0, nil }
func (f *fakeConn) Write(b []byte) (int, error)        { f.mu.Lock(); defer f.mu.Unlock(); f.out = append(f.out, b...); return len(b), nil }
func (f *fakeConn) Close() error                       { return nil }
func (f *fakeConn) LocalAddr() net.Addr                { return &net.TCPAddr{} }
func (f *fakeConn) RemoteAddr() net.Addr               { return &net.TCPAddr{} }
func (f *fakeConn) SetDeadline(time.Time) error        { return nil }
func (f *fakeConn) SetReadDeadline(time.Time) error    { return nil }
func (f *fakeConn) SetWriteDeadline(time.Time) error   { return nil }

func TestHandleVersion_MismatchWithMacOSManifest(t *testing.T) {
	fc := &fakeConn{}
	c := &Client{conn: fc}

	m := &UpdateManifest{
		Version:     "00024",
		MacOSURL:    "https://example.com/mac.zip",
		MacOSSHA256: [32]byte{0xaa},
	}
	c.handleVersion(VersionRequest{Version: "00023", Platform: PlatformMacOSARM64}, "00024", m)

	// Frame layout on the wire: [len u8][opVersion][success=0][url_len u16][url][sha256]
	fc.mu.Lock()
	defer fc.mu.Unlock()
	if len(fc.out) < 4 {
		t.Fatalf("short output: %v", fc.out)
	}
	// fc.out[0] = frame length
	if fc.out[1] != opVersion {
		t.Fatalf("op: %d", fc.out[1])
	}
	if fc.out[2] != 0 {
		t.Fatalf("success byte: %d", fc.out[2])
	}
	urlLen := int(fc.out[3]) | int(fc.out[4])<<8
	if string(fc.out[5:5+urlLen]) != "https://example.com/mac.zip" {
		t.Fatalf("url: %q", string(fc.out[5:5+urlLen]))
	}
	// Also sanity-check the sha starts with 0xaa.
	if fc.out[5+urlLen] != 0xaa {
		t.Fatalf("sha first byte: %d", fc.out[5+urlLen])
	}
}

func TestHandleVersion_MismatchStaleManifest_Bare(t *testing.T) {
	fc := &fakeConn{}
	c := &Client{conn: fc}

	// Manifest reports older version than the lobby advertises → bare reject.
	m := &UpdateManifest{Version: "00022", MacOSURL: "https://ignored"}
	c.handleVersion(VersionRequest{Version: "00023", Platform: PlatformMacOSARM64}, "00024", m)

	fc.mu.Lock()
	defer fc.mu.Unlock()
	// Expect only: [len=2][opVersion][success=0]
	if len(fc.out) != 3 || fc.out[1] != opVersion || fc.out[2] != 0 {
		t.Fatalf("unexpected: %v", fc.out)
	}
}
