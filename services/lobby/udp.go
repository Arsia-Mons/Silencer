package main

import (
	"encoding/binary"
	"log"
	"net"
)

// serveUDP handles dedicated-server heartbeats:
// [0x00][u32 gameid][u16 port][u8 state]
func serveUDP(conn *net.UDPConn, hub *Hub) {
	buf := make([]byte, 512)
	for {
		n, addr, err := conn.ReadFromUDP(buf)
		if err != nil {
			log.Printf("[udp] read: %v", err)
			return
		}
		if n < 1 {
			continue
		}
		switch buf[0] {
		case 0x00:
			if n < 1+4+2+1 {
				continue
			}
			gameID := binary.LittleEndian.Uint32(buf[1:5])
			port := binary.LittleEndian.Uint16(buf[5:7])
			state := buf[7]
			hub.OnHeartbeat(gameID, addr.IP.String(), port, state)
		default:
			log.Printf("[udp] unknown opcode 0x%02x from %s", buf[0], addr)
		}
	}
}
