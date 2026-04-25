import mongoose from 'mongoose';
import { MONGO_URL } from '../config.js';

export async function connectDB() {
  mongoose.connection.on('connected', () => console.log('[db] MongoDB connected'));
  mongoose.connection.on('error', (e) => console.error('[db] MongoDB error:', e));
  mongoose.connection.on('disconnected', () => console.log('[db] MongoDB disconnected'));
  await mongoose.connect(MONGO_URL);
}
