// JSON-lines RPC over AF_UNIX. One frame per newline-terminated line.

export type Request = {
  id: number;
  op: string;
  args: Record<string, unknown>;
  stream?: boolean;
};

export type Reply = {
  id: number;
  ok: boolean;
  result?: unknown;
  error?: string;
  code?: string;
  final: boolean;
};

export function encodeFrame(frame: Request | Reply): string {
  return JSON.stringify(frame) + "\n";
}

export function parseFrames<T>(buf: string): { frames: T[]; rest: string } {
  const frames: T[] = [];
  let rest = buf;
  for (;;) {
    const nl = rest.indexOf("\n");
    if (nl < 0) return { frames, rest };
    const line = rest.slice(0, nl);
    rest = rest.slice(nl + 1);
    if (line.length === 0) continue;
    try {
      frames.push(JSON.parse(line) as T);
    } catch (e) {
      throw new Error(`malformed RPC frame: ${line} (${(e as Error).message})`);
    }
  }
}
