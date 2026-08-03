import { Channel, invoke } from "@tauri-apps/api/core";

// Shapes mirror the Rust structs in src-tauri/src/commands.rs. Config keys are
// the shared on-disk contract; do not rename.
type Server = { name: string; host: string; port: number };

type Config = {
  channel: string;
  installed_version: string;
  install_dir: string;
  game_binary: string;
  servers: Server[];
  last_server: string;
  manifest_url_stable: string;
  manifest_url_nightly: string;
  announcements_url: string;
};

type Manifest = {
  version: string;
  macos_url: string;
  macos_sha256: string;
  windows_url: string;
  windows_sha256: string;
};

type Announcement = { title: string; body: string; date: string; pinned: boolean };
type DownloadProgress = { downloaded: number; total: number };

let config: Config;
let manifest: Manifest | null = null;
let selectedServer: Server | null = null;
const latencies = new Map<string, { text: string; offline: boolean }>();

const el = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;

function escapeHtml(s: string): string {
  return s.replace(
    /[&<>"']/g,
    (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]!,
  );
}

async function init(): Promise<void> {
  config = await invoke<Config>("load_config");
  renderChannel();
  renderServers();
  await Promise.all([refreshManifest(), refreshNews(), pingAll(), refreshPlay()]);
}

// ---- Channel ----
function renderChannel(): void {
  document.querySelectorAll<HTMLButtonElement>(".chan-btn").forEach((btn) => {
    const c = btn.dataset.channel!;
    btn.classList.toggle("active", c === config.channel);
    btn.onclick = async () => {
      if (config.channel === c) return;
      config.channel = c;
      await invoke("save_config", { config });
      renderChannel();
      await refreshManifest();
    };
  });
}

// ---- Manifest / update ----
function manifestUrl(): string {
  return config.channel === "nightly" ? config.manifest_url_nightly : config.manifest_url_stable;
}

async function refreshManifest(): Promise<void> {
  const status = el("update-status");
  const btn = el<HTMLButtonElement>("update-btn");
  btn.hidden = true;
  el("progress").hidden = true;
  status.className = "update-status";
  status.textContent = "Checking for updates…";

  try {
    manifest = await invoke<Manifest>("fetch_manifest", { url: manifestUrl() });
  } catch {
    manifest = null;
    status.classList.add("warn");
    status.textContent = `Manifest unavailable for ${config.channel}`;
    return;
  }

  if (!config.installed_version) {
    status.textContent = `Not installed — ${manifest.version} available`;
    btn.textContent = "INSTALL";
    btn.hidden = false;
    btn.onclick = doUpdate;
  } else if (manifest.version !== config.installed_version) {
    status.textContent = `Update available: ${manifest.version}`;
    btn.textContent = "UPDATE";
    btn.hidden = false;
    btn.onclick = doUpdate;
  } else {
    status.textContent = `Up to date — ${config.installed_version}`;
  }
}

async function doUpdate(): Promise<void> {
  if (!manifest) return;
  const btn = el<HTMLButtonElement>("update-btn");
  const status = el("update-status");
  const progress = el("progress");
  const bar = el("progress-bar");

  btn.disabled = true;
  status.className = "update-status";
  status.textContent = "Downloading…";
  progress.hidden = false;
  bar.style.width = "0%";

  const onProgress = new Channel<DownloadProgress>();
  onProgress.onmessage = (p) => {
    if (p.total > 0) {
      const pct = Math.round((p.downloaded / p.total) * 100);
      bar.style.width = `${pct}%`;
      status.textContent = `Downloading… ${pct}%`;
    } else {
      status.textContent = `Downloading… ${(p.downloaded / 1e6).toFixed(1)} MB`;
    }
  };

  try {
    const version = await invoke<string>("apply_update", {
      manifest,
      installDir: config.install_dir,
      onProgress,
    });
    config.installed_version = version;
    await invoke("save_config", { config });
    progress.hidden = true;
    btn.hidden = true;
    status.textContent = `Up to date — ${version}`;
    await refreshPlay();
  } catch (e) {
    progress.hidden = true;
    status.className = "update-status warn";
    status.textContent = `Update failed: ${e}`;
  } finally {
    btn.disabled = false;
  }
}

// ---- Servers ----
function renderServers(): void {
  const list = el("server-list");
  list.innerHTML = "";
  selectedServer =
    config.servers.find((s) => s.name === config.last_server) ?? config.servers[0] ?? null;

  for (const s of config.servers) {
    const lat = latencies.get(s.name);
    const li = document.createElement("li");
    li.className = "server" + (selectedServer?.name === s.name ? " selected" : "");
    li.dataset.server = s.name;
    li.innerHTML = `
      <span class="server-name">${escapeHtml(s.name)}</span>
      <span class="server-host">${escapeHtml(s.host)}:${s.port}</span>
      <span class="latency${lat?.offline ? " offline" : ""}">${escapeHtml(lat?.text ?? "…")}</span>`;
    li.onclick = () => selectServer(s);
    list.appendChild(li);
  }
}

function selectServer(s: Server): void {
  selectedServer = s;
  config.last_server = s.name;
  void invoke("save_config", { config });
  document.querySelectorAll<HTMLElement>(".server").forEach((row) => {
    row.classList.toggle("selected", row.dataset.server === s.name);
  });
  void refreshPlay();
}

async function pingAll(): Promise<void> {
  await Promise.all(
    config.servers.map(async (s) => {
      const ms = await invoke<number | null>("ping_server", { host: s.host, port: s.port });
      const offline = ms === null;
      const entry = { text: offline ? "OFFLINE" : `${ms} ms`, offline };
      latencies.set(s.name, entry);
      const row = document.querySelector<HTMLElement>(
        `.server[data-server="${CSS.escape(s.name)}"] .latency`,
      );
      if (row) {
        row.textContent = entry.text;
        row.classList.toggle("offline", offline);
      }
    }),
  );
}

// ---- Play ----
async function refreshPlay(): Promise<void> {
  const btn = el<HTMLButtonElement>("play-btn");
  const hint = el("play-hint");
  const hasBinary = config.game_binary
    ? await invoke<boolean>("path_exists", { path: config.game_binary })
    : false;

  btn.disabled = !hasBinary || !selectedServer;
  btn.onclick = doPlay;

  if (!config.game_binary) {
    hint.textContent = "No game installed. Run the update, then set game_binary in the config.";
  } else if (!hasBinary) {
    hint.textContent = `Game binary not found:\n${config.game_binary}`;
  } else if (!selectedServer) {
    hint.textContent = "Select a server to play.";
  } else {
    hint.textContent = "";
  }
}

async function doPlay(): Promise<void> {
  if (!selectedServer || !config.game_binary) return;
  try {
    await invoke("launch_game", {
      binary: config.game_binary,
      host: selectedServer.host,
      port: selectedServer.port,
    });
  } catch (e) {
    el("play-hint").textContent = `Launch failed: ${e}`;
  }
}

// ---- News ----
async function refreshNews(): Promise<void> {
  const list = el("news-list");
  let items: Announcement[] = [];
  try {
    items = await invoke<Announcement[]>("fetch_announcements", { url: config.announcements_url });
  } catch {
    items = [];
  }

  if (items.length === 0) {
    list.innerHTML = `<div class="empty">No announcements</div>`;
    return;
  }

  items.sort((a, b) => {
    if (a.pinned !== b.pinned) return a.pinned ? -1 : 1;
    return (b.date || "").localeCompare(a.date || "");
  });

  list.innerHTML = "";
  for (const a of items) {
    const article = document.createElement("article");
    article.className = "news-item" + (a.pinned ? " pinned" : "");
    article.innerHTML = `
      <div class="news-head">
        <h3>${escapeHtml(a.title)}</h3>
        ${a.pinned ? '<span class="pin">PINNED</span>' : ""}
        <time>${escapeHtml(a.date)}</time>
      </div>
      <p>${escapeHtml(a.body)}</p>`;
    list.appendChild(article);
  }
}

void init();
