/**
 * Behavior tree endpoints.
 *
 * The filesystem is the source of truth. Behavior trees are stored as JSON
 * files in shared/assets/behaviortrees/ and committed to git. The admin UI
 * reads them from disk for visualization; writes go to disk only so that
 * changes flow back through version control.
 *
 * GET    /behaviortrees         — list all behavior tree IDs  (public)
 * GET    /behaviortrees/:id     — return behavior tree JSON   (public)
 * PUT    /behaviortrees/:id     — write behavior tree to disk (admin only)
 * DELETE /behaviortrees/:id     — delete behavior tree file   (admin only)
 */

import { Router } from 'express';
import { writeFileSync, unlinkSync, existsSync, mkdirSync } from 'fs';
import { readdirSync, readFileSync } from 'fs';
import { join } from 'path';
import { requireAuth, requireRole } from '../auth/jwt.js';
import { ASSETS_DIR } from '../config.js';

const router = Router();
const BT_DIR = join(ASSETS_DIR, 'behaviortrees');

const VALID_NODE_TYPES = new Set([
  'Selector', 'Sequence', 'Parallel', 'RandomSelector',
  'Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess',
  'Wait', 'Leaf', 'Condition',
]);

function validateId(id) {
  if (!id || /[/\\.]/.test(id)) throw new Error('Invalid behavior tree id');
}

function diskPath(id) {
  return join(BT_DIR, `${id}.json`);
}

function ensureDir() {
  if (!existsSync(BT_DIR)) mkdirSync(BT_DIR, { recursive: true });
}

function writeToDisk(id, data) {
  ensureDir();
  writeFileSync(diskPath(id), JSON.stringify(data, null, 2), 'utf8');
}

function validate(bt) {
  if (!bt || typeof bt !== 'object') return 'Not an object';
  if (!bt.rootId || typeof bt.rootId !== 'string') return 'Missing rootId';
  if (!bt.nodes || typeof bt.nodes !== 'object') return 'Missing nodes';
  if (!bt.nodes[bt.rootId]) return `rootId "${bt.rootId}" not found in nodes`;
  for (const [id, node] of Object.entries(bt.nodes)) {
    if (!VALID_NODE_TYPES.has(node.type)) return `Node "${id}" has invalid type "${node.type}"`;
    if (!Array.isArray(node.children)) return `Node "${id}" missing children array`;
    const isDecorator = ['Inverter', 'Cooldown', 'Repeat', 'Timeout', 'ForceSuccess'].includes(node.type);
    if (isDecorator && node.children.length !== 1) return `Decorator "${id}" must have exactly 1 child`;
    const isLeafOrCond = ['Leaf', 'Condition', 'Wait'].includes(node.type);
    if (isLeafOrCond && node.children.length !== 0) return `${node.type} "${id}" must have 0 children`;
    for (const cid of node.children) {
      if (!bt.nodes[cid]) return `Node "${id}" references unknown child "${cid}"`;
    }
  }
  // Detect cycles via DFS
  const visited = new Set();
  function dfs(nid) {
    if (visited.has(nid)) return `Cycle detected at node "${nid}"`;
    visited.add(nid);
    for (const cid of bt.nodes[nid]?.children ?? []) {
      const err = dfs(cid);
      if (err) return err;
    }
    visited.delete(nid);
    return null;
  }
  return dfs(bt.rootId);
}

// GET /behaviortrees  — public
router.get('/', (_req, res) => {
  try {
    ensureDir();
    const ids = readdirSync(BT_DIR)
      .filter(f => f.endsWith('.json'))
      .map(f => f.slice(0, -5));
    res.json(ids);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /behaviortrees/:id  — public
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

// PUT /behaviortrees/:id  (admin only) — writes to disk only; commit to git to ship
router.put('/:id', requireAuth, requireRole('admin'), (req, res) => {
  try {
    validateId(req.params.id);
    const body = req.body;
    if (!body || typeof body !== 'object') {
      return res.status(400).json({ error: 'Body must be a JSON object' });
    }
    const err = validate(body);
    if (err) return res.status(400).json({ error: err });
    body.id = req.params.id;
    writeToDisk(req.params.id, body);
    res.json({ ok: true });
  } catch (err) {
    res.status(err.message.includes('Invalid') ? 400 : 500).json({ error: err.message });
  }
});

// DELETE /behaviortrees/:id  (admin only)
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
