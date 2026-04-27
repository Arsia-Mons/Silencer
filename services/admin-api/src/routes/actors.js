/**
 * Actor definition endpoints.
 *
 * The filesystem is the source of truth. Actor defs are stored as JSON
 * files in shared/assets/actordefs/ and committed to git. The admin UI
 * reads them from disk for visualization; writes go to disk only so that
 * changes flow back through version control.
 *
 * GET    /actors         — list all actor def IDs (public)
 * GET    /actors/:id     — return actor def JSON  (public)
 * PUT    /actors/:id     — write actor def to disk (admin only)
 * DELETE /actors/:id     — delete actor def file   (admin only)
 */

import { Router } from 'express';
import { writeFileSync, unlinkSync, existsSync, mkdirSync } from 'fs';
import { readdirSync, readFileSync } from 'fs';
import { join } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';

const router = Router();
const ACTORS_DIR = join(ASSETS_DIR, 'actordefs');

function validateId(id) {
  if (!id || /[/\\.]/.test(id)) throw new Error('Invalid actor id');
}

function diskPath(id) {
  return join(ACTORS_DIR, `${id}.json`);
}

function ensureActorsDir() {
  if (!existsSync(ACTORS_DIR)) mkdirSync(ACTORS_DIR, { recursive: true });
}

function writeToDisk(id, data) {
  ensureActorsDir();
  writeFileSync(diskPath(id), JSON.stringify(data, null, 2), 'utf8');
}

// GET /actors  — public
router.get('/', (_req, res) => {
  try {
    ensureActorsDir();
    const ids = readdirSync(ACTORS_DIR)
      .filter(f => f.endsWith('.json'))
      .map(f => f.slice(0, -5));
    res.json(ids);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /actors/:id  — public
router.get('/:id', (req, res) => {
  try {
    validateId(req.params.id);
    const path = diskPath(req.params.id);
    if (!existsSync(path)) return res.status(404).json({ error: 'Not found' });
    const data = JSON.parse(readFileSync(path, 'utf8'));
    res.json(data);
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// PUT /actors/:id  (admin only) — writes to disk only; commit to git to ship
router.put('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    validateId(req.params.id);
    const body = req.body;
    if (!body || typeof body !== 'object') {
      return res.status(400).json({ error: 'Body must be a JSON object' });
    }
    writeToDisk(req.params.id, body);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// DELETE /actors/:id  (admin only)
router.delete('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    validateId(req.params.id);
    const path = diskPath(req.params.id);
    if (!existsSync(path)) return res.status(404).json({ error: 'Not found' });
    unlinkSync(path);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

export default router;
