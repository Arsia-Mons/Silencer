// Silencer launcher (Tauri prototype). All network I/O, hashing, zip
// extraction, TCP ping, config, and process spawning live in `commands`;
// the webview is view + `invoke` only, which sidesteps webview CORS entirely.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            commands::load_config,
            commands::save_config,
            commands::fetch_manifest,
            commands::fetch_announcements,
            commands::ping_server,
            commands::apply_update,
            commands::launch_game,
            commands::path_exists,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
