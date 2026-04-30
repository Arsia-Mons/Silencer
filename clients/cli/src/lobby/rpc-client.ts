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
      type Waiter = { resolve: (r: IteratorResult<Reply>) => void; reject: (e: unknown) => void };
      let waiters: Waiter[] = [];
      let done = false;
      let errored: unknown = null;
      let buf = "";
      let socket: any;
      // Per-iterator decoder + stream:true so a multi-byte UTF-8 sequence
      // split across two `data` callbacks survives. A module-level decoder
      // would also work for one consumer, but it would corrupt boundary
      // state if multiple iterators ran concurrently (which the tests do).
      const decoder = new TextDecoder();

      const push = (r: Reply) => {
        if (waiters.length) waiters.shift()!.resolve({ value: r, done: false });
        else queue.push(r);
        if (r.final) finish();
      };
      const finish = () => {
        done = true;
        for (const w of waiters) w.resolve({ value: undefined, done: true });
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
        for (const w of waiters) w.reject(e);
        waiters = [];
        try {
          socket?.end();
        } catch {
          /* ignore */
        }
      };

      Bun.connect({
        unix: socketPath,
        socket: {
          open(s: any) {
            socket = s;
            s.write(encodeFrame(req));
          },
          data(_s: any, chunk: Uint8Array) {
            buf += decoder.decode(chunk, { stream: true });
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
          return new Promise<IteratorResult<Reply>>((resolve, reject) =>
            waiters.push({ resolve, reject }),
          );
        },
        async return(value?: unknown): Promise<IteratorResult<Reply>> {
          finish();
          return { value: value as Reply, done: true };
        },
      };
    },
  };
}
