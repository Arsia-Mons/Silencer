/**
 * IMA ADPCM (WAVE_FORMAT_DVI_ADPCM, 0x0011) WAV parser + decoder.
 *
 * Handles the exact format produced by the Silencer sound.bin packer:
 *   11025 Hz, mono, 4-bit, 256-byte blocks, 505 samples/block.
 */

const STEP_TABLE = [
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
  45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
  230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
  963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
  3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
  10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
  29794, 32767,
];

const INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8];

interface WavInfo {
  sampleRate: number;
  channels: number;
  blockAlign: number;
  samplesPerBlock: number;
  data: Uint8Array;
}

function parseWav(buf: ArrayBuffer): WavInfo {
  const v = new DataView(buf);
  const b = new Uint8Array(buf);
  const tag4 = (o: number) => String.fromCharCode(b[o], b[o+1], b[o+2], b[o+3]);

  if (tag4(0) !== 'RIFF' || tag4(8) !== 'WAVE') throw new Error('Not a WAV file');

  // Defaults matching the Silencer format
  let sampleRate = 11025, channels = 1, blockAlign = 256, samplesPerBlock = 505;
  let dataStart = 0, dataLen = 0;

  let i = 12;
  while (i + 8 <= b.length) {
    const chunkTag = tag4(i);
    const chunkSz = v.getUint32(i + 4, true);
    if (chunkTag === 'fmt ') {
      channels       = v.getUint16(i + 10, true);
      sampleRate     = v.getUint32(i + 12, true);
      blockAlign     = v.getUint16(i + 20, true);
      // cbSize at i+24 = 2 → samplesPerBlock at i+26
      if (chunkSz >= 20) samplesPerBlock = v.getUint16(i + 26, true);
    } else if (chunkTag === 'data') {
      dataStart = i + 8;
      dataLen = chunkSz;
      break;
    }
    i += 8 + chunkSz + (chunkSz & 1); // word-aligned
  }

  if (!dataStart) throw new Error('No data chunk');
  return { sampleRate, channels, blockAlign, samplesPerBlock, data: b.slice(dataStart, dataStart + dataLen) };
}

function decodeImaAdpcm(info: WavInfo): Float32Array {
  const { data, blockAlign, samplesPerBlock } = info;
  const numBlocks = Math.floor(data.length / blockAlign);
  const out = new Float32Array(numBlocks * samplesPerBlock);
  let outIdx = 0;
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);

  for (let b = 0; b < numBlocks; b++) {
    const base = b * blockAlign;
    // Block header: predictor (int16 LE), stepIndex (uint8), reserved (uint8)
    let predictor = dv.getInt16(base, true);
    let stepIdx = data[base + 2];
    if (stepIdx > 88) stepIdx = 88;

    out[outIdx++] = predictor / 32768;

    for (let di = base + 4; di < base + blockAlign; di++) {
      const byte = data[di];
      // Low nibble first, then high nibble
      for (let n = 0; n < 2; n++) {
        const nibble = n === 0 ? (byte & 0x0f) : (byte >> 4) & 0x0f;
        const step = STEP_TABLE[stepIdx];
        let diff = step >> 3;
        if (nibble & 4) diff += step;
        if (nibble & 2) diff += step >> 1;
        if (nibble & 1) diff += step >> 2;
        if (nibble & 8) diff = -diff;
        predictor = Math.max(-32768, Math.min(32767, predictor + diff));
        stepIdx = Math.max(0, Math.min(88, stepIdx + INDEX_TABLE[nibble]));
        out[outIdx++] = predictor / 32768;
      }
    }
  }

  return out.subarray(0, outIdx);
}

/** Decode an IMA ADPCM WAV ArrayBuffer → Web Audio AudioBuffer. */
export async function decodeAdpcmWav(arrayBuf: ArrayBuffer, ctx: AudioContext): Promise<AudioBuffer> {
  const info = parseWav(arrayBuf);
  const pcm = decodeImaAdpcm(info);
  const audioBuffer = ctx.createBuffer(info.channels, pcm.length, info.sampleRate);
  audioBuffer.copyToChannel(new Float32Array(pcm), 0);
  return audioBuffer;
}
