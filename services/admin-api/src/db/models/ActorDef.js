import mongoose from 'mongoose';

// Stores the full actordef JSON blob keyed by actor id (e.g. "player", "guard").
// _id is the actor id string so lookups are a simple findById.
const actorDefSchema = new mongoose.Schema({
  _id:  { type: String },
  data: { type: mongoose.Schema.Types.Mixed, required: true },
}, { timestamps: true });

export default mongoose.model('ActorDef', actorDefSchema);
