// Stream parser for the TUIBackend wire protocol (little-endian, framed):
//   [u8 type][u32 len][payload of len bytes]
//   type 0x01 PALETTE  payload = 256 * RGBA (1024 bytes, alpha forced 255)
//   type 0x02 FRAME    payload = u16 w, u16 h, w*h indexed bytes

export type Palette = Uint8Array; // 1024 bytes RGBA (256 entries)

export interface Frame {
  w: number;
  h: number;
  pixels: Uint8Array; // length w*h, indexed
}

export type FrameMessage =
  | { type: 'palette'; palette: Palette }
  | { type: 'frame'; frame: Frame };

export class FrameStreamParser {
  private buf: Uint8Array = new Uint8Array(0);

  push(chunk: Uint8Array): FrameMessage[] {
    const merged = new Uint8Array(this.buf.length + chunk.length);
    merged.set(this.buf, 0);
    merged.set(chunk, this.buf.length);
    this.buf = merged;

    const out: FrameMessage[] = [];
    while (this.buf.length >= 5) {
      const type = this.buf[0]!;
      // `>>> 0` coerces to unsigned. Without it `<< 24` returns a signed
      // int32: any length with byte[4] >= 0x80 wraps negative, the `length
      // < 5+len` guard never trips, and the buffer-advance step clamps
      // back to 0 on a negative end — i.e. infinite loop on corrupt data.
      const len =
        (this.buf[1]! |
          (this.buf[2]! << 8) |
          (this.buf[3]! << 16) |
          (this.buf[4]! << 24)) >>>
        0;
      if (this.buf.length < 5 + len) break;
      const payload = this.buf.subarray(5, 5 + len);
      this.buf = this.buf.slice(5 + len);

      if (type === 0x01) {
        if (payload.length !== 1024) {
          throw new Error(`palette payload size ${payload.length} != 1024`);
        }
        out.push({ type: 'palette', palette: new Uint8Array(payload) });
      } else if (type === 0x02) {
        if (payload.length < 4) {
          throw new Error(`frame payload too small: ${payload.length}`);
        }
        const w = payload[0]! | (payload[1]! << 8);
        const h = payload[2]! | (payload[3]! << 8);
        const pixels = new Uint8Array(payload.subarray(4));
        if (pixels.length !== w * h) {
          throw new Error(`frame pixel count ${pixels.length} != ${w}*${h}`);
        }
        out.push({ type: 'frame', frame: { w, h, pixels } });
      } else {
        throw new Error(`unknown frame message type 0x${type.toString(16)}`);
      }
    }
    return out;
  }
}
