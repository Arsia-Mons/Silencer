import { encodeFrame, parseFrames, type Reply, type Request } from "./protocol.ts";

export async function rpcCall(socketPath: string, req: Request): Promise<Reply> {
  for await (const r of rpcStream(socketPath, req)) {
    if (r.final) return r;
  }
  throw new Error("daemon closed connection without a final reply");
}

export function rpcStream(socketPath: string, req: Request): AsyncIterable<Reply> {
  return {
    [Symbol.asyncIterator]() {
      const queue: Reply[] = [];
      let waiters: Array<(r: IteratorResult<Reply>) => void> = [];
      let done = false;
      let errored: unknown = null;
      let buf = "";
      let socket: any;

      const push = (r: Reply) => {
        if (waiters.length) waiters.shift()!({ value: r, done: false });
        else queue.push(r);
        if (r.final) finish();
      };
      const finish = () => {
        done = true;
        for (const w of waiters) w({ value: undefined as any, done: true });
        waiters = [];
        try {
          socket?.end();
        } catch {
          /* ignore */
        }
      };
      const fail = (e: unknown) => {
        errored = e;
        done = true;
        for (const w of waiters) w({ value: undefined as any, done: true });
        waiters = [];
      };

      Bun.connect({
        unix: socketPath,
        socket: {
          open(s: any) {
            socket = s;
            s.write(encodeFrame(req));
          },
          data(_s: any, chunk: Uint8Array) {
            buf += new TextDecoder().decode(chunk);
            let parsed;
            try {
              parsed = parseFrames<Reply>(buf);
            } catch (e) {
              fail(e);
              return;
            }
            buf = parsed.rest;
            for (const f of parsed.frames) push(f);
          },
          close() {
            if (!done) fail(new Error("daemon closed connection"));
          },
          error(_s: any, e: Error) {
            fail(e);
          },
        },
      }).catch(fail);

      return {
        async next(): Promise<IteratorResult<Reply>> {
          if (errored) throw errored;
          if (queue.length) return { value: queue.shift()!, done: false };
          if (done) return { value: undefined as any, done: true };
          return new Promise((resolve) => waiters.push(resolve));
        },
      };
    },
  };
}
