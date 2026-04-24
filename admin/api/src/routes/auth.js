import { Router } from 'express';
import crypto from 'crypto';
import AdminUser from '../db/models/AdminUser.js';
import Player from '../db/models/Player.js';
import { signToken, requireAuth, requireRole } from '../auth/jwt.js';
import { LOBBY_PLAYER_AUTH_URL } from '../config.js';

const router = Router();

// Simple in-memory rate limiter: max 10 attempts per IP per 60 s
const rateLimiter = new Map();
function checkRateLimit(ip) {
  const now = Date.now();
  const entry = rateLimiter.get(ip) || { count: 0, reset: now + 60_000 };
  if (now > entry.reset) { entry.count = 0; entry.reset = now + 60_000; }
  entry.count++;
  rateLimiter.set(ip, entry);
  return entry.count <= 10;
}
// Prune old entries every 5 min
setInterval(() => {
  const now = Date.now();
  for (const [k, v] of rateLimiter) if (now > v.reset) rateLimiter.delete(k);
}, 300_000);

// Role rank — must match jwt.js
const ROLE_RANK = { viewer: 0, moderator: 1, manager: 2, admin: 3, superadmin: 4 };
const myRank = (req) => ROLE_RANK[req.user?.role] ?? -1;
const rankOf = (role) => ROLE_RANK[role] ?? -1;

// POST /auth/login
router.post('/login', async (req, res) => {
  try {
    const { username, password } = req.body;
    const user = await AdminUser.findOne({ username });
    if (!user || !(await user.checkPassword(password))) {
      return res.status(401).json({ error: 'Invalid credentials' });
    }
    const token = signToken({ id: user._id, username: user.username, role: user.role });
    res.json({ token, role: user.role, username: user.username });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// POST /auth/player-login — authenticate with game credentials via Go lobby
router.post('/player-login', async (req, res) => {
  const ip = req.headers['x-forwarded-for']?.split(',')[0]?.trim() || req.socket.remoteAddress || 'unknown';
  if (!checkRateLimit(ip)) {
    return res.status(429).json({ error: 'Too many login attempts — try again in 60 seconds' });
  }
  try {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ error: 'username and password required' });

    // Hash password to SHA1 hex (matches game's auth protocol)
    const sha1Hex = crypto.createHash('sha1').update(password).digest('hex');

    // Validate against Go lobby (internal Docker network only)
    const authRes = await fetch(`${LOBBY_PLAYER_AUTH_URL}/player-auth`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: username, sha1Hex }),
    });
    const authData = await authRes.json();
    if (!authData.ok) return res.status(401).json({ error: 'Invalid game credentials' });

    const { accountId, name } = authData;

    // Lazy-create or update the player's MongoDB profile
    await Player.findOneAndUpdate(
      { accountId },
      { $set: { name }, $setOnInsert: { firstSeen: new Date(), agencies: Array.from({ length: 5 }, () => ({})) } },
      { upsert: true }
    );

    const token = signToken({ accountId, name, type: 'player' });
    res.json({ token, accountId, name });
  } catch (e) {
    console.error('[player-login]', e.message);
    res.status(500).json({ error: 'Authentication service unavailable' });
  }
});

// GET /auth/users — admin+ can list all admin accounts
router.get('/users', requireAuth, requireRole('admin'), async (req, res) => {
  const users = await AdminUser.find({}, '_id username role createdBy createdAt').lean();
  res.json(users);
});

// POST /auth/users — create a new admin account
// superadmin may create any role; admin may create manager/moderator/viewer
router.post('/users', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    const { username, password, role } = req.body;
    if (!username || !password || !role) return res.status(400).json({ error: 'username, password and role are required' });
    if (rankOf(role) < 0) return res.status(400).json({ error: 'Invalid role' });
    // Caller must outrank the target role
    if (myRank(req) <= rankOf(role)) return res.status(403).json({ error: 'Cannot create a user with equal or higher role' });
    const passHash = await AdminUser.hashPassword(password);
    const user = await AdminUser.create({ username, passHash, role, createdBy: req.user.username });
    res.status(201).json({ _id: user._id, username: user.username, role: user.role });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

// PATCH /auth/users/:id — update role (and optionally username)
router.patch('/users/:id', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    const target = await AdminUser.findById(req.params.id);
    if (!target) return res.status(404).json({ error: 'User not found' });
    // Caller must outrank both the current role and the new role
    if (myRank(req) <= rankOf(target.role)) return res.status(403).json({ error: 'Cannot edit a user with equal or higher role' });
    if (req.body.role !== undefined) {
      if (rankOf(req.body.role) < 0) return res.status(400).json({ error: 'Invalid role' });
      if (myRank(req) <= rankOf(req.body.role)) return res.status(403).json({ error: 'Cannot assign equal or higher role' });
      target.role = req.body.role;
    }
    if (req.body.username !== undefined) target.username = req.body.username;
    await target.save();
    res.json({ _id: target._id, username: target.username, role: target.role });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

// PATCH /auth/users/:id/password — reset another user's password
router.patch('/users/:id/password', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    const { password } = req.body;
    if (!password || password.length < 6) return res.status(400).json({ error: 'Password must be at least 6 characters' });
    const target = await AdminUser.findById(req.params.id);
    if (!target) return res.status(404).json({ error: 'User not found' });
    if (myRank(req) <= rankOf(target.role)) return res.status(403).json({ error: 'Cannot reset password for equal or higher role' });
    target.passHash = await AdminUser.hashPassword(password);
    await target.save();
    res.json({ ok: true });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

// PATCH /auth/me/password — change own password
router.patch('/me/password', requireAuth, async (req, res) => {
  try {
    const { currentPassword, newPassword } = req.body;
    if (!newPassword || newPassword.length < 6) return res.status(400).json({ error: 'New password must be at least 6 characters' });
    const user = await AdminUser.findById(req.user.id);
    if (!user) return res.status(404).json({ error: 'User not found' });
    if (currentPassword && !(await user.checkPassword(currentPassword))) {
      return res.status(401).json({ error: 'Current password incorrect' });
    }
    user.passHash = await AdminUser.hashPassword(newPassword);
    await user.save();
    res.json({ ok: true });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

// DELETE /auth/users/:id — caller must outrank target
router.delete('/users/:id', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    const target = await AdminUser.findById(req.params.id);
    if (!target) return res.status(404).json({ error: 'User not found' });
    if (myRank(req) <= rankOf(target.role)) return res.status(403).json({ error: 'Cannot delete a user with equal or higher role' });
    await AdminUser.findByIdAndDelete(req.params.id);
    res.json({ ok: true });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

export default router;
