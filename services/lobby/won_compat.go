package main

// won_compat.go — WON.net-compatible HTTP endpoints for WONDLL.dll stubs.
//
// The replacement WONDLL.dll (clients/wondll/) makes HTTP calls to these
// endpoints instead of the original WON.net servers. They share the same
// internal HTTP server as the player-auth API (playerauth.go), started by
// StartPlayerAuthServer on the -player-auth-addr port (default :15171).
//
// Endpoints:
//   POST /won/login           — authenticate existing account
//   POST /won/create-account  — register + authenticate new account
//   GET  /won/profile/:name   — fetch player profile fields
//   POST /won/profile/:name   — set one profile field (requires token)

import (
	"crypto/sha1"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"strings"
)

// RegisterWONRoutes adds all /won/* handlers to the given ServeMux.
// Called from StartPlayerAuthServer so they share the same port.
func RegisterWONRoutes(mux *http.ServeMux, store *Store) {
	mux.HandleFunc("/won/login", wonLogin(store))
	mux.HandleFunc("/won/create-account", wonCreateAccount(store))
	mux.HandleFunc("/won/profile/", wonProfile(store))
	log.Printf("[won-compat] routes registered: /won/login /won/create-account /won/profile/")
}

// -------------------------------------------------------------------------
// POST /won/login
// Body: {"name":"<callsign>","password":"<plaintext>"}
// 200:  {"ok":true,"token":"<name>"}
// 401:  {"ok":false,"error":"bad credentials"}
// -------------------------------------------------------------------------

func wonLogin(store *Store) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		var req struct {
			Name     string `json:"name"`
			Password string `json:"password"`
		}
		body, _ := io.ReadAll(io.LimitReader(r.Body, 512))
		if err := json.Unmarshal(body, &req); err != nil || req.Name == "" {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}

		sha1sum := sha1.Sum([]byte(req.Password))
		user, ok := store.Authenticate(req.Name, sha1sum[:])
		if !ok {
			w.WriteHeader(http.StatusUnauthorized)
			writeJSON(w, map[string]any{"ok": false, "error": "bad credentials"})
			return
		}

		writeJSON(w, map[string]any{
			"ok":        true,
			"token":     user.Name, // simple name-as-token; DLL uses it for profile calls
			"accountId": user.AccountID,
			"name":      user.Name,
		})
	}
}

// -------------------------------------------------------------------------
// POST /won/create-account
// Body: {"name":"<callsign>","password":"<plaintext>"}
// 201:  {"ok":true,"token":"<name>","accountId":<n>}
// 409:  {"ok":false,"error":"name taken"}
// -------------------------------------------------------------------------

func wonCreateAccount(store *Store) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		var req struct {
			Name     string `json:"name"`
			Password string `json:"password"`
		}
		body, _ := io.ReadAll(io.LimitReader(r.Body, 512))
		if err := json.Unmarshal(body, &req); err != nil || req.Name == "" || req.Password == "" {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}

		sha1sum := sha1.Sum([]byte(req.Password))
		user, err := store.CreateUser(req.Name, sha1sum[:])
		if err != nil {
			w.WriteHeader(http.StatusConflict)
			writeJSON(w, map[string]any{"ok": false, "error": err.Error()})
			return
		}

		w.WriteHeader(http.StatusCreated)
		writeJSON(w, map[string]any{
			"ok":        true,
			"token":     user.Name,
			"accountId": user.AccountID,
			"name":      user.Name,
		})
	}
}

// -------------------------------------------------------------------------
// GET  /won/profile/<name>     — fetch profile
// POST /won/profile/<name>     — set one field
//
// Profile fields exposed to the original client:
//   level, credits, missions, victories, forfeits, repute
//   (repute is derived from victories/forfeits, not stored separately)
// -------------------------------------------------------------------------

var repueTiers = []string{
	"Just Kill Me",
	"And you are?",
	"Ignored",
	"Still Ignored",
	"Misunderstood",
	"Noticed",
	"Respected",
	"Very Favorable",
	"Well Respected",
	"Feared",
	"God-like",
}

func computeRepute(victories, forfeits int) string {
	score := victories - forfeits/2
	idx := score / 5
	if idx < 0 {
		idx = 0
	}
	if idx >= len(repueTiers) {
		idx = len(repueTiers) - 1
	}
	return repueTiers[idx]
}

func wonProfile(store *Store) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		// Extract name from /won/profile/<name>
		name := strings.TrimPrefix(r.URL.Path, "/won/profile/")
		if name == "" {
			http.Error(w, "missing name", http.StatusBadRequest)
			return
		}

		store.mu.Lock()
		user := store.ByName[strings.ToLower(name)]
		store.mu.Unlock()

		if user == nil {
			http.Error(w, "not found", http.StatusNotFound)
			return
		}

		switch r.Method {

		case http.MethodGet:
			// Aggregate stats across all characters for the profile display.
			var missions, victories, forfeits int
			level := 1
			credits := 0
			for _, ch := range user.Characters {
				missions += int(ch.Stats.Wins) + int(ch.Stats.Losses)
				victories += int(ch.Stats.Wins)
				forfeits += int(ch.Stats.Losses)
				if int(ch.Stats.Level) > level {
					level = int(ch.Stats.Level)
				}
			}

			writeJSON(w, map[string]any{
				"name":      user.Name,
				"level":     level,
				"credits":   credits,
				"missions":  missions,
				"victories": victories,
				"forfeits":  forfeits,
				"repute":    computeRepute(victories, forfeits),
			})

		case http.MethodPost:
			// {"key":"...", "value":"...", "token":"<name>"}
			var req struct {
				Key   string `json:"key"`
				Value string `json:"value"`
				Token string `json:"token"`
			}
			body, _ := io.ReadAll(io.LimitReader(r.Body, 256))
			if err := json.Unmarshal(body, &req); err != nil || req.Key == "" {
				http.Error(w, "bad request", http.StatusBadRequest)
				return
			}
			// Simple token check: token == name (set by DLL on login)
			if !strings.EqualFold(req.Token, user.Name) {
				http.Error(w, "unauthorized", http.StatusUnauthorized)
				return
			}
			// Profile fields are read-only from the server side in our model
			// (stats come from match results). Log but don't reject.
			log.Printf("[won-compat] profile set %s.%s = %q", name, req.Key, req.Value)
			writeJSON(w, map[string]any{"ok": true})

		default:
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		}
	}
}
