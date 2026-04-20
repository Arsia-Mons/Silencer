package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"
)

// DiscordBridge relays chat between the game lobby and a Discord channel.
//
// Outgoing (lobby → Discord) uses a webhook POST.
// Incoming (Discord → lobby) polls the channel messages REST endpoint.
type DiscordBridge struct {
	webhookURL string // Discord webhook URL for sending
	botToken   string // Bot token for reading channel messages
	channelID  string // Discord channel ID to read from

	// onMessage is called when a Discord user sends a message.
	onMessage func(author, content string)

	client *http.Client

	mu          sync.Mutex
	lastMsgID   string // tracks the last seen Discord message ID for polling
	selfBotID   string // the bot's own user ID, to skip echo
	webhookID   string // the webhook's ID, extracted from URL
	rateLimited bool
}

// NewDiscordBridge creates a bridge. All three parameters must be non-empty
// for the bridge to be functional. onMessage is called for each incoming
// Discord message that should be relayed to the lobby.
func NewDiscordBridge(webhookURL, botToken, channelID string, onMessage func(author, content string)) *DiscordBridge {
	b := &DiscordBridge{
		webhookURL: webhookURL,
		botToken:   botToken,
		channelID:  channelID,
		onMessage:  onMessage,
		client:     &http.Client{Timeout: 10 * time.Second},
	}
	// Extract webhook ID from URL: https://discord.com/api/webhooks/{id}/{token}
	if u, err := url.Parse(webhookURL); err == nil {
		parts := strings.Split(strings.Trim(u.Path, "/"), "/")
		// Path: api/webhooks/{id}/{token}
		for i, p := range parts {
			if p == "webhooks" && i+1 < len(parts) {
				b.webhookID = parts[i+1]
				break
			}
		}
	}
	return b
}

// Enabled returns true if the bridge has enough configuration to operate.
func (b *DiscordBridge) Enabled() bool {
	return b.webhookURL != "" && b.botToken != "" && b.channelID != ""
}

// SendToDiscord posts a chat message to the Discord channel via webhook.
func (b *DiscordBridge) SendToDiscord(username, content string) {
	if b.webhookURL == "" {
		return
	}
	go b.doWebhookPost(username, content)
}

func (b *DiscordBridge) doWebhookPost(username, content string) {
	payload := webhookPayload{
		Username: username,
		Content:  content,
	}
	body, err := json.Marshal(payload)
	if err != nil {
		log.Printf("[discord] marshal webhook payload: %v", err)
		return
	}

	req, err := http.NewRequest("POST", b.webhookURL+"?wait=false", bytes.NewReader(body))
	if err != nil {
		log.Printf("[discord] create webhook request: %v", err)
		return
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := b.client.Do(req)
	if err != nil {
		log.Printf("[discord] webhook POST: %v", err)
		return
	}
	defer resp.Body.Close()
	io.Copy(io.Discard, resp.Body)

	if resp.StatusCode == http.StatusTooManyRequests {
		log.Printf("[discord] webhook rate-limited")
		return
	}
	if resp.StatusCode >= 300 {
		log.Printf("[discord] webhook returned %d", resp.StatusCode)
	}
}

// Run starts the polling loop for incoming Discord messages.
// It blocks until ctx is cancelled.
func (b *DiscordBridge) Run(ctx context.Context) {
	if b.botToken == "" || b.channelID == "" {
		return
	}

	// Fetch our own bot user ID so we can skip our messages.
	b.fetchSelfID()

	// Seed lastMsgID so we only relay messages posted after startup.
	b.seedLastMessageID(ctx)

	ticker := time.NewTicker(3 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			b.poll(ctx)
		}
	}
}

func (b *DiscordBridge) fetchSelfID() {
	req, err := http.NewRequest("GET", "https://discord.com/api/v10/users/@me", nil)
	if err != nil {
		log.Printf("[discord] create @me request: %v", err)
		return
	}
	req.Header.Set("Authorization", "Bot "+b.botToken)

	resp, err := b.client.Do(req)
	if err != nil {
		log.Printf("[discord] @me request: %v", err)
		return
	}
	defer resp.Body.Close()

	var me struct {
		ID string `json:"id"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&me); err != nil {
		log.Printf("[discord] decode @me: %v", err)
		return
	}
	b.mu.Lock()
	b.selfBotID = me.ID
	b.mu.Unlock()
	log.Printf("[discord] bot user ID: %s", me.ID)
}

func (b *DiscordBridge) seedLastMessageID(ctx context.Context) {
	endpoint := fmt.Sprintf("https://discord.com/api/v10/channels/%s/messages?limit=1", b.channelID)
	req, err := http.NewRequestWithContext(ctx, "GET", endpoint, nil)
	if err != nil {
		return
	}
	req.Header.Set("Authorization", "Bot "+b.botToken)

	resp, err := b.client.Do(req)
	if err != nil {
		log.Printf("[discord] seed request: %v", err)
		return
	}
	defer resp.Body.Close()

	var msgs []discordMessage
	if err := json.NewDecoder(resp.Body).Decode(&msgs); err != nil {
		return
	}
	if len(msgs) > 0 {
		b.mu.Lock()
		b.lastMsgID = msgs[0].ID
		b.mu.Unlock()
		log.Printf("[discord] seeded last message ID: %s", msgs[0].ID)
	}
}

func (b *DiscordBridge) poll(ctx context.Context) {
	b.mu.Lock()
	after := b.lastMsgID
	b.mu.Unlock()

	endpoint := fmt.Sprintf("https://discord.com/api/v10/channels/%s/messages?limit=10", b.channelID)
	if after != "" {
		endpoint += "&after=" + after
	}

	req, err := http.NewRequestWithContext(ctx, "GET", endpoint, nil)
	if err != nil {
		return
	}
	req.Header.Set("Authorization", "Bot "+b.botToken)

	resp, err := b.client.Do(req)
	if err != nil {
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusTooManyRequests {
		log.Printf("[discord] poll rate-limited; backing off")
		return
	}
	if resp.StatusCode != http.StatusOK {
		body, _ := io.ReadAll(io.LimitReader(resp.Body, 256))
		log.Printf("[discord] poll returned %d: %s", resp.StatusCode, body)
		return
	}

	var msgs []discordMessage
	if err := json.NewDecoder(resp.Body).Decode(&msgs); err != nil {
		log.Printf("[discord] decode messages: %v", err)
		return
	}

	if len(msgs) == 0 {
		return
	}

	b.mu.Lock()
	selfID := b.selfBotID
	hookID := b.webhookID
	b.mu.Unlock()

	// Discord returns newest-first; process oldest-first.
	for i := len(msgs) - 1; i >= 0; i-- {
		m := msgs[i]
		if b.shouldSkip(m, selfID, hookID) {
			continue
		}
		content := m.Content
		if content == "" {
			continue
		}
		b.onMessage(m.Author.displayName(), content)
	}

	// Update lastMsgID to the newest message (first in the array).
	b.mu.Lock()
	b.lastMsgID = msgs[0].ID
	b.mu.Unlock()
}

// shouldSkip returns true for messages we should not relay to the lobby.
func (b *DiscordBridge) shouldSkip(m discordMessage, selfID, hookID string) bool {
	// Skip messages from our bot user.
	if selfID != "" && m.Author.ID == selfID {
		return true
	}
	// Skip messages sent by our webhook.
	if hookID != "" && m.WebhookID == hookID {
		return true
	}
	// Skip other bot messages.
	if m.Author.Bot {
		return true
	}
	return false
}

// ---- Discord API types (minimal) ----

type webhookPayload struct {
	Username string `json:"username,omitempty"`
	Content  string `json:"content"`
}

type discordMessage struct {
	ID        string        `json:"id"`
	Content   string        `json:"content"`
	Author    discordAuthor `json:"author"`
	WebhookID string        `json:"webhook_id,omitempty"`
}

type discordAuthor struct {
	ID         string `json:"id"`
	Username   string `json:"username"`
	GlobalName string `json:"global_name,omitempty"`
	Bot        bool   `json:"bot,omitempty"`
}

func (a discordAuthor) displayName() string {
	if a.GlobalName != "" {
		return a.GlobalName
	}
	if a.Username != "" {
		return a.Username
	}
	return "Discord"
}
