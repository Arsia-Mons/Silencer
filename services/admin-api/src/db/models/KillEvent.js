import mongoose from 'mongoose';

const killEventSchema = new mongoose.Schema({
  gameId:            { type: Number, required: true, index: true },
  mapName:           { type: String, required: true },
  x:                 { type: Number, required: true },
  y:                 { type: Number, required: true },
  killerAccountId:   { type: Number, index: true },
  victimAccountId:   { type: Number, index: true },
  weapon:            { type: Number, default: 0 },
  agencyIdx:         { type: Number, default: 0 },
  ts:                { type: Date, default: Date.now, index: true },
}, { timestamps: false });

killEventSchema.index({ mapName: 1, ts: -1 });
killEventSchema.index({ killerAccountId: 1, ts: -1 });
// TTL: auto-expire after 90 days to keep collection lean
killEventSchema.index({ ts: 1 }, { expireAfterSeconds: 60 * 60 * 24 * 90 });

export default mongoose.model('KillEvent', killEventSchema);
