package main

import (
	"encoding/hex"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"time"
)

// StartPlayerAuthServer starts a lightweight HTTP server on addr (e.g. ":15171")
// that the admin-api can call to validate game player credentials.
// The endpoint is not exposed externally — it is only reachable within the Docker network.
func StartPlayerAuthServer(addr string, store *Store) {
	mux := http.NewServeMux()
	mux.HandleFunc("/player-auth", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		body, err := io.ReadAll(io.LimitReader(r.Body, 256))
		if err != nil {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		var req struct {
			Name    string `json:"name"`
			SHA1Hex string `json:"sha1Hex"`
		}
		if err := json.Unmarshal(body, &req); err != nil || req.Name == "" || req.SHA1Hex == "" {
			http.Error(w, "bad request", http.StatusBadRequest)
			return
		}
		sha1bytes, err := hex.DecodeString(req.SHA1Hex)
		if err != nil || len(sha1bytes) != 20 {
			writeJSON(w, map[string]any{"ok": false, "error": "invalid sha1"})
			return
		}
		user, ok := store.Authenticate(req.Name, sha1bytes)
		if !ok {
			writeJSON(w, map[string]any{"ok": false})
			return
		}
		writeJSON(w, map[string]any{
			"ok":        true,
			"accountId": user.AccountID,
			"name":      user.Name,
		})
	})

	srv := &http.Server{
		Addr:         addr,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 5 * time.Second,
	}
	log.Printf("[player-auth] internal HTTP server on %s", addr)
	if err := srv.ListenAndServe(); err != nil {
		log.Fatalf("[player-auth] server failed: %v", err)
	}
}

func writeJSON(w http.ResponseWriter, v any) {
	w.Header().Set("Content-Type", "application/json")
	_ = json.NewEncoder(w).Encode(v)
}
