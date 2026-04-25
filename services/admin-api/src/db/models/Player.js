import mongoose from 'mongoose';

const agencySchema = new mongoose.Schema({
  wins:         { type: Number, default: 0 },
  losses:       { type: Number, default: 0 },
  xpToNextLevel:{ type: Number, default: 0 },
  level:        { type: Number, default: 0 },
  endurance:    { type: Number, default: 0 },
  shield:       { type: Number, default: 0 },
  jetpack:      { type: Number, default: 0 },
  techSlots:    { type: Number, default: 3 },
  hacking:      { type: Number, default: 0 },
  contacts:     { type: Number, default: 0 },
}, { _id: false });

const ipEntrySchema = new mongoose.Schema({
  ip:        { type: String, required: true },
  firstSeen: { type: Date, default: Date.now },
  lastSeen:  { type: Date, default: Date.now },
  count:     { type: Number, default: 1 },
}, { _id: false });

// Lifetime cumulative stats — $inc'd after every match. Mirrors MatchStat fields.
const lifetimeStatsSchema = new mongoose.Schema({
  // weapon[0]=Blaster, [1]=Laser, [2]=Rocket, [3]=Flamer (stored flat for easy querying)
  blasterFires:   { type: Number, default: 0 },
  blasterHits:    { type: Number, default: 0 },
  blasterKills:   { type: Number, default: 0 },
  laserFires:     { type: Number, default: 0 },
  laserHits:      { type: Number, default: 0 },
  laserKills:     { type: Number, default: 0 },
  rocketFires:    { type: Number, default: 0 },
  rocketHits:     { type: Number, default: 0 },
  rocketKills:    { type: Number, default: 0 },
  flamerFires:    { type: Number, default: 0 },
  flamerHits:     { type: Number, default: 0 },
  flamerKills:    { type: Number, default: 0 },

  civiliansKilled:    { type: Number, default: 0 },
  guardsKilled:       { type: Number, default: 0 },
  robotsKilled:       { type: Number, default: 0 },
  defenseKilled:      { type: Number, default: 0 },
  secretsPickedUp:    { type: Number, default: 0 },
  secretsReturned:    { type: Number, default: 0 },
  secretsStolen:      { type: Number, default: 0 },
  secretsDropped:     { type: Number, default: 0 },
  powerupsPickedUp:   { type: Number, default: 0 },
  deaths:             { type: Number, default: 0 },
  kills:              { type: Number, default: 0 },
  suicides:           { type: Number, default: 0 },
  poisons:            { type: Number, default: 0 },
  tractsPlanted:      { type: Number, default: 0 },
  grenadesThrown:     { type: Number, default: 0 },
  neutronsThrown:     { type: Number, default: 0 },
  empsThrown:         { type: Number, default: 0 },
  shapedThrown:       { type: Number, default: 0 },
  plasmasThrown:      { type: Number, default: 0 },
  flaresThrown:       { type: Number, default: 0 },
  poisonFlaresThrown: { type: Number, default: 0 },
  healthPacksUsed:       { type: Number, default: 0 },
  fixedCannonsPlaced:    { type: Number, default: 0 },
  fixedCannonsDestroyed: { type: Number, default: 0 },
  detsPlanted:           { type: Number, default: 0 },
  camerasPlanted:        { type: Number, default: 0 },
  virusesUsed:           { type: Number, default: 0 },
  filesHacked:    { type: Number, default: 0 },
  filesReturned:  { type: Number, default: 0 },
  creditsEarned:  { type: Number, default: 0 },
  creditsSpent:   { type: Number, default: 0 },
  healsDone:      { type: Number, default: 0 },
}, { _id: false });

const playerSchema = new mongoose.Schema({
  accountId:        { type: Number, required: true, unique: true, index: true },
  name:             { type: String, required: true, index: true },
  agencies:         { type: [agencySchema], default: () => Array.from({ length: 5 }, () => ({})) },
  lifetimeStats:    { type: lifetimeStatsSchema, default: () => ({}) },
  firstSeen:        { type: Date, default: Date.now },
  lastSeen:         { type: Date, default: Date.now },
  loginCount:       { type: Number, default: 0 },
  lastIp:           { type: String, default: '' },
  ipHistory:        { type: [ipEntrySchema], default: [] },
  totalPlaytimeSecs:{ type: Number, default: 0 },
  banned:           { type: Boolean, default: false },
  banReason:        { type: String, default: '' },
}, { timestamps: true });

export default mongoose.model('Player', playerSchema);
