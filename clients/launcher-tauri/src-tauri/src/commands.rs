use serde::{Deserialize, Serialize};
use std::io::Write;
use std::net::ToSocketAddrs;
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};
use tauri::ipc::Channel;

// ---------------------------------------------------------------------------
// Config — the on-disk shape is a shared contract with the competing launcher.
// Path and keys are fixed: ~/.config/silencer-launcher/launcher.json.
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Server {
    pub name: String,
    pub host: String,
    pub port: u16,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub channel: String,
    pub installed_version: String,
    pub install_dir: String,
    pub game_binary: String,
    pub servers: Vec<Server>,
    pub last_server: String,
    pub manifest_url_stable: String,
    pub manifest_url_nightly: String,
    pub announcements_url: String,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            channel: "stable".into(),
            installed_version: String::new(),
            install_dir: default_install_dir().to_string_lossy().into_owned(),
            game_binary: String::new(),
            servers: vec![Server {
                name: "Official".into(),
                host: "lobby.arsiamons.com".into(),
                port: 517,
            }],
            last_server: String::new(),
            manifest_url_stable:
                "https://github.com/Arsia-Mons/Silencer/releases/latest/download/update.json".into(),
            manifest_url_nightly:
                "https://github.com/Arsia-Mons/Silencer/releases/download/latest/update.json".into(),
            announcements_url: "https://admin.arsiamons.com/api/announcements".into(),
        }
    }
}

fn config_dir() -> PathBuf {
    let home = std::env::var_os("HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."));
    home.join(".config").join("silencer-launcher")
}

fn config_path() -> PathBuf {
    config_dir().join("launcher.json")
}

fn default_install_dir() -> PathBuf {
    config_dir().join("current")
}

fn write_config(cfg: &Config) -> Result<(), String> {
    std::fs::create_dir_all(config_dir()).map_err(|e| e.to_string())?;
    let data = serde_json::to_string_pretty(cfg).map_err(|e| e.to_string())?;
    std::fs::write(config_path(), data).map_err(|e| e.to_string())
}

/// Read the config, creating it with defaults on first run. A corrupt file is
/// replaced with defaults rather than surfaced as an error — the launcher must
/// always come up.
#[tauri::command]
pub fn load_config() -> Config {
    if let Ok(data) = std::fs::read_to_string(config_path()) {
        if let Ok(cfg) = serde_json::from_str::<Config>(&data) {
            return cfg;
        }
    }
    let cfg = Config::default();
    let _ = write_config(&cfg);
    cfg
}

#[tauri::command]
pub fn save_config(config: Config) -> Result<(), String> {
    write_config(&config)
}

// ---------------------------------------------------------------------------
// Update manifest — shape mirrors services/lobby/update.go.
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Manifest {
    pub version: String,
    pub macos_url: String,
    pub macos_sha256: String,
    pub windows_url: String,
    pub windows_sha256: String,
}

#[tauri::command]
pub async fn fetch_manifest(url: String) -> Result<Manifest, String> {
    let resp = reqwest::get(&url).await.map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status().as_u16()));
    }
    let text = resp.text().await.map_err(|e| e.to_string())?;
    serde_json::from_str::<Manifest>(&text).map_err(|e| e.to_string())
}

// ---------------------------------------------------------------------------
// Announcements.
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Announcement {
    #[serde(default)]
    pub title: String,
    #[serde(default)]
    pub body: String,
    #[serde(default)]
    pub date: String,
    #[serde(default)]
    pub pinned: bool,
}

#[tauri::command]
pub async fn fetch_announcements(url: String) -> Result<Vec<Announcement>, String> {
    let resp = reqwest::get(&url).await.map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("HTTP {}", resp.status().as_u16()));
    }
    let text = resp.text().await.map_err(|e| e.to_string())?;
    serde_json::from_str::<Vec<Announcement>>(&text).map_err(|e| e.to_string())
}

// ---------------------------------------------------------------------------
// Server latency — TCP connect time, "offline" (None) on timeout.
// ---------------------------------------------------------------------------

#[tauri::command]
pub async fn ping_server(host: String, port: u16) -> Option<u64> {
    tauri::async_runtime::spawn_blocking(move || {
        let start = Instant::now();
        let addrs = (host.as_str(), port).to_socket_addrs().ok()?;
        for addr in addrs {
            if std::net::TcpStream::connect_timeout(&addr, Duration::from_secs(3)).is_ok() {
                return Some(start.elapsed().as_millis() as u64);
            }
        }
        None
    })
    .await
    .ok()
    .flatten()
}

// ---------------------------------------------------------------------------
// Update: download the platform zip with progress, verify SHA-256, extract.
// ---------------------------------------------------------------------------

#[derive(Clone, Serialize)]
pub struct DownloadProgress {
    pub downloaded: u64,
    pub total: u64, // 0 when the server sends no Content-Length
}

/// Download this platform's zip into a temp file (streaming the SHA-256 as we
/// go), verify the hash against the manifest, and extract into `install_dir`.
/// On a hash mismatch the download is discarded and an error is returned; the
/// caller persists `installed_version` only after this resolves Ok.
#[tauri::command]
pub async fn apply_update(
    manifest: Manifest,
    install_dir: String,
    on_progress: Channel<DownloadProgress>,
) -> Result<String, String> {
    let (url, expected_sha) = if cfg!(target_os = "windows") {
        (manifest.windows_url.clone(), manifest.windows_sha256.clone())
    } else {
        (manifest.macos_url.clone(), manifest.macos_sha256.clone())
    };
    if url.is_empty() {
        return Err("manifest has no download URL for this platform".into());
    }

    let mut resp = reqwest::get(&url).await.map_err(|e| e.to_string())?;
    if !resp.status().is_success() {
        return Err(format!("download failed: HTTP {}", resp.status().as_u16()));
    }
    let total = resp.content_length().unwrap_or(0);

    let tmp_path =
        std::env::temp_dir().join(format!("silencer-update-{}.zip", std::process::id()));
    let mut file = std::fs::File::create(&tmp_path).map_err(|e| e.to_string())?;
    let mut hasher = <sha2::Sha256 as sha2::Digest>::new();
    let mut downloaded: u64 = 0;
    while let Some(chunk) = resp.chunk().await.map_err(|e| e.to_string())? {
        sha2::Digest::update(&mut hasher, &chunk);
        file.write_all(&chunk).map_err(|e| e.to_string())?;
        downloaded += chunk.len() as u64;
        let _ = on_progress.send(DownloadProgress { downloaded, total });
    }
    file.flush().map_err(|e| e.to_string())?;
    drop(file);

    let actual_sha = hex_lower(&sha2::Digest::finalize(hasher));
    if !actual_sha.eq_ignore_ascii_case(expected_sha.trim()) {
        let _ = std::fs::remove_file(&tmp_path);
        return Err(format!(
            "hash mismatch: manifest {expected_sha}, downloaded {actual_sha}"
        ));
    }

    let dir = PathBuf::from(&install_dir);
    let tmp_for_extract = tmp_path.clone();
    tauri::async_runtime::spawn_blocking(move || -> Result<(), String> {
        std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
        let f = std::fs::File::open(&tmp_for_extract).map_err(|e| e.to_string())?;
        let mut archive = zip::ZipArchive::new(f).map_err(|e| e.to_string())?;
        archive.extract(&dir).map_err(|e| e.to_string())?;
        Ok(())
    })
    .await
    .map_err(|e| e.to_string())??;

    let _ = std::fs::remove_file(&tmp_path);
    Ok(manifest.version)
}

fn hex_lower(bytes: &[u8]) -> String {
    let mut s = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        s.push_str(&format!("{b:02x}"));
    }
    s
}

// ---------------------------------------------------------------------------
// Launch.
// ---------------------------------------------------------------------------

#[tauri::command]
pub fn path_exists(path: String) -> bool {
    !path.is_empty() && Path::new(&path).exists()
}

/// Spawn the game detached with the selected server's lobby flags. The child
/// is not awaited, so it outlives the launcher.
#[tauri::command]
pub fn launch_game(binary: String, host: String, port: u16) -> Result<(), String> {
    if binary.is_empty() || !Path::new(&binary).exists() {
        return Err("game binary not found".into());
    }
    std::process::Command::new(&binary)
        .arg("--lobby-host")
        .arg(&host)
        .arg("--lobby-port")
        .arg(port.to_string())
        .spawn()
        .map(|_| ())
        .map_err(|e| e.to_string())
}
