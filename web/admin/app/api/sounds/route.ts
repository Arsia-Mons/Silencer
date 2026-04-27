import { NextResponse } from 'next/server';
import { readFileSync } from 'fs';
import path from 'path';

function readSoundNames(): string[] {
  const binPath = path.join(process.cwd(), '../../shared/assets/sound.bin');
  const buf = readFileSync(binPath);
  const numsounds = buf.readUInt32LE(0);
  const HEADER_SIZE = 0x60;
  const names: string[] = [];
  for (let i = 0; i < numsounds; i++) {
    const base = 8 + i * HEADER_SIZE;
    const nameBytes = buf.subarray(base + 4, base + 4 + 0x10);
    const end = nameBytes.indexOf(0);
    const name = nameBytes.subarray(0, end < 0 ? undefined : end).toString('ascii');
    const length = buf.readUInt32LE(base + 4 + 0x10 + 4);
    if (name && length >= 256) names.push(name);
  }
  return [...new Set(names)].sort();
}

let cached: string[] | null = null;

export async function GET() {
  if (!cached) cached = readSoundNames();
  return NextResponse.json(cached);
}
