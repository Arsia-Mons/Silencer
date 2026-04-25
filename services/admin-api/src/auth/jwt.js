import jwt from 'jsonwebtoken';
import { JWT_SECRET, JWT_EXPIRES_IN } from '../config.js';

export function signToken(payload) {
  return jwt.sign(payload, JWT_SECRET, { expiresIn: JWT_EXPIRES_IN });
}

export function verifyToken(token) {
  return jwt.verify(token, JWT_SECRET);
}

// Middleware: require valid JWT (admin or player)
export function requireAuth(req, res, next) {
  const header = req.headers.authorization || '';
  const token = header.startsWith('Bearer ') ? header.slice(7) : null;
  if (!token) return res.status(401).json({ error: 'No token' });
  try {
    req.user = verifyToken(token);
    next();
  } catch {
    res.status(401).json({ error: 'Invalid token' });
  }
}

// Middleware: require player JWT (type === 'player')
export function requirePlayer(req, res, next) {
  const header = req.headers.authorization || '';
  const token = header.startsWith('Bearer ') ? header.slice(7) : null;
  if (!token) return res.status(401).json({ error: 'No token' });
  try {
    req.player = verifyToken(token);
    if (req.player.type !== 'player') return res.status(403).json({ error: 'Player token required' });
    next();
  } catch {
    res.status(401).json({ error: 'Invalid token' });
  }
}

// Middleware factory: require minimum role level
const ROLE_RANK = { viewer: 0, moderator: 1, manager: 2, admin: 3, superadmin: 4 };

export function requireRole(minRole) {
  return (req, res, next) => {
    if ((ROLE_RANK[req.user?.role] ?? -1) >= ROLE_RANK[minRole]) return next();
    res.status(403).json({ error: 'Insufficient role' });
  };
}
