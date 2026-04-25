package main

import (
	"crypto/sha1"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

const maxMapUploadSize = 65535 // matches engine map-data buffer (world.cpp AllocateMapData)

// validMapFilename: basename-only, alphanumeric + underscore + hyphen + dot, .sil extension,
// max 60 chars to stay inside the LobbyGame.mapname wire field.
var validMapFilename = regexp.MustCompile(`(?i)^[a-z0-9_\-]{1,55}\.sil$`)

// MapMeta holds the metadata stored alongside each uploaded map.
type MapMeta struct {
	SHA1       string `json:"sha1"`
	Name       string `json:"name"`
	Size       int    `json:"size"`
	Author     string `json:"author"`
	UploadedAt string `json:"uploaded_at"`
}

// MapStore manages community maps stored as files in a directory.
type MapStore struct {
	mu     sync.RWMutex
	dir    string
	apiKey string
	bySHA1 map[string]*MapMeta // lowercase sha1hex → meta
	byName map[string]*MapMeta // uppercase name → meta (last-upload wins)
}

func NewMapStore(dir, apiKey string) (*MapStore, error) {
	if err := os.MkdirAll(dir, 0755); err != nil {
		return nil, fmt.Errorf("create maps dir: %w", err)
	}
	s := &MapStore{
		dir:    dir,
		apiKey: apiKey,
		bySHA1: make(map[string]*MapMeta),
		byName: make(map[string]*MapMeta),
	}
	s.loadIndex()
	return s, nil
}

func (s *MapStore) indexPath() string        { return filepath.Join(s.dir, "index.json") }
func (s *MapStore) mapPath(sha1hex string) string { return filepath.Join(s.dir, sha1hex+".sil") }

func (s *MapStore) loadIndex() {
	b, err := os.ReadFile(s.indexPath())
	if err != nil {
		return
	}
	var metas []*MapMeta
	if err := json.Unmarshal(b, &metas); err != nil {
		log.Printf("[map-api] warn: failed to parse index.json: %v", err)
		return
	}
	for _, m := range metas {
		s.bySHA1[strings.ToLower(m.SHA1)] = m
		s.byName[strings.ToUpper(m.Name)] = m
	}
	log.Printf("[map-api] loaded %d community map(s)", len(metas))
}

func (s *MapStore) saveIndex() {
	metas := make([]*MapMeta, 0, len(s.bySHA1))
	for _, m := range s.bySHA1 {
		metas = append(metas, m)
	}
	b, err := json.Marshal(metas)
	if err != nil {
		return
	}
	tmp := s.indexPath() + ".tmp"
	_ = os.WriteFile(tmp, b, 0644)
	_ = os.Rename(tmp, s.indexPath())
}

// Upload stores a map and returns its metadata. Duplicate SHA-1s are no-ops
// (file already stored); a new name pointing to an existing SHA-1 is allowed.
func (s *MapStore) Upload(name, author string, data []byte) (*MapMeta, error) {
	if !validMapFilename.MatchString(name) {
		return nil, fmt.Errorf("invalid filename (must be ≤60 chars, alphanumeric/underscore/hyphen, .sil extension)")
	}
	if len(data) > maxMapUploadSize {
		return nil, fmt.Errorf("map too large: %d bytes (max %d)", len(data), maxMapUploadSize)
	}
	if len(data) == 0 {
		return nil, fmt.Errorf("empty map file")
	}

	h := sha1.Sum(data)
	sha1hex := hex.EncodeToString(h[:])

	s.mu.Lock()
	defer s.mu.Unlock()

	// Write data file only if not already present.
	path := s.mapPath(sha1hex)
	if _, err := os.Stat(path); os.IsNotExist(err) {
		if err := os.WriteFile(path, data, 0644); err != nil {
			return nil, fmt.Errorf("write map file: %w", err)
		}
	}

	meta := &MapMeta{
		SHA1:       sha1hex,
		Name:       name,
		Size:       len(data),
		Author:     author,
		UploadedAt: time.Now().UTC().Format(time.RFC3339),
	}
	s.bySHA1[sha1hex] = meta
	s.byName[strings.ToUpper(name)] = meta
	s.saveIndex()
	return meta, nil
}

func (s *MapStore) GetBySHA1(sha1hex string) ([]byte, *MapMeta, error) {
	s.mu.RLock()
	meta := s.bySHA1[strings.ToLower(sha1hex)]
	s.mu.RUnlock()
	if meta == nil {
		return nil, nil, nil
	}
	data, err := os.ReadFile(s.mapPath(meta.SHA1))
	return data, meta, err
}

func (s *MapStore) GetByName(name string) ([]byte, *MapMeta, error) {
	s.mu.RLock()
	meta := s.byName[strings.ToUpper(name)]
	s.mu.RUnlock()
	if meta == nil {
		return nil, nil, nil
	}
	data, err := os.ReadFile(s.mapPath(meta.SHA1))
	return data, meta, err
}

func (s *MapStore) List() []*MapMeta {
	s.mu.RLock()
	defer s.mu.RUnlock()
	out := make([]*MapMeta, 0, len(s.bySHA1))
	for _, m := range s.bySHA1 {
		out = append(out, m)
	}
	return out
}

func setCORSHeaders(w http.ResponseWriter, origin string) {
	if origin == "" {
		return
	}
	// Allow file:// (origin "null") and any remote origin.
	allow := origin
	if allow == "null" {
		allow = "*"
	}
	w.Header().Set("Access-Control-Allow-Origin", allow)
	w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type, X-Filename, X-Author, X-Api-Key")
	w.Header().Set("Access-Control-Max-Age", "86400")
}

// StartMapAPIServer runs the public HTTP map API on addr.
//
//	GET  /api/maps                  → JSON array of MapMeta
//	POST /api/maps                  → upload (body=raw .SIL, X-Filename, X-Author, X-Api-Key)
//	GET  /api/maps/by-sha1/{sha1}   → raw .SIL bytes looked up by SHA-1 (used by game client)
//	GET  /api/maps/{name}           → raw .SIL bytes looked up by filename
func StartMapAPIServer(addr string, ms *MapStore) {
	mux := http.NewServeMux()

	// /api/maps — list and upload
	mux.HandleFunc("/api/maps", func(w http.ResponseWriter, r *http.Request) {
		setCORSHeaders(w, r.Header.Get("Origin"))
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		switch r.Method {
		case http.MethodGet:
			maps := ms.List()
			w.Header().Set("Content-Type", "application/json")
			_ = json.NewEncoder(w).Encode(maps)

		case http.MethodPost:
			if ms.apiKey != "" {
				key := r.Header.Get("X-Api-Key")
				if key == "" {
					key = r.URL.Query().Get("key")
				}
				if key != ms.apiKey {
					http.Error(w, "unauthorized", http.StatusUnauthorized)
					return
				}
			}
			name := filepath.Base(r.Header.Get("X-Filename"))
			author := strings.TrimSpace(r.Header.Get("X-Author"))
			if author == "" {
				author = "anonymous"
			}
			data, err := io.ReadAll(io.LimitReader(r.Body, int64(maxMapUploadSize)+1))
			if err != nil {
				http.Error(w, "read error", http.StatusBadRequest)
				return
			}
			meta, err := ms.Upload(name, author, data)
			if err != nil {
				http.Error(w, err.Error(), http.StatusBadRequest)
				return
			}
			log.Printf("[map-api] upload: %s by %s sha1=%s size=%d", name, author, meta.SHA1[:8], meta.Size)
			w.Header().Set("Content-Type", "application/json")
			w.WriteHeader(http.StatusCreated)
			_ = json.NewEncoder(w).Encode(meta)

		default:
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		}
	})

	// /api/maps/ — download by SHA-1 or by name
	mux.HandleFunc("/api/maps/", func(w http.ResponseWriter, r *http.Request) {
		setCORSHeaders(w, r.Header.Get("Origin"))
		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		if r.Method != http.MethodGet {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}

		sub := strings.TrimPrefix(r.URL.Path, "/api/maps/")

		// /api/maps/by-sha1/{sha1hex}
		if strings.HasPrefix(sub, "by-sha1/") {
			sha1hex := strings.TrimPrefix(sub, "by-sha1/")
			if len(sha1hex) != 40 {
				http.Error(w, "sha1 must be 40 hex chars", http.StatusBadRequest)
				return
			}
			data, meta, err := ms.GetBySHA1(sha1hex)
			if err != nil {
				http.Error(w, "read error", http.StatusInternalServerError)
				return
			}
			if data == nil {
				http.NotFound(w, r)
				return
			}
			w.Header().Set("Content-Type", "application/octet-stream")
			w.Header().Set("Content-Disposition", fmt.Sprintf(`attachment; filename="%s"`, meta.Name))
			_, _ = w.Write(data)
			return
		}

		// /api/maps/{name}
		name := filepath.Base(sub)
		data, _, err := ms.GetByName(name)
		if err != nil {
			http.Error(w, "read error", http.StatusInternalServerError)
			return
		}
		if data == nil {
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "application/octet-stream")
		w.Header().Set("Content-Disposition", fmt.Sprintf(`attachment; filename="%s"`, name))
		_, _ = w.Write(data)
	})

	srv := &http.Server{
		Addr:         addr,
		Handler:      mux,
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
	}
	log.Printf("[map-api] public HTTP server on %s", addr)
	if err := srv.ListenAndServe(); err != nil && err != http.ErrServerClosed {
		log.Fatalf("[map-api] server failed: %v", err)
	}
}
