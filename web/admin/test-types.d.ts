declare module 'bun:test' {
  export function describe(name: string, fn: () => void): void;
  export function test(name: string, fn: () => void | Promise<void>): void;
  export function expect(value: unknown): {
    toBe(expected: unknown): void;
    toBeNull(): void;
    toContain(expected: string): void;
    toHaveLength(expected: number): void;
    toThrow(expected?: string): void;
    not: {
      toBe(expected: unknown): void;
    };
  };
}
