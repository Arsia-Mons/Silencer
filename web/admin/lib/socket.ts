'use client';
import { useEffect, useRef, useState } from 'react';
import { io, Socket } from 'socket.io-client';

// Production: NEXT_PUBLIC_WS_URL is unset → io() connects to the current
//   origin (admin.arsiamons.com), and Cloudflare Tunnel routes /socket.io/*
//   to admin-api:24080.
// Local dev: NEXT_PUBLIC_WS_URL=ws://localhost:24080 (compose build arg).
const WS_URL = process.env.NEXT_PUBLIC_WS_URL || '';

let singleton: ReturnType<typeof io> | null = null;

export function getSocket(): ReturnType<typeof io> {
  const token = typeof window !== 'undefined' ? localStorage.getItem('zs_token') : null;

  // If socket exists but was created with a different (or missing) token, destroy and recreate
  if (singleton) {
    const currentToken = (singleton.auth as Record<string, unknown>)?.token;
    if (currentToken !== token) {
      singleton.disconnect();
      singleton = null;
    } else {
      return singleton;
    }
  }

  const opts = {
    auth: { token },
    transports: ['websocket'],
    autoConnect: true,
    reconnectionAttempts: Infinity,
    reconnectionDelay: 2000,
  };
  singleton = WS_URL ? io(WS_URL, opts) : io(opts);
  return singleton;
}

export function useSocket(events: Record<string, (...args: unknown[]) => void>): boolean {
  const [connected, setConnected] = useState(false);
  const cbRef = useRef(events);
  cbRef.current = events;

  useEffect(() => {
    const s = getSocket();
    const onConnect    = () => { setConnected(true); s.emit('getSnapshot'); };
    const onDisconnect = () => setConnected(false);
    s.on('connect', onConnect);
    s.on('disconnect', onDisconnect);

    // Already connected — request snapshot immediately
    if (s.connected) {
      setConnected(true);
      s.emit('getSnapshot');
    }

    const handlers = Object.entries(cbRef.current || {});
    handlers.forEach(([ev, fn]) => s.on(ev, fn as (...args: unknown[]) => void));
    return () => {
      s.off('connect', onConnect);
      s.off('disconnect', onDisconnect);
      handlers.forEach(([ev, fn]) => s.off(ev, fn as (...args: unknown[]) => void));
    };
  }, []);

  return connected;
}

/** Lightweight hook — just the connection status, no event subscriptions. */
export function useWsConnected(): boolean {
  return useSocket({});
}
