package main

import (
	"encoding/binary"
	"log"
	"net"
)

// serveUDP handles dedicated-server heartbeats:
// [0x00][u32 gameid][u16 port][u8 state][u8 parkedcount][u32 acct]*parkedcount
//         [u32 tickcount][u32 aliveMask]   ← extended fields, issue #23
//
// The parkedcount + accountid list, and the extended fields, are optional for
// forward-compat with older dedicated servers.
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
			var parked []uint32
			off := 8
			if n >= off+1 {
				count := int(buf[off])
				off++
				if n >= off+count*4 {
					parked = make([]uint32, count)
					for i := 0; i < count; i++ {
						parked[i] = binary.LittleEndian.Uint32(buf[off : off+4])
						off += 4
					}
				}
			}
			var tickCount, aliveMask uint32
			if n >= off+4 {
				tickCount = binary.LittleEndian.Uint32(buf[off : off+4])
				off += 4
			}
			if n >= off+4 {
				aliveMask = binary.LittleEndian.Uint32(buf[off : off+4])
			}
			hub.OnHeartbeat(gameID, addr.IP.String(), port, state, parked, tickCount, aliveMask)
		default:
			log.Printf("[udp] unknown opcode 0x%02x from %s", buf[0], addr)
		}
	}
}
