/**
 * Sound Cue endpoints.
 *
 * Sound Cues are small node-graph JSON files stored in
 * shared/assets/gas/sound-cues/<id>.json.  Each file is a self-contained
 * graph (nodes + edges) that the game runtime evaluates at play-time to pick
 * a sound dynamically (random, round-robin, mixer, etc.).
 *
 * GET    /sound-cues           list all cue IDs + metadata
 * GET    /sound-cues/:id       return full cue JSON
 * PUT    /sound-cues/:id       save / create a cue (body = cue JSON)
 * DELETE /sound-cues/:id       delete a cue file
 */

import { Router } from 'express';
import { existsSync, readdirSync, readFileSync, writeFileSync, unlinkSync, mkdirSync } from 'fs';
import { join, basename } from 'path';
import { requireAuth } from '../auth/jwt.js';

const router = Router();

const ASSETS_DIR = process.env.ASSETS_DIR
  || join(import.meta.dir, '../../../../shared/assets');
const CUES_DIR = join(ASSETS_DIR, 'gas', 'sound-cues');

function ensureCuesDir() {
  if (!existsSync(CUES_DIR)) mkdirSync(CUES_DIR, { recursive: true });
}

function sanitizeId(id) {
  return id.replace(/[^a-zA-Z0-9_-]/g, '_');
}

// GET /sound-cues — list all cues
router.get('/', requireAuth, (_req, res) => {
  ensureCuesDir();
  try {
    const files = readdirSync(CUES_DIR).filter(f => f.endsWith('.json'));
    const cues = files.map(f => {
      const raw = readFileSync(join(CUES_DIR, f), 'utf8');
      try {
        const parsed = JSON.parse(raw);
        return {
          id: parsed.id ?? basename(f, '.json'),
          nodeCount: Array.isArray(parsed.nodes) ? parsed.nodes.length : 0,
          edgeCount: Array.isArray(parsed.edges) ? parsed.edges.length : 0,
        };
      } catch {
        return { id: basename(f, '.json'), nodeCount: 0, edgeCount: 0 };
      }
    });
    res.json(cues);
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// GET /sound-cues/:id — return full cue JSON
router.get('/:id', requireAuth, (req, res) => {
  ensureCuesDir();
  const id = sanitizeId(req.params.id);
  const path = join(CUES_DIR, `${id}.json`);
  if (!existsSync(path)) return res.status(404).json({ error: 'Cue not found' });
  try {
    res.json(JSON.parse(readFileSync(path, 'utf8')));
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// PUT /sound-cues/:id — save / create
router.put('/:id', requireAuth, async (req, res) => {
  ensureCuesDir();
  const id = sanitizeId(req.params.id);
  const path = join(CUES_DIR, `${id}.json`);
  try {
    const body = await req.json();
    // Enforce id field matches URL
    body.id = id;
    // Basic validation
    if (!Array.isArray(body.nodes)) return res.status(400).json({ error: 'nodes must be an array' });
    if (!Array.isArray(body.edges)) return res.status(400).json({ error: 'edges must be an array' });
    const outputNodes = body.nodes.filter(n => n.type === 'Output');
    if (outputNodes.length !== 1) return res.status(400).json({ error: 'Cue must have exactly one Output node' });
    writeFileSync(path, JSON.stringify(body, null, 2));
    res.json({ ok: true, id });
  } catch (e) {
    res.status(400).json({ error: e.message });
  }
});

// DELETE /sound-cues/:id
router.delete('/:id', requireAuth, (req, res) => {
  ensureCuesDir();
  const id = sanitizeId(req.params.id);
  const path = join(CUES_DIR, `${id}.json`);
  if (!existsSync(path)) return res.status(404).json({ error: 'Cue not found' });
  try {
    unlinkSync(path);
    res.json({ ok: true });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
