import mongoose from 'mongoose';

const eventSchema = new mongoose.Schema({
  type:      { type: String, required: true, index: true },
  accountId: { type: Number, index: true },
  gameId:    { type: Number, index: true },
  data:      { type: mongoose.Schema.Types.Mixed },
  ts:        { type: Date, default: Date.now, index: true },
}, { timestamps: false });

export default mongoose.model('Event', eventSchema);
