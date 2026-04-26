/**
 * Actor definition CRUD endpoints.
 *
 * GET    /actors         — list all actor defs (filenames without .json)
 * GET    /actors/:id     — return parsed actor def JSON
 * PUT    /actors/:id     — write actor def JSON to actordefs/:id.json (admin only)
 * DELETE /actors/:id     — delete actor def (admin only)
 */

import { Router } from 'express';
import { readdirSync, readFileSync, writeFileSync, unlinkSync, existsSync, mkdirSync } from 'fs';
import { join } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';

const router = Router();
const ACTORS_DIR = join(ASSETS_DIR, 'actordefs');

function actorPath(id) {
  // Prevent path traversal
  if (!id || /[/\\.]/.test(id)) throw new Error('Invalid actor id');
  return join(ACTORS_DIR, `${id}.json`);
}

function ensureActorsDir() {
  if (!existsSync(ACTORS_DIR)) mkdirSync(ACTORS_DIR, { recursive: true });
}

// GET /actors
router.get('/', requireAuth, (_req, res) => {
  try {
    ensureActorsDir();
    const files = readdirSync(ACTORS_DIR).filter(f => f.endsWith('.json'));
    res.json(files.map(f => f.slice(0, -5)));
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /actors/:id
router.get('/:id', requireAuth, (req, res) => {
  try {
    const path = actorPath(req.params.id);
    if (!existsSync(path)) return res.status(404).json({ error: 'Not found' });
    res.json(JSON.parse(readFileSync(path, 'utf8')));
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// PUT /actors/:id  (admin role required)
router.put('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    ensureActorsDir();
    const path = actorPath(req.params.id);
    const body = req.body;
    if (!body || typeof body !== 'object') {
      return res.status(400).json({ error: 'Body must be a JSON object' });
    }
    writeFileSync(path, JSON.stringify(body, null, 2), 'utf8');
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// DELETE /actors/:id  (admin role required)
router.delete('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    const path = actorPath(req.params.id);
    if (!existsSync(path)) return res.status(404).json({ error: 'Not found' });
    unlinkSync(path);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

export default router;
