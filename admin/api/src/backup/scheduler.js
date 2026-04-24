import cron from 'node-cron';
import { triggerBackup } from './manager.js';

const CRON_SCHEDULE = process.env.BACKUP_CRON || '0 */6 * * *'; // every 6 hours

export function startBackupScheduler() {
  cron.schedule(CRON_SCHEDULE, () => {
    console.log('[backup] auto-backup triggered by scheduler');
    triggerBackup();
  });
  console.log(`[backup] scheduler started — ${CRON_SCHEDULE}`);
}
