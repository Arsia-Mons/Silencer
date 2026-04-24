import mongoose from 'mongoose';

const weaponSchema = new mongoose.Schema({
  fires:       { type: Number, default: 0 },
  hits:        { type: Number, default: 0 },
  playerKills: { type: Number, default: 0 },
}, { _id: false });

// Per-match stats document. One record per player per game.
// Mirrors Stats class from src/stats.h / stats.cpp.
const matchStatSchema = new mongoose.Schema({
  accountId:  { type: Number, required: true, index: true },
  gameId:     { type: Number, required: true, index: true },
  agencyIdx:  { type: Number, default: 0 },
  won:        { type: Boolean, default: false },
  xp:         { type: Number, default: 0 },

  // weapon[0]=Blaster, [1]=Laser, [2]=Rocket, [3]=Flamer
  weapons:    { type: [weaponSchema], default: () => Array.from({ length: 4 }, () => ({})) },

  // NPC / environment kills
  civiliansKilled:    { type: Number, default: 0 },
  guardsKilled:       { type: Number, default: 0 },
  robotsKilled:       { type: Number, default: 0 },
  defenseKilled:      { type: Number, default: 0 },

  // Secrets / objectives
  secretsPickedUp:    { type: Number, default: 0 },
  secretsReturned:    { type: Number, default: 0 },
  secretsStolen:      { type: Number, default: 0 },
  secretsDropped:     { type: Number, default: 0 },

  powerupsPickedUp:   { type: Number, default: 0 },

  // Combat
  deaths:             { type: Number, default: 0 },
  kills:              { type: Number, default: 0 },
  suicides:           { type: Number, default: 0 },
  poisons:            { type: Number, default: 0 },

  // Tech / throwables
  tractsPlanted:      { type: Number, default: 0 },
  grenadesThrown:     { type: Number, default: 0 },
  neutronsThrown:     { type: Number, default: 0 },
  empsThrown:         { type: Number, default: 0 },
  shapedThrown:       { type: Number, default: 0 },
  plasmasThrown:      { type: Number, default: 0 },
  flaresThrown:       { type: Number, default: 0 },
  poisonFlaresThrown: { type: Number, default: 0 },

  // Support
  healthPacksUsed:       { type: Number, default: 0 },
  fixedCannonsPlaced:    { type: Number, default: 0 },
  fixedCannonsDestroyed: { type: Number, default: 0 },
  detsPlanted:           { type: Number, default: 0 },
  camerasPlanted:        { type: Number, default: 0 },
  virusesUsed:           { type: Number, default: 0 },

  // Terminal hacking
  filesHacked:    { type: Number, default: 0 },
  filesReturned:  { type: Number, default: 0 },
// Economy & healing
  creditsEarned:  { type: Number, default: 0 },
  creditsSpent:   { type: Number, default: 0 },
  healsDone:      { type: Number, default: 0 },
}, { timestamps: true });

matchStatSchema.index({ accountId: 1, gameId: 1 }, { unique: true });

export default mongoose.model('MatchStat', matchStatSchema);
