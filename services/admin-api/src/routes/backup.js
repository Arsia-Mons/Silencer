import { Router } from 'express';
import { requireAuth } from '../auth/jwt.js';
import { triggerBackup, listBackupFiles, getBackupState } from '../backup/manager.js';

const router = Router();

// POST /backup/trigger — start a backup (async, returns 202)
router.post('/trigger', requireAuth, (req, res) => {
  const result = triggerBackup();
  if (!result.ok && result.inProgress) {
    return res.status(409).json({ error: 'Backup already in progress' });
  }
  res.status(202).json(result);
});

// GET /backup/status — current backup state + last result
router.get('/status', requireAuth, (req, res) => {
  res.json(getBackupState());
});

// GET /backup/list — list all backup files on disk
router.get('/list', requireAuth, async (req, res) => {
  try {
    const files = await listBackupFiles();
    res.json({ files });
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

export default router;
