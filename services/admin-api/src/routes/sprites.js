/**
 * GET /sprites                — list all non-empty banks
 * GET /sprites/:bank          — PNG image of frame 0 (or ?frame=N)
 * GET /sprites/:bank/frames   — array of frame metadata {frame,width,height,offsetX,offsetY}
 * GET /sprites/:bank/:frame   — PNG image of a specific frame
 */

import { Router } from 'express';
import { PNG } from 'pngjs';
import { ASSETS_DIR } from '../config.js';
import { getAllBanks, getBankMetadata, decodeSpriteFrame } from '../sprites/decoder.js';

const router = Router();

/** Convert RGBA buffer → PNG Buffer. */
function rgbaToPng(width, height, rgba) {
  const png = new PNG({ width, height, filterType: -1 });
  png.data = Buffer.from(rgba.buffer, rgba.byteOffset, rgba.byteLength);
  return PNG.sync.write(png);
}

/** Send a 400 error with a JSON body. */
function badRequest(res, msg) {
  return res.status(400).json({ error: msg });
}

// GET /sprites
router.get('/', (_req, res) => {
  try {
    res.json(getAllBanks(ASSETS_DIR));
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// GET /sprites/:bank/frames
router.get('/:bank/frames', (req, res) => {
  const bank = parseInt(req.params.bank, 10);
  if (isNaN(bank)) return badRequest(res, 'bank must be an integer');
  try {
    res.json(getBankMetadata(ASSETS_DIR, bank));
  } catch (err) {
    res.status(err.message.includes('out of range') ? 404 : 500).json({ error: err.message });
  }
});

// GET /sprites/:bank/:frame  or  GET /sprites/:bank?frame=N
router.get('/:bank/:frame?', (req, res) => {
  const bank = parseInt(req.params.bank, 10);
  if (isNaN(bank)) return badRequest(res, 'bank must be an integer');
  const frameNum = req.params.frame !== undefined
    ? parseInt(req.params.frame, 10)
    : parseInt(req.query.frame ?? '0', 10);
  if (isNaN(frameNum)) return badRequest(res, 'frame must be an integer');
  try {
    const { width, height, rgba } = decodeSpriteFrame(ASSETS_DIR, bank, frameNum);
    const png = rgbaToPng(width, height, rgba);
    res.set('Content-Type', 'image/png');
    res.set('Cache-Control', 'public, max-age=86400');
    res.send(png);
  } catch (err) {
    const status = err.message.includes('out of range') ? 404 : 500;
    res.status(status).json({ error: err.message });
  }
});

export default router;
