import express from 'express';
import { LOBBY_MAP_API_URL } from '../config.js';

const router = express.Router();

// GET /api/maps — list published maps
router.get('/', async (_req, res) => {
  try {
    const r = await fetch(`${LOBBY_MAP_API_URL}/api/maps`);
    res.status(r.status).json(await r.json());
  } catch {
    res.status(502).json({ error: 'map API unreachable' });
  }
});

// POST /api/maps — upload a map (binary body proxied through)
router.post('/', express.raw({ type: 'application/octet-stream', limit: '10mb' }), async (req, res) => {
  try {
    const fwd = {};
    if (req.headers['x-filename']) fwd['X-Filename'] = req.headers['x-filename'];
    if (req.headers['x-author'])   fwd['X-Author']   = req.headers['x-author'];
    if (req.headers['x-api-key'])  fwd['X-Api-Key']  = req.headers['x-api-key'];

    const r = await fetch(`${LOBBY_MAP_API_URL}/api/maps`, {
      method: 'POST',
      body: req.body,
      headers: { 'Content-Type': 'application/octet-stream', ...fwd },
    });
    const ct = r.headers.get('content-type') || '';
    if (ct.includes('application/json')) {
      res.status(r.status).json(await r.json());
    } else {
      const text = (await r.text()).trim();
      res.status(r.status).json({ error: text || r.statusText });
    }
  } catch {
    res.status(502).json({ error: 'map API unreachable' });
  }
});

// GET /api/maps/* — download by sha1 or name
router.get('/*', async (req, res) => {
  try {
    const r = await fetch(`${LOBBY_MAP_API_URL}/api/maps${req.path}`);
    if (!r.ok) { res.status(r.status).send(await r.text()); return; }
    const cd = r.headers.get('content-disposition');
    if (cd) res.set('Content-Disposition', cd);
    res.set('Content-Type', r.headers.get('content-type') || 'application/octet-stream');
    res.send(Buffer.from(await r.arrayBuffer()));
  } catch {
    res.status(502).json({ error: 'map API unreachable' });
  }
});

export default router;
