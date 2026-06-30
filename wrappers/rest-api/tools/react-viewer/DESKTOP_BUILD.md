# Desktop Application — Internals & Advanced Build

This document is the **reference** companion to the [README](README.md). The README
covers the everyday "build it now" path via the helper scripts; this file covers what
actually happens under the hood, the Tauri / Rust internals, and platform-specific
notes.

For routine builds, just use:
- `./build-all.sh` (Linux / macOS)
- `.\build-all.ps1` (Windows)

…and skip to the [Tauri Configuration Details](#tauri-configuration-details) and
[Subprocess Management](#subprocess-management-rust) sections below.

## Architecture

```
Tauri (Rust)
├── Spawns FastAPI subprocess on app startup (production)
├── Manages lifecycle (graceful shutdown)
├── Provides IPC commands to React frontend
└── Bundles both React dist/ and FastAPI executable

React Frontend
├── Detects Tauri environment
├── Routes API requests to localhost:8000
└── Works identically in browser mode

FastAPI Backend
├── Runs as subprocess (production) or separate process (dev)
├── Responds to requests from React frontend
└── Manages WebRTC, RealSense SDK, Socket.IO
```

## Development Setup

### Prerequisites

- Node.js 18+
- Rust 1.56+ (install from https://rustup.rs/)
- Python 3.8+ (for FastAPI backend)
- **Linux only:** the apt prerequisites listed in the README
  (`libwebkit2gtk-4.0-dev`, `libgtk-3-dev`, `libsoup2.4-dev`,
  `libayatana-appindicator3-dev`, `librsvg2-dev`, `libssl-dev`,
  `pkg-config`, `build-essential`).

### Install Tauri CLI

```bash
npm run tauri:install
```

This installs `@tauri-apps/cli` and `@tauri-apps/api` as dev dependencies.

### Development Workflow (hot-reload)

In dev mode, Tauri does **not** spawn the FastAPI subprocess — you run it yourself in
a separate terminal. This lets you iterate on the backend independently.

**Terminal 1: Start the FastAPI backend**
```bash
cd wrappers/rest-api
python3 install.py
python3 main.py
```

**Terminal 2: Start React + Tauri dev mode**
```bash
cd wrappers/rest-api/tools/react-viewer
npm run tauri:dev
```

This opens a native Tauri window with hot-reload for React code. The React app
connects to the manual FastAPI instance on `localhost:8000`.

## Production Build

The recommended path is the helper script (`build-all.sh` / `build-all.ps1`) — see the
[README's Production Build section](README.md#production-build) for usage and
prerequisites. The script does three things:

1. Runs PyInstaller on `wrappers/rest-api/main.py` to produce a self-contained
   `realsense_api(.exe)` binary plus its `_internal/` runtime directory.
2. Stages the bundle at `<repo-root>/build/tauri-resources/realsense_api/` so Tauri
   can pick it up via the `resources` entry in `tauri.conf.json`.
3. Runs `npm run build` (Vite → `dist/`) and `npm run tauri:build` (Cargo + bundler)
   to produce the platform-native installer(s).

If you need to invoke the steps by hand (debugging, partial rebuild, custom
toolchain), the equivalent commands are:

```bash
# 1. Build the FastAPI executable
cd wrappers/rest-api
python3 -m PyInstaller main.py --name realsense_api \
  --distpath ../../build/rest-api-dist \
  --workpath ../../build/rest-api-work -y

# 2. Stage it for Tauri
mkdir -p ../../build/tauri-resources
rm -rf ../../build/tauri-resources/realsense_api
cp -r ../../build/rest-api-dist/realsense_api ../../build/tauri-resources/

# 3. Build React + Tauri bundle
cd tools/react-viewer
npm run build
CARGO_TARGET_DIR=../../../../build/tauri-target npm run tauri:build
```

**Output locations** (relative to the repository root):
- FastAPI bundle:        `build/rest-api-dist/realsense_api/`
- Staged for Tauri:      `build/tauri-resources/realsense_api/`
- Cargo build output:    `build/tauri-target/release/`
- Native installers:     `build/tauri-target/release/bundle/`

## Directory Structure

```
src-tauri/
├── src/main.rs              # Tauri window + subprocess spawner
├── tauri.conf.json          # Tauri configuration
├── Cargo.toml               # Rust dependencies
├── build.rs                 # Build script
├── icons/                   # App icons (multiple sizes)
└── resources/               # (legacy in-source location, no longer used)
```

The FastAPI bundle is staged out-of-source under `<repo-root>/build/tauri-resources/`
to keep the working tree clean. `tauri.conf.json` references it via a relative path
in its `bundle.resources` array.

## Tauri Configuration Details

### `tauri.conf.json`

Key fields used in this project:

- **`build.devPath`** — Vite dev server URL (`http://localhost:3000`).
- **`build.distDir`** — production React build directory (`../dist`).
- **`build.beforeDevCommand`** — runs `npm run dev` before launching dev mode.
- **`build.beforeBuildCommand`** — runs `npm run build` before bundling.
- **`tauri.bundle.resources`** — points to the staged FastAPI bundle so it gets
  packaged into the installer.
- **`tauri.bundle.icon`** — list of icon files (PNG / ICO / ICNS) reused from
  `common/res/`.
- **`tauri.bundle.deb.depends`** — runtime `.deb` package dependencies
  (`libwebkit2gtk-4.0-37`, `libgtk-3-0`).

### `Cargo.toml` Dependencies

- **tauri** — main framework
- **tokio** — async runtime for spawning the FastAPI subprocess
- **reqwest** — HTTP client for health checks
- **serde / serde_json** — JSON serialization

## Subprocess Management (Rust)

**On app startup (production only):**
1. Locate the FastAPI executable in the bundled resource directory (Tauri exposes
   this via `app.path_resolver().resource_dir()`).
2. Spawn the process with environment variables (`UVICORN_PORT`, `UVICORN_HOST`,
   `PYTHONUNBUFFERED`).
3. Capture stdout/stderr in background threads (visible via the in-app
   ApiDiagnostics panel).
4. Poll `GET /api/v1/health` on `localhost:8000` until it returns 200 (max ~30 s),
   then yield to the React UI.

**On app exit:**
1. Send SIGKILL/TerminateProcess to the FastAPI subprocess.
2. `wait()` on the handle and `std::process::exit(0)`.

**In development:**
- The Rust subprocess spawner is gated on `#[cfg(not(debug_assertions))]` and is
  disabled, so the FastAPI server is never auto-spawned. Run it yourself with
  `python3 main.py` (see the dev-mode section above).

## Calling Rust from React

Use `@tauri-apps/api` to invoke Rust commands:

```typescript
import { invoke } from '@tauri-apps/api/tauri'

const status = await invoke('api_status')
const port   = await invoke('get_api_port')
const logs   = await invoke('get_backend_logs') as string[]
```

The API client in `src/api/client.ts` already detects Tauri and routes requests
appropriately, so you don't need to wire this up for normal API calls — it's only
needed for Tauri-specific commands (diagnostics, etc.).

## Cross-Platform Considerations

### Windows
- Executable: `realsense_api.exe`
- No console window in production (Tauri suppresses it via `creation_flags(0x08000000)`)
- Installers: `.msi` (WiX) and `.exe` (NSIS)

### Linux
- Executable: `realsense_api` (no extension)
- Build on the target distro for best compatibility (glibc compatibility is per-distro)
- Installers: `.deb` and `.AppImage`

### macOS
- Executable: `realsense_api` (no extension)
- Code signing required for distribution outside developer machines
- Bundle: `.dmg`
- Consider notarization for App Store distribution

## Troubleshooting

### `failed to get cargo metadata: No such file or directory`
Cargo isn't installed or isn't on `PATH`. Install Rust via https://rustup.rs/ and run
`source $HOME/.cargo/env` in the current shell (and add the same line to `~/.bashrc`
to make it permanent). `build-all.sh` also tries to auto-source `$HOME/.cargo/env`
when it detects `cargo` is missing, but only if rustup put the file there in the
first place.

### `The system library 'libsoup-2.4' required by crate 'soup2-sys' was not found`
Linux Tauri build dep missing. Install the apt prerequisites listed in the README
(`libsoup2.4-dev` + the rest of the WebKitGTK headers).

### `realsense_api executable not found`
The PyInstaller stage didn't run, or its output isn't where Tauri's
`bundle.resources` expects it. Check that
`<repo-root>/build/tauri-resources/realsense_api/realsense_api(.exe)` exists. If you
ran the helper script and it claimed success but the file is missing, scroll up in
its log — PyInstaller errors are easy to miss.

### Tauri build "succeeds" in seconds but produces no installer
`tauri.conf.json` is missing or malformed. Tauri 1.x silently skips bundling when
the config can't be loaded. Verify the file exists in `src-tauri/` and is valid JSON.

### FastAPI subprocess fails to start at runtime
- Check that the RealSense SDK is installed on the host (Tauri can bundle the
  Python `pyrealsense2` module but the SDK's native libraries must be present).
- Verify the bundled executable runs standalone:
  `<repo-root>/build/tauri-resources/realsense_api/realsense_api`.
- Open the in-app ApiDiagnostics panel (bottom-right of the window) for the
  captured stdout/stderr from the subprocess.

### React can't connect to the API
- Verify FastAPI is reachable: `curl http://localhost:8000/api/v1/health`.
- In dev mode, make sure you actually started `python3 main.py` in a second
  terminal — Tauri does not spawn it.

## References

- Tauri Docs: https://tauri.app/
- PyInstaller Docs: https://pyinstaller.org/
- Rust Book: https://doc.rust-lang.org/book/
