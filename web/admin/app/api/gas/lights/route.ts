import { NextResponse } from 'next/server';
import fs from 'node:fs';
import path from 'node:path';

export async function GET() {
  try {
    const filePath = path.resolve(process.cwd(), '../../shared/assets/gas/lights.json');
    const content = fs.readFileSync(filePath, 'utf-8');
    return new NextResponse(content, {
      status: 200,
      headers: { 'Content-Type': 'application/json' },
    });
  } catch {
    return NextResponse.json({ lights: [] }, { status: 200 });
  }
}
