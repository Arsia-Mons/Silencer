/**
 * GET /api/physics-materials
 * Reads physics material JSON files from the shared GAS assets directory and
 * returns them as an array sorted by id. Used by PropsTab footstep override picker.
 */
import { NextResponse } from 'next/server';
import { readdir, readFile } from 'fs/promises';
import { resolve } from 'path';

const MAT_DIR = resolve(process.cwd(), '../../shared/assets/gas/physics_materials');

export async function GET() {
  try {
    const files = await readdir(MAT_DIR);
    const results: { id: string }[] = [];
    for (const file of files) {
      if (!file.endsWith('.json')) continue;
      try {
        const data = JSON.parse(await readFile(resolve(MAT_DIR, file), 'utf-8')) as { id?: string };
        if (data.id) results.push(data as { id: string });
      } catch { /* skip malformed */ }
    }
    results.sort((a, b) => a.id.localeCompare(b.id));
    return NextResponse.json(results);
  } catch {
    return NextResponse.json([]);
  }
}
