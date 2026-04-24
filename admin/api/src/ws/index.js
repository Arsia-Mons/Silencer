import { Server } from 'socket.io';
import { verifyToken } from '../auth/jwt.js';

let io;
let liveState = { onlinePlayers: 0, activeGames: 0, rabbitmqConnected: false };
const onlinePlayers = new Map(); // accountId → { name, gameId, gameStatus }
const activeGames    = new Map(); // gameId   → { name, mapName, hostname, port, state }

export function initWS(httpServer) {
  io = new Server(httpServer, {
    cors: { origin: '*', methods: ['GET', 'POST'] },
  });

  io.use((socket, next) => {
    const token = socket.handshake.auth?.token;
    if (!token) return next(new Error('No token'));
    try {
      socket.user = verifyToken(token);
      next();
    } catch {
      next(new Error('Invalid token'));
    }
  });

  io.on('connection', (socket) => {
    // Send current snapshot on connect
    socket.emit('snapshot', {
      players: Object.fromEntries(onlinePlayers),
      games:   Object.fromEntries(activeGames),
    });

    // Allow client to re-request snapshot (e.g. after page navigation)
    socket.on('getSnapshot', () => {
      socket.emit('snapshot', {
        players: Object.fromEntries(onlinePlayers),
        games:   Object.fromEntries(activeGames),
      });
    });
  });

  return io;
}

export function broadcast(event, data) {
  io?.emit(event, data);
}

// Called by AMQP consumer to update live state and broadcast
export function handleLobbyEvent(type, data) {
  switch (type) {
    case 'player.login':
      onlinePlayers.set(data.accountId, { name: data.name, gameId: 0, gameStatus: 0 });
      liveState.onlinePlayers = onlinePlayers.size;
      broadcast('player.login', data);
      break;
    case 'player.logout':
      onlinePlayers.delete(data.accountId);
      liveState.onlinePlayers = onlinePlayers.size;
      broadcast('player.logout', data);
      break;
    case 'player.presence':
      if (onlinePlayers.has(data.accountId)) {
        onlinePlayers.get(data.accountId).gameId = data.gameId;
        onlinePlayers.get(data.accountId).gameStatus = data.gameStatus;
      }
      broadcast('player.presence', data);
      break;
    case 'game.created':
      activeGames.set(data.gameId, { name: data.name, mapName: data.mapName, state: 'created', accountId: data.accountId });
      liveState.activeGames = activeGames.size;
      broadcast('game.created', data);
      break;
    case 'game.ready':
      if (activeGames.has(data.gameId)) {
        Object.assign(activeGames.get(data.gameId), { hostname: data.hostname, port: data.port, state: 'ready' });
      }
      liveState.activeGames = activeGames.size;
      broadcast('game.ready', data);
      break;
    case 'game.ended':
      activeGames.delete(data.gameId);
      liveState.activeGames = activeGames.size;
      broadcast('game.ended', data);
      break;
  }
}

export function setRabbitMQStatus(connected) {
  liveState.rabbitmqConnected = connected;
}

export function getLiveState() { return { ...liveState }; }
