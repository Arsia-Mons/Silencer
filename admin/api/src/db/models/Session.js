import mongoose from 'mongoose';

const sessionSchema = new mongoose.Schema({
  gameId:    { type: Number, required: true, index: true },
  accountId: { type: Number, required: true },
  name:      { type: String },
  mapName:   { type: String },
  hostname:  { type: String },
  port:      { type: Number },
  state:     { type: String, enum: ['created', 'ready', 'ended'], default: 'created' },
  startedAt: { type: Date, default: Date.now },
  endedAt:   { type: Date },
}, { timestamps: true });

export default mongoose.model('Session', sessionSchema);
