import { NextResponse } from 'next/server';
import sounds from '../../../public/sounds.json';

export async function GET() {
  return NextResponse.json(sounds);
}
