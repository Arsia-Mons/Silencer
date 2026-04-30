import { describe, expect, test, beforeEach, afterEach } from "bun:test";
import { resolveLobbydDir, socketPath, logPath, MAX_SUN_PATH } from "../../src/lobby/paths.ts";

describe("resolveLobbydDir", () => {
  const origPlatform = process.platform;
  const origEnv = { ...process.env };
  afterEach(() => {
    Object.defineProperty(process, "platform", { value: origPlatform });
    process.env = { ...origEnv };
  });

  test("override env wins on every platform", () => {
    process.env.SILENCER_LOBBYD_DIR = "/custom/dir";
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
    Object.defineProperty(process, "platform", { value: "win32" });
    expect(resolveLobbydDir()).toBe("/custom/dir");
  });

  test("linux uses XDG_RUNTIME_DIR/silencer when set", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.XDG_RUNTIME_DIR = "/run/user/1000";
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/run/user/1000/silencer");
  });

  test("linux falls back to /tmp/silencer when XDG_RUNTIME_DIR unset", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    delete process.env.XDG_RUNTIME_DIR;
    Object.defineProperty(process, "platform", { value: "linux" });
    expect(resolveLobbydDir()).toBe("/tmp/silencer");
  });

  test("macOS uses TMPDIR/silencer", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.TMPDIR = "/var/folders/xx/yy/T/";
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(resolveLobbydDir()).toBe("/var/folders/xx/yy/T/silencer");
  });

  test("windows uses LOCALAPPDATA\\Silencer\\lobbyd", () => {
    delete process.env.SILENCER_LOBBYD_DIR;
    process.env.LOCALAPPDATA = "C:\\Users\\u\\AppData\\Local";
    Object.defineProperty(process, "platform", { value: "win32" });
    expect(resolveLobbydDir()).toBe("C:\\Users\\u\\AppData\\Local\\Silencer\\lobbyd");
  });
});

describe("socketPath / logPath", () => {
  test("socketPath is dir + lobbyd.sock", () => {
    expect(socketPath("/tmp/silencer")).toBe("/tmp/silencer/lobbyd.sock");
  });
  test("logPath is dir + lobbyd.log", () => {
    expect(logPath("/tmp/silencer")).toBe("/tmp/silencer/lobbyd.log");
  });
  test("socketPath throws when resolved path exceeds MAX_SUN_PATH on darwin", () => {
    const longDir = "/" + "a".repeat(120);
    const origPlatform = process.platform;
    Object.defineProperty(process, "platform", { value: "darwin" });
    expect(() => socketPath(longDir)).toThrow(/exceeds.*sun_path/);
    Object.defineProperty(process, "platform", { value: origPlatform });
  });
});

test("MAX_SUN_PATH is 100 (10-byte safety margin under macOS's 104)", () => {
  expect(MAX_SUN_PATH).toBe(100);
});
