/**
 * GET /api/physics-materials
 * Returns the fixed list of physics material IDs known to the C++ engine
 * (nameToId map in gasloader.cpp). Used by PropsTab footstep override picker.
 * The list is static — adding a new material requires updating both C++ and here.
 */
import { NextResponse } from 'next/server';

const MATERIALS = [
  'Asphalt', 'Brick', 'Carpet', 'Concrete', 'Dirt', 'EnergyForcefield',
  'FleshOrganic', 'Glass', 'GrassDry', 'GrassLush', 'Gravel', 'Ice',
  'Linoleum', 'MagmaAsh', 'Marble', 'MetalGrate', 'MetalSolid', 'Mud',
  'Puddle', 'Rock', 'Sand', 'SnowCrust', 'SnowPowder', 'Tile',
  'WaterDeep', 'WaterShallow', 'WoodCreaky', 'WoodSolid',
].map(id => ({ id }));

export async function GET() {
  return NextResponse.json(MATERIALS);
}
