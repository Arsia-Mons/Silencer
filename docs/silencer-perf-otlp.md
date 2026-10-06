# Client perf telemetry → OpenObserve (OTLP)

Two-layer telemetry exported from the Silencer client's perf tracer
(`clients/silencer/src/platform/perf_trace.{h,cpp}` +
`perf_otlp.{h,cpp}`) over OTLP/HTTP. **Off by default** — only the
existing `SILENCER_PERF` stdout path runs unless `SILENCER_PERF_OTLP`
is set. The two paths are independent.

- **Layer 1 — METRICS** (always-on once enabled, low-cardinality, one
  flush/second): `fps`, frame-ms `p50/p95/p99`, worst-frame, and
  per-section avg ms (`ui`, `present`, `sim`, `world.draw`, …). Exported
  as **OTLP metrics**. Each metric name becomes its own OpenObserve
  metrics stream (`frame_ms_p95`, `frame_fps`, `ui`, …). This is the
  signal filter: query it to find slow cohorts, then pull their traces.
- **Layer 2 — TRACES** (sampled flame graphs). A trace = one logical
  **operation**:
  - `startup` — process start → first frame ready (palette + resources
    load as child spans). Always emitted.
  - `level_load` — each map load; carries a `map` span attribute. Always
    emitted.
  - `frame` — every rendered frame; the existing `PERF_SCOPE`s
    (`game.tick`, `sim`, `world.draw`, `ui`, `present`) nest under it.
    **Tail-sampled**: emitted only when `frame_ms > budget × factor`
    (default `16.67 ms × 2`), plus a 1-in-N baseline (default N=600). So
    every `frame` trace in OpenObserve is already a hitch.

Operations nest via a thread-local scope stack: a new `PERF_SCOPE`'s
parent is the top of the stack, so the real hierarchy
(`ui.gpu_build` → `ui` → `frame`) is preserved.

### Identity / device — OTLP Resource

All constant-per-session identity + device data lives in OTLP **Resource**
attributes, set once and attached to every span *and* every metric, sent
once per export request (so the metadata cost amortizes to near-zero — it
is **not** stamped on every span): `service.name`, `session.id` (fresh
UUID per launch, also printed to stdout), `account.id` (once lobby auth
completes), `build.version`, `device.os/gpu/gpu_driver/cpu_cores/ram_mb`,
`display.resolution/viewport/refresh`, `render.ui_path/vsync/fullscreen`.
A few of these can change mid-session (viewport/fullscreen/ui_path on
resize, `account.id` on login) — those bump `session.epoch` so each
epoch's Resource is internally constant.

Because identity/device live in the Resource, the **same** `session.id`
filter narrows both the metrics dashboard and the Traces tab.

### Transport

Export runs off the frame thread: a bounded queue + one background curl
sender doing batched POSTs. Overflow **drops** rather than back-pressuring
the game — telemetry never stalls a frame.

## Run it locally

### 1. OpenObserve in Docker

```bash
docker run -d --name openobserve -p 5080:5080 \
  -e ZO_ROOT_USER_EMAIL="root@example.com" \
  -e ZO_ROOT_USER_PASSWORD="Complexpass#123" \
  -v "$HOME/.openobserve-data:/data" -e ZO_DATA_DIR="/data" \
  public.ecr.aws/zinclabs/openobserve:latest
```

UI: http://localhost:5080 (login `root@example.com` / `Complexpass#123`).
OTLP/HTTP endpoints (OpenObserve v0.91, OTLP/JSON, basic auth):
`POST /api/default/v1/traces` and `/api/default/v1/metrics`.

### 2. Build

```bash
bash clients/silencer/build.sh        # macOS/Linux, default win-ninja → build/
```

### 3. Run with export on

```bash
export SILENCER_PERF_OTLP=http://localhost:5080/api/default
export SILENCER_PERF_OTLP_USER='root@example.com'
export SILENCER_PERF_OTLP_PASS='Complexpass#123'

# windowed (real vsync-bound frame times + GPU/display Resource fields):
clients/silencer/build/Silencer.app/Contents/MacOS/Silencer

# headless, driven to a level_load via the Tutorial:
BIN=clients/silencer/build/Silencer.app/Contents/MacOS/Silencer
"$BIN" --headless --control-port 5170 &
bun clients/cli/index.ts --port 5170 wait_for_state --state MAINMENU --timeout-ms 20000
bun clients/cli/index.ts --port 5170 click --label Tutorial
bun clients/cli/index.ts --port 5170 wait_for_state --state SINGLEPLAYERGAME --timeout-ms 20000
```

The session id is printed at startup:
`[perf] OTLP export -> … | session.id=<uuid>`.

Optional tuning env: `SILENCER_PERF_FRAME_BUDGET_MS` (default 16.67),
`SILENCER_PERF_FRAME_OVER_FACTOR` (default 2.0),
`SILENCER_PERF_FRAME_BASELINE_N` (default 600). Auth alternative to
USER/PASS: `SILENCER_PERF_OTLP_AUTH=<verbatim Authorization header>`.

### 4. View both layers

Streams auto-appear once data arrives.

- **Layer 1 — Dashboards.** A starter dashboard "Silencer Client Perf
  (Layer 1)" ships two panels: `frame_ms_p95` over time, and avg p95
  grouped by `device.gpu` / `device.os`. To rebuild by hand, add a panel
  on the `frame_ms_p95` **metrics** stream with X = `histogram(_timestamp)`,
  Y = `avg(value)`; group the second by `concat(device_gpu,' / ',device_os)`.
- **Layer 2 — Traces tab.** Filter by resource attributes + duration,
  e.g. `operation_name='frame' AND service_device_gpu='metal' AND
  duration > 33000` (duration is µs). Click a trace → flame graph with the
  nested scopes. Roots are `startup`, `level_load`, and over-budget
  `frame`.
- **One session across both layers.** Metrics filter field is
  `session_id`; traces filter field is `service_session_id` (OpenObserve
  prefixes trace Resource attrs with `service_`). Same UUID, both layers.
