/**
 * Actor definition CRUD endpoints.
 *
 * MongoDB is the source of truth. Every write also syncs to disk so the C++
 * game server can load actordefs from the filesystem without any changes.
 *
 * GET    /actors         — list all actor defs
 * GET    /actors/:id     — return parsed actor def JSON
 * PUT    /actors/:id     — upsert actor def (admin only)
 * DELETE /actors/:id     — delete actor def (admin only)
 */

import { Router } from 'express';
import { writeFileSync, unlinkSync, existsSync, mkdirSync } from 'fs';
import { readdirSync, readFileSync } from 'fs';
import { join } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';
import ActorDef from '../db/models/ActorDef.js';

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

/**
 * Seed MongoDB from disk files on first run. Skips any actor already in Mongo.
 */
export async function seedActorDefs() {
  ensureActorsDir();
  const files = readdirSync(ACTORS_DIR).filter(f => f.endsWith('.json'));
  for (const file of files) {
    const id = file.slice(0, -5);
    const exists = await ActorDef.exists({ _id: id });
    if (!exists) {
      const data = JSON.parse(readFileSync(diskPath(id), 'utf8'));
      await ActorDef.create({ _id: id, data });
      console.log(`[actordefs] seeded "${id}" from disk`);
    }
  }
}

// GET /actors
router.get('/', requireAuth, async (_req, res) => {
  try {
    const docs = await ActorDef.find({}, '_id').lean();
    res.json(docs.map(d => d._id));
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /actors/:id
router.get('/:id', requireAuth, async (req, res) => {
  try {
    validateId(req.params.id);
    const doc = await ActorDef.findById(req.params.id).lean();
    if (!doc) return res.status(404).json({ error: 'Not found' });
    res.json(doc.data);
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// PUT /actors/:id  (admin only)
router.put('/:id', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    validateId(req.params.id);
    const body = req.body;
    if (!body || typeof body !== 'object') {
      return res.status(400).json({ error: 'Body must be a JSON object' });
    }
    await ActorDef.findByIdAndUpdate(
      req.params.id,
      { data: body },
      { upsert: true, new: true },
    );
    writeToDisk(req.params.id, body);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// DELETE /actors/:id  (admin only)
router.delete('/:id', requireAuth, requireRole('admin'), async (req, res) => {
  try {
    validateId(req.params.id);
    const deleted = await ActorDef.findByIdAndDelete(req.params.id);
    if (!deleted) return res.status(404).json({ error: 'Not found' });
    const path = diskPath(req.params.id);
    if (existsSync(path)) unlinkSync(path);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

export default router;
