import http from 'http';
import express from 'express';
import cors from 'cors';
import { PORT } from './config.js';
import { connectDB } from './db/connection.js';
import { initWS } from './ws/index.js';
import { startConsumer } from './amqp/consumer.js';
import authRoutes    from './routes/auth.js';
import playerRoutes  from './routes/players.js';
import sessionRoutes from './routes/sessions.js';
import eventRoutes   from './routes/events.js';
import statsRoutes   from './routes/stats.js';
import meRoutes      from './routes/me.js';
import backupRoutes     from './routes/backup.js';
import gameStatsRoutes from './routes/gamestats.js';
import { startBackupScheduler } from './backup/scheduler.js';
import AdminUser from './db/models/AdminUser.js';

const app    = express();
const server = http.createServer(app);

app.use(cors());
app.use(express.json());

app.use('/auth',     authRoutes);
app.use('/players',  playerRoutes);
app.use('/sessions', sessionRoutes);
app.use('/events',   eventRoutes);
app.use('/stats',    statsRoutes);
app.use('/me',       meRoutes);
app.use('/backup',     backupRoutes);
app.use('/gamestats', gameStatsRoutes);

app.get('/health', (_req, res) => res.json({ ok: true }));

async function seed() {
  const count = await AdminUser.countDocuments();
  if (count === 0) {
    const passHash = await AdminUser.hashPassword('admin');
    await AdminUser.create({ username: 'admin', passHash, role: 'superadmin', createdBy: 'seed' });
    console.log('[seed] created default admin user (username: admin, password: admin) — CHANGE THIS!');
  }
}

async function start() {
  await connectDB();
  await seed();
  startBackupScheduler();
  initWS(server);
  await startConsumer();
  server.listen(PORT, () => console.log(`[api] zSILENCER admin API on :${PORT}`));
}

start().catch((e) => { console.error(e); process.exit(1); });
