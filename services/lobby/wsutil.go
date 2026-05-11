package main

// Minimal RFC 6455 WebSocket implementation for the spectator facade.
//
// We hand-roll instead of pulling in github.com/gorilla/websocket /
// github.com/coder/websocket because the surface we need is small:
//   - text-frame messages only (JSON)
//   - server-side only (server frames are unmasked; client frames carry
//     a 4-byte mask we XOR in place)
//   - close + ping/pong control frames
//
// Not implemented (intentionally): permessage-deflate, fragmented
// messages, binary frames. If a browser ever fragments a JSON command,
// recvFrame returns an error and the connection is closed.

import (
	"bufio"
	"crypto/sha1"
	"encoding/base64"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"strings"
	"sync"
	"time"
)

const (
	wsOpContinuation = 0x0
	wsOpText         = 0x1
	wsOpBinary       = 0x2
	wsOpClose        = 0x8
	wsOpPing         = 0x9
	wsOpPong         = 0xA

	wsMagicGUID  = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
	wsMaxPayload = 1 << 20 // 1 MB cap on a single text frame
)

// wsConn is a server-side WebSocket connection. Reads return decoded
// text payloads; writes serialise text frames. Concurrent writes are
// serialised by wmu; reads are not safe across goroutines.
type wsConn struct {
	c   net.Conn
	br  *bufio.Reader
	wmu sync.Mutex
}

// wsAccept upgrades an HTTP request to WebSocket. Returns the connection
// or writes an HTTP error response and returns nil.
func wsAccept(w http.ResponseWriter, r *http.Request) *wsConn {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return nil
	}
	if !strings.EqualFold(r.Header.Get("Upgrade"), "websocket") {
		http.Error(w, "expected websocket upgrade", http.StatusBadRequest)
		return nil
	}
	if !strings.Contains(strings.ToLower(r.Header.Get("Connection")), "upgrade") {
		http.Error(w, "expected Connection: Upgrade", http.StatusBadRequest)
		return nil
	}
	if r.Header.Get("Sec-WebSocket-Version") != "13" {
		http.Header(w.Header()).Set("Sec-WebSocket-Version", "13")
		http.Error(w, "unsupported websocket version", http.StatusUpgradeRequired)
		return nil
	}
	key := r.Header.Get("Sec-WebSocket-Key")
	if key == "" {
		http.Error(w, "missing Sec-WebSocket-Key", http.StatusBadRequest)
		return nil
	}

	hj, ok := w.(http.Hijacker)
	if !ok {
		http.Error(w, "hijack unsupported", http.StatusInternalServerError)
		return nil
	}
	conn, brw, err := hj.Hijack()
	if err != nil {
		http.Error(w, "hijack failed", http.StatusInternalServerError)
		return nil
	}

	h := sha1.Sum([]byte(key + wsMagicGUID))
	accept := base64.StdEncoding.EncodeToString(h[:])

	resp := "HTTP/1.1 101 Switching Protocols\r\n" +
		"Upgrade: websocket\r\n" +
		"Connection: Upgrade\r\n" +
		"Sec-WebSocket-Accept: " + accept + "\r\n\r\n"
	if _, err := brw.Writer.WriteString(resp); err != nil {
		conn.Close()
		return nil
	}
	if err := brw.Writer.Flush(); err != nil {
		conn.Close()
		return nil
	}
	return &wsConn{c: conn, br: brw.Reader}
}

// recvFrame reads one frame and returns its opcode + payload. Handles
// the unmasking of client→server data. Returns io.EOF when the peer
// sends a close frame or the TCP socket closes.
func (w *wsConn) recvFrame() (opcode byte, payload []byte, err error) {
	hdr := make([]byte, 2)
	if _, err = io.ReadFull(w.br, hdr); err != nil {
		return 0, nil, err
	}
	fin := hdr[0] & 0x80
	op := hdr[0] & 0x0F
	masked := hdr[1] & 0x80
	length := uint64(hdr[1] & 0x7F)

	if fin == 0 {
		return 0, nil, errors.New("fragmented frames not supported")
	}
	if masked == 0 {
		return 0, nil, errors.New("client frame missing mask")
	}

	switch length {
	case 126:
		var buf [2]byte
		if _, err = io.ReadFull(w.br, buf[:]); err != nil {
			return 0, nil, err
		}
		length = uint64(binary.BigEndian.Uint16(buf[:]))
	case 127:
		var buf [8]byte
		if _, err = io.ReadFull(w.br, buf[:]); err != nil {
			return 0, nil, err
		}
		length = binary.BigEndian.Uint64(buf[:])
	}
	if length > wsMaxPayload {
		return 0, nil, fmt.Errorf("frame payload %d exceeds %d", length, wsMaxPayload)
	}

	var mask [4]byte
	if _, err = io.ReadFull(w.br, mask[:]); err != nil {
		return 0, nil, err
	}
	payload = make([]byte, length)
	if _, err = io.ReadFull(w.br, payload); err != nil {
		return 0, nil, err
	}
	for i := range payload {
		payload[i] ^= mask[i%4]
	}
	return op, payload, nil
}

// sendFrame writes one frame with the given opcode and payload.
// Server-to-client frames are unmasked per the RFC.
func (w *wsConn) sendFrame(opcode byte, payload []byte) error {
	w.wmu.Lock()
	defer w.wmu.Unlock()
	hdr := []byte{0x80 | (opcode & 0x0F), 0}
	switch n := len(payload); {
	case n < 126:
		hdr[1] = byte(n)
	case n <= 0xFFFF:
		hdr[1] = 126
		hdr = append(hdr, 0, 0)
		binary.BigEndian.PutUint16(hdr[2:], uint16(n))
	default:
		hdr[1] = 127
		hdr = append(hdr, 0, 0, 0, 0, 0, 0, 0, 0)
		binary.BigEndian.PutUint64(hdr[2:], uint64(n))
	}
	// Set a generous write deadline; a stalled spectator shouldn't
	// hold up the goroutine forever.
	_ = w.c.SetWriteDeadline(time.Now().Add(15 * time.Second))
	if _, err := w.c.Write(hdr); err != nil {
		return err
	}
	if len(payload) > 0 {
		if _, err := w.c.Write(payload); err != nil {
			return err
		}
	}
	return nil
}

// SendText writes a UTF-8 text payload as a single WebSocket frame.
func (w *wsConn) SendText(s []byte) error {
	return w.sendFrame(wsOpText, s)
}

// SendClose sends a close frame and closes the underlying connection.
// Idempotent — multiple calls are safe.
func (w *wsConn) SendClose() error {
	_ = w.sendFrame(wsOpClose, []byte{0x03, 0xE8}) // status 1000 normal
	return w.c.Close()
}

// Recv reads frames until a text message arrives. Control frames
// (ping/close) are handled inline: ping → pong, close → io.EOF.
func (w *wsConn) Recv() ([]byte, error) {
	for {
		op, payload, err := w.recvFrame()
		if err != nil {
			return nil, err
		}
		switch op {
		case wsOpText:
			return payload, nil
		case wsOpBinary:
			return nil, errors.New("binary frames not supported")
		case wsOpClose:
			_ = w.sendFrame(wsOpClose, nil)
			_ = w.c.Close()
			return nil, io.EOF
		case wsOpPing:
			if err := w.sendFrame(wsOpPong, payload); err != nil {
				return nil, err
			}
		case wsOpPong:
			// ignore
		default:
			return nil, fmt.Errorf("unknown opcode %d", op)
		}
	}
}

// Close closes the underlying TCP connection without sending a close
// frame. Use SendClose for a clean shutdown.
func (w *wsConn) Close() error { return w.c.Close() }
