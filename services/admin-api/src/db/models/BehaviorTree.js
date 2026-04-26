import mongoose from 'mongoose';

// Stores the full behavior tree JSON blob keyed by tree id (e.g. "guard", "civilian-flee").
// _id is the tree id string so lookups are a simple findById.
const behaviorTreeSchema = new mongoose.Schema({
  _id:  { type: String },
  data: { type: mongoose.Schema.Types.Mixed, required: true },
}, { timestamps: true });

export default mongoose.model('BehaviorTree', behaviorTreeSchema);
