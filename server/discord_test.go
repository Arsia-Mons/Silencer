package main

import (
	"encoding/json"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"
)

func TestWebhookPayload(t *testing.T) {
	var got webhookPayload
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != "POST" {
			t.Errorf("method = %s, want POST", r.Method)
		}
		if ct := r.Header.Get("Content-Type"); ct != "application/json" {
			t.Errorf("content-type = %s, want application/json", ct)
		}
		body, _ := io.ReadAll(r.Body)
		if err := json.Unmarshal(body, &got); err != nil {
			t.Errorf("unmarshal: %v", err)
		}
		w.WriteHeader(http.StatusNoContent)
	}))
	defer srv.Close()

	bridge := NewDiscordBridge(srv.URL, "", "", nil)
	bridge.doWebhookPost("TestUser", "hello lobby")

	if got.Username != "TestUser" {
		t.Errorf("username = %q, want %q", got.Username, "TestUser")
	}
	if got.Content != "hello lobby" {
		t.Errorf("content = %q, want %q", got.Content, "hello lobby")
	}
}

func TestPollSkipsSelf(t *testing.T) {
	msgs := []discordMessage{
		{
			ID:      "200",
			Content: "from human",
			Author:  discordAuthor{ID: "user1", Username: "human"},
		},
		{
			ID:        "199",
			Content:   "from webhook",
			Author:    discordAuthor{ID: "hook1", Username: "zSILENCER", Bot: true},
			WebhookID: "hook1",
		},
		{
			ID:      "198",
			Content: "from bot user",
			Author:  discordAuthor{ID: "bot99", Username: "mybot", Bot: true},
		},
	}

	b := &DiscordBridge{
		webhookID: "hook1",
		selfBotID: "bot99",
	}

	for _, m := range msgs {
		skip := b.shouldSkip(m, b.selfBotID, b.webhookID)
		if m.ID == "200" && skip {
			t.Errorf("message %s should NOT be skipped", m.ID)
		}
		if m.ID == "199" && !skip {
			t.Errorf("message %s (webhook) should be skipped", m.ID)
		}
		if m.ID == "198" && !skip {
			t.Errorf("message %s (bot) should be skipped", m.ID)
		}
	}
}

func TestShouldSkip(t *testing.T) {
	tests := []struct {
		name   string
		msg    discordMessage
		selfID string
		hookID string
		want   bool
	}{
		{
			name:   "normal user message",
			msg:    discordMessage{Author: discordAuthor{ID: "u1", Username: "player"}},
			selfID: "bot1",
			hookID: "wh1",
			want:   false,
		},
		{
			name:   "own bot message",
			msg:    discordMessage{Author: discordAuthor{ID: "bot1", Username: "bot"}},
			selfID: "bot1",
			hookID: "wh1",
			want:   true,
		},
		{
			name:   "own webhook message",
			msg:    discordMessage{WebhookID: "wh1", Author: discordAuthor{ID: "wh1", Username: "hook", Bot: true}},
			selfID: "bot1",
			hookID: "wh1",
			want:   true,
		},
		{
			name:   "other bot",
			msg:    discordMessage{Author: discordAuthor{ID: "other", Username: "otherbot", Bot: true}},
			selfID: "bot1",
			hookID: "wh1",
			want:   true,
		},
		{
			name:   "empty author fields",
			msg:    discordMessage{Author: discordAuthor{ID: "u2"}},
			selfID: "bot1",
			hookID: "wh1",
			want:   false,
		},
	}
	b := &DiscordBridge{}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got := b.shouldSkip(tt.msg, tt.selfID, tt.hookID)
			if got != tt.want {
				t.Errorf("shouldSkip() = %v, want %v", got, tt.want)
			}
		})
	}
}

func TestWebhookIDExtraction(t *testing.T) {
	b := NewDiscordBridge(
		"https://discord.com/api/webhooks/123456/abctoken",
		"", "", nil,
	)
	if b.webhookID != "123456" {
		t.Errorf("webhookID = %q, want %q", b.webhookID, "123456")
	}
}

func TestWebhookIDExtraction_Empty(t *testing.T) {
	b := NewDiscordBridge("", "", "", nil)
	if b.webhookID != "" {
		t.Errorf("webhookID = %q, want empty", b.webhookID)
	}
}

func TestEnabled(t *testing.T) {
	tests := []struct {
		webhook, token, channel string
		want                    bool
	}{
		{"https://hook", "tok", "123", true},
		{"", "tok", "123", false},
		{"https://hook", "", "123", false},
		{"https://hook", "tok", "", false},
		{"", "", "", false},
	}
	for _, tt := range tests {
		b := NewDiscordBridge(tt.webhook, tt.token, tt.channel, nil)
		if got := b.Enabled(); got != tt.want {
			t.Errorf("Enabled(%q,%q,%q) = %v, want %v", tt.webhook, tt.token, tt.channel, got, tt.want)
		}
	}
}

func TestAuthorDisplayName(t *testing.T) {
	tests := []struct {
		name   string
		author discordAuthor
		want   string
	}{
		{"global name preferred", discordAuthor{GlobalName: "Cool Name", Username: "user123"}, "Cool Name"},
		{"falls back to username", discordAuthor{Username: "user123"}, "user123"},
		{"falls back to Discord", discordAuthor{}, "Discord"},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := tt.author.displayName(); got != tt.want {
				t.Errorf("displayName() = %q, want %q", got, tt.want)
			}
		})
	}
}

func TestChatFromDiscord(t *testing.T) {
	store, _ := NewStore("/dev/null")
	proc := newProcManager("/bin/false", "127.0.0.1", 517)
	hub := NewHub(store, "", "127.0.0.1", proc)

	// Collect messages sent to a fake client.
	var sent []string
	var mu sync.Mutex
	fakeConn := &fakeNetConn{writeFunc: func(b []byte) (int, error) {
		// Parse the chat frame: [len][opChat][channel cstr][message cstr]
		if len(b) > 2 && b[1] == opChat {
			payload := b[2:] // skip length byte and opChat
			parts := strings.SplitN(string(payload), "\x00", 3)
			if len(parts) >= 2 {
				mu.Lock()
				sent = append(sent, parts[1]) // message part
				mu.Unlock()
			}
		}
		return len(b), nil
	}}

	c := &Client{
		conn:    fakeConn,
		hub:     hub,
		channel: "Lobby",
	}
	hub.mu.Lock()
	hub.clients[c] = struct{}{}
	hub.mu.Unlock()

	hub.ChatFromDiscord("TestDiscordUser", "hello from discord")

	mu.Lock()
	defer mu.Unlock()
	if len(sent) == 0 {
		t.Fatal("expected at least one message sent to client")
	}
	found := false
	for _, s := range sent {
		if strings.Contains(s, "[D] TestDiscordUser: hello from discord") {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected Discord message in sent; got %v", sent)
	}
}

// fakeNetConn implements net.Conn for testing sendChat.
type fakeNetConn struct {
	writeFunc func([]byte) (int, error)
}

func (f *fakeNetConn) Read(b []byte) (int, error)  { return 0, io.EOF }
func (f *fakeNetConn) Write(b []byte) (int, error)  { return f.writeFunc(b) }
func (f *fakeNetConn) Close() error                  { return nil }
func (f *fakeNetConn) LocalAddr() net.Addr            { return &net.TCPAddr{} }
func (f *fakeNetConn) RemoteAddr() net.Addr           { return &net.TCPAddr{} }
func (f *fakeNetConn) SetDeadline(t time.Time) error      { return nil }
func (f *fakeNetConn) SetReadDeadline(t time.Time) error   { return nil }
func (f *fakeNetConn) SetWriteDeadline(t time.Time) error  { return nil }
