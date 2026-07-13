import mongoose from 'mongoose';

// Sections mirror docs/roadmap.md. Keep in sync with SECTION_META in
// web/admin/app/roadmap/page.tsx.
export const ROADMAP_SECTIONS = ['modes', 'mechanics', 'weapons', 'npcs', 'objects', 'items'];
export const ROADMAP_STATUSES = ['proposed', 'designing', 'in-progress', 'shipped'];
export const ROADMAP_EFFORTS = ['S', 'M', 'L'];

const roadmapItemSchema = new mongoose.Schema({
  section:  { type: String, enum: ROADMAP_SECTIONS, required: true },
  title:    { type: String, required: true },
  detail:   { type: String, default: '' },
  buildsOn: { type: String, default: '' },
  status:   { type: String, enum: ROADMAP_STATUSES, default: 'proposed' },
  effort:   { type: String, enum: ROADMAP_EFFORTS, default: 'M' },
  order:    { type: Number, default: 0 },
  createdBy:{ type: String },
}, { timestamps: true });

export default mongoose.model('RoadmapItem', roadmapItemSchema);
