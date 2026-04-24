import mongoose from 'mongoose';
import bcrypt from 'bcryptjs';

const adminUserSchema = new mongoose.Schema({
  username: { type: String, required: true, unique: true },
  passHash: { type: String, required: true },
  role:     { type: String, enum: ['superadmin', 'admin', 'manager', 'moderator', 'viewer'], default: 'viewer' },
  createdBy:{ type: String },
}, { timestamps: true });

adminUserSchema.methods.checkPassword = function (plain) {
  return bcrypt.compare(plain, this.passHash);
};

adminUserSchema.statics.hashPassword = (plain) => bcrypt.hash(plain, 12);

export default mongoose.model('AdminUser', adminUserSchema);
