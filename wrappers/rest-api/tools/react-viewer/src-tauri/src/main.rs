// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::process::Child;
use std::sync::{Arc, Mutex};
use tauri::{State, Manager, AppHandle};

#[cfg(not(debug_assertions))]
use std::process::{Command, Stdio};
#[cfg(not(debug_assertions))]
use std::path::PathBuf;
#[cfg(not(debug_assertions))]
use std::io::{BufRead, BufReader};

// Global state to hold FastAPI subprocess
#[derive(Clone)]
struct AppState {
    api_process: Arc<Mutex<Option<Child>>>,
    api_port: Arc<Mutex<u16>>,
    backend_logs: Arc<Mutex<Vec<String>>>,
}

fn main() {
    let app_state = AppState {
        api_process: Arc::new(Mutex::new(None)),
        api_port: Arc::new(Mutex::new(8000)),
        backend_logs: Arc::new(Mutex::new(Vec::new())),
    };

    tauri::Builder::default()
        .manage(app_state)
        .setup(|app| {
            // Start FastAPI subprocess on app startup (in production only)
            #[cfg(not(debug_assertions))]
            {
                let state = app.state::<AppState>();
                let api_process = Arc::clone(&state.api_process);
                let api_port = Arc::clone(&state.api_port);
                let backend_logs = Arc::clone(&state.backend_logs);
                let app_handle = app.app_handle();

                tauri::async_runtime::spawn(async move {
                    start_api_server(api_process, api_port, backend_logs, app_handle).await;
                });
            }

            Ok(())
        })
        .on_window_event(|event| match event.event() {
            tauri::WindowEvent::CloseRequested { api, .. } => {
                api.prevent_close();
                let app_handle = event.window().app_handle();
                let app_state = app_handle.state::<AppState>();
                
                // Gracefully shutdown API server
                let mut process = app_state.api_process.lock().unwrap();
                if let Some(mut child) = process.take() {
                    let _ = child.kill();
                    let _ = child.wait();
                }
                std::process::exit(0);
            }
            _ => {}
        })
        .invoke_handler(tauri::generate_handler![
            api_status,
            get_api_port,
            test_api_connection,
            get_backend_logs,
            get_backend_status,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

/// Start the FastAPI backend subprocess
#[cfg(not(debug_assertions))]
async fn start_api_server(api_process: Arc<Mutex<Option<Child>>>, _api_port: Arc<Mutex<u16>>, backend_logs: Arc<Mutex<Vec<String>>>, app_handle: AppHandle) {
    // Determine the path to the FastAPI executable
    let exe_name = if cfg!(target_os = "windows") {
        "realsense_api.exe"
    } else {
        "realsense_api"
    };

    // Try to find the executable in bundled resources or current directory
    let exe_path = find_api_executable(exe_name, &app_handle);
    
    match exe_path {
        Some(path) => {
            let log_msg = format!("[Tauri] Found FastAPI executable at: {:?}", path);
            println!("{}", log_msg);
            backend_logs.lock().unwrap().push(log_msg);
            
            match spawn_api_process(&path, 8000, Arc::clone(&backend_logs)) {
                Ok(child) => {
                    {
                        let mut process = api_process.lock().unwrap();
                        *process = Some(child);
                    }
                    let log_msg = "[Tauri] FastAPI server started on port 8000".to_string();
                    println!("{}", log_msg);
                    backend_logs.lock().unwrap().push(log_msg);

                    // Wait for server to be ready
                    wait_for_api_ready(8000, Arc::clone(&backend_logs)).await;
                }
                Err(e) => {
                    let error_msg = format!("[Tauri] Failed to spawn FastAPI process: {}", e);
                    eprintln!("{}", error_msg);
                    backend_logs.lock().unwrap().push(error_msg);
                    
                    let path_msg = format!("[Tauri] Path: {:?}", path);
                    eprintln!("{}", path_msg);
                    backend_logs.lock().unwrap().push(path_msg);
                    
                    let perm_msg = "[Tauri] Ensure the executable has execute permissions".to_string();
                    eprintln!("{}", perm_msg);
                    backend_logs.lock().unwrap().push(perm_msg);
                }
            }
        }
        None => {
            let error_msg = "[Tauri] CRITICAL: FastAPI executable not found!".to_string();
            eprintln!("{}", error_msg);
            backend_logs.lock().unwrap().push(error_msg);
            
            let expected_msg = "[Tauri] Expected at: bundled resources (realsense_api/), ../build/tauri-resources/realsense_api/, ./realsense_api/, ./resources/realsense_api/, ../rest-api/dist/realsense_api/".to_string();
            eprintln!("{}", expected_msg);
            backend_logs.lock().unwrap().push(expected_msg);
        }
    }
}

#[cfg(not(debug_assertions))]
/// Find the FastAPI executable in bundled resources
fn find_api_executable(exe_name: &str, app_handle: &AppHandle) -> Option<PathBuf> {
    // In production, try to get the resource directory from the app handle
    if let Some(resource_dir) = app_handle.path_resolver().resource_dir() {
        let candidates = vec![
            resource_dir.join("realsense_api").join(exe_name),                // packaged dir (preferred)
            resource_dir.join(exe_name),                                       // legacy root location
            resource_dir.join("resources").join("realsense_api").join(exe_name), // safety fallback if resources are nested
        ];

        for candidate in candidates {
            if candidate.exists() {
                println!("[Tauri] Found FastAPI executable in bundled resources: {:?}", candidate);
                return Some(candidate);
            }
        }
    }
    
    // Fallback: Try relative paths for development/testing
    let potential_paths = vec![
        PathBuf::from("realsense_api").join(exe_name),
        PathBuf::from("./resources/realsense_api").join(exe_name),
        PathBuf::from("../build/tauri-resources/realsense_api").join(exe_name),
        PathBuf::from("../rest-api/dist/realsense_api").join(exe_name),
    ];

    for path in potential_paths {
        if path.exists() {
            println!("[Tauri] Found FastAPI executable at: {:?}", path);
            return Some(path);
        }
    }

    eprintln!("[Tauri] FastAPI executable '{}' not found in any location", exe_name);
    None
}

#[cfg(not(debug_assertions))]
/// Spawn the FastAPI process with environment variables
fn spawn_api_process(path: &std::path::Path, port: u16, backend_logs: Arc<Mutex<Vec<String>>>) -> Result<Child, std::io::Error> {
    println!("[Tauri] Spawning FastAPI process from: {:?}", path);
    
    let mut cmd = Command::new(path);
    cmd.env("UVICORN_PORT", port.to_string())
        .env("UVICORN_HOST", "127.0.0.1")
        .env("PYTHONUNBUFFERED", "1")
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    #[cfg(not(debug_assertions))]
    {
        // In production, suppress console window on Windows
        #[cfg(target_os = "windows")]
        {
            use std::os::windows::process::CommandExt;
            cmd.creation_flags(0x08000000); // CREATE_NO_WINDOW
        }
    }

    match cmd.spawn() {
        Ok(mut child) => {
            let pid = child.id();
            println!("[Tauri] FastAPI process spawned with PID: {}", pid);
            
            // Capture stdout in background thread
            if let Some(stdout) = child.stdout.take() {
                let logs = Arc::clone(&backend_logs);
                std::thread::spawn(move || {
                    let reader = BufReader::new(stdout);
                    for line in reader.lines() {
                        if let Ok(line) = line {
                            let log_msg = format!("[Backend STDOUT] {}", line);
                            println!("{}", log_msg);
                            logs.lock().unwrap().push(log_msg);
                        }
                    }
                });
            }
            
            // Capture stderr in background thread
            if let Some(stderr) = child.stderr.take() {
                let logs = Arc::clone(&backend_logs);
                std::thread::spawn(move || {
                    let reader = BufReader::new(stderr);
                    for line in reader.lines() {
                        if let Ok(line) = line {
                            let log_msg = format!("[Backend STDERR] {}", line);
                            eprintln!("{}", log_msg);
                            logs.lock().unwrap().push(log_msg);
                        }
                    }
                });
            }
            
            Ok(child)
        }
        Err(e) => {
            eprintln!("[Tauri] Failed to spawn process: {}", e);
            eprintln!("[Tauri] Executable: {:?}", path);
            Err(e)
        }
    }
}

#[cfg(not(debug_assertions))]
/// Wait for the API server to be ready (health check)
async fn wait_for_api_ready(port: u16, backend_logs: Arc<Mutex<Vec<String>>>) {
    let max_retries = 60; // ~30 seconds with 500ms intervals
    let mut retries = 0;

    println!("[Tauri] Waiting for FastAPI server to be ready on port {}...", port);

    loop {
        if retries >= max_retries {
            let timeout_msg = format!("[Tauri] CRITICAL: FastAPI server did not respond after {} retries (~30 seconds)", max_retries);
            eprintln!("{}", timeout_msg);
            backend_logs.lock().unwrap().push(timeout_msg);
            
            let diag_msg = format!("[Tauri] DIAGNOSTICS: Check if realsense_api.exe is running, verify RealSense SDK, ensure USB devices, check port {} conflicts", port);
            eprintln!("{}", diag_msg);
            backend_logs.lock().unwrap().push(diag_msg);
            break;
        }

        match reqwest::Client::new()
            .get(&format!("http://127.0.0.1:{}/api/v1/health", port))
            .timeout(std::time::Duration::from_secs(2))
            .send()
            .await
        {
            Ok(resp) if resp.status().is_success() => {
                let ready_msg = "[Tauri] ✓ FastAPI server is ready!".to_string();
                println!("{}", ready_msg);
                backend_logs.lock().unwrap().push(ready_msg);
                break;
            }
            Ok(resp) => {
                if retries % 10 == 0 {
                    println!("[Tauri] Server responded with status {}, waiting...", resp.status());
                }
                retries += 1;
                tokio::time::sleep(std::time::Duration::from_millis(500)).await;
            }
            Err(e) => {
                if retries % 10 == 0 {
                    println!("[Tauri] Health check attempt {} failed: {} (waiting...)", retries, e);
                }
                retries += 1;
                tokio::time::sleep(std::time::Duration::from_millis(500)).await;
            }
        }
    }
}

/// Check if the API server is running
#[tauri::command]
fn api_status() -> String {
    "ok".to_string()
}

/// Get the port the API server is running on
#[tauri::command]
fn get_api_port(state: State<AppState>) -> u16 {
    *state.api_port.lock().unwrap()
}

/// Diagnostic command: Test if API server is accessible
#[tauri::command]
async fn test_api_connection() -> Result<String, String> {
    match reqwest::Client::new()
        .get("http://127.0.0.1:8000/api/v1/health")
        .timeout(std::time::Duration::from_secs(5))
        .send()
        .await
    {
        Ok(resp) => {
            if resp.status().is_success() {
                Ok("✅ API server is accessible and responding".to_string())
            } else {
                Err(format!("❌ API server responded with status: {}", resp.status()))
            }
        }
        Err(e) => Err(format!("❌ Cannot reach API server: {}", e)),
    }
}

/// Get backend logs for diagnostics
#[tauri::command]
fn get_backend_logs(state: State<AppState>) -> Vec<String> {
    state.backend_logs.lock().unwrap().clone()
}

/// Get detailed backend status for diagnostics
#[tauri::command]
fn get_backend_status(state: State<AppState>) -> serde_json::Value {
    let mut process = state.api_process.lock().unwrap();
    let port = *state.api_port.lock().unwrap();
    let logs = state.backend_logs.lock().unwrap().clone();
    
    // Check if process is actually still running (not just if we have a handle)
    let is_running = if let Some(ref mut child) = *process {
        match child.try_wait() {
            Ok(Some(status)) => {
                // Process has exited
                let exit_msg = format!("[Tauri] Backend process exited with status: {}", status);
                eprintln!("{}", exit_msg);
                drop(process); // Release lock before modifying logs
                state.backend_logs.lock().unwrap().push(exit_msg);
                false
            }
            Ok(None) => true, // Still running
            Err(e) => {
                let err_msg = format!("[Tauri] Error checking process status: {}", e);
                eprintln!("{}", err_msg);
                drop(process);
                state.backend_logs.lock().unwrap().push(err_msg);
                false
            }
        }
    } else {
        false
    };
    
    serde_json::json!({
        "is_running": is_running,
        "port": port,
        "log_count": logs.len(),
        "last_logs": logs.iter().rev().take(10).cloned().collect::<Vec<_>>(),
    })
}
