# Desktop Application Build & Development

This directory contains the Tauri configuration and Rust code for building a standalone desktop application that bundles both the React frontend and FastAPI backend.

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

### Install Tauri CLI

```bash
npm run tauri:install
```

This installs `@tauri-apps/cli` and `@tauri-apps/api` as dev dependencies.

### Development Workflow

**Terminal 1: Start FastAPI backend** (runs standalone; Tauri app will not spawn it in dev)
```bash
cd wrappers/rest-api
pip install -r requirements.txt
python main.py
```

**Terminal 2: Start React + Tauri dev mode**
```bash
cd wrappers/react-viewer
npm run tauri:dev
```

This opens a Tauri window with hot-reload for React code changes. The React app will automatically connect to the FastAPI backend on `localhost:8000`.

## Production Build

### Step 1: Build FastAPI Executable

The Tauri build will look for the FastAPI executable in `src-tauri/resources/`.

**Windows:**
```powershell
cd wrappers/rest-api
pip install pyinstaller
./build/build.ps1 -Clean
Copy-Item 'C:\work\librealsense\build\rest-api-dist\realsense_api\realsense_api.exe' -Destination '../react-viewer/src-tauri/resources/'
```

**Linux/macOS:**
```bash
cd wrappers/rest-api
pip install pyinstaller
bash build/build.sh --clean
cp ../../build/rest-api-dist/realsense_api/realsense_api ../react-viewer/src-tauri/resources/
```

### Step 2: Build Tauri Application

```bash
cd wrappers/react-viewer
npm run tauri:build
```

This:
1. Compiles React to `dist/`
2. Bundles everything (React + FastAPI executable + Tauri)
3. Creates platform-specific installers:
   - Windows: `.msi` installer
   - macOS: `.dmg` package
   - Linux: `.AppImage` or `.deb` package

**Output locations:**
- FastAPI exe: `build/rest-api-dist/realsense_api/`
- Tauri installers (MSI/NSIS): `build/tauri-target/release/bundle/`

## Directory Structure

```
src-tauri/
├── src/main.rs              # Tauri window + subprocess spawner
├── tauri.conf.json          # Tauri configuration
├── Cargo.toml               # Rust dependencies
├── build.rs                 # Build script
├── icons/                   # App icons (multiple sizes)
└── resources/               # FastAPI executable (added during build)
    └── realsense_api.exe    # Bundled FastAPI executable

resources/                    # This file documents the build process
```

## Tauri Configuration Details

### `tauri.conf.json`

- **devPath:** Points to Vite dev server (`localhost:5173`)
- **frontendDist:** Production React build directory (`./dist`)
- **beforeDevCommand:** Runs `npm run dev` (starts Vite)
- **beforeBuildCommand:** Runs `npm run build` (compiles React)

### `Cargo.toml` Dependencies

- **tauri:** Main framework
- **tokio:** Async runtime for spawning FastAPI subprocess
- **reqwest:** HTTP client for health checks
- **serde/serde_json:** JSON serialization

## Subprocess Management (Rust)

**On App Startup:**
1. Locate FastAPI executable in `resources/` or bundled path
2. Spawn process with environment variables (`UVICORN_PORT`, `UVICORN_HOST`)
3. Wait for health check (`GET /api/v1/health`)
4. Store process handle in app state

**On App Exit:**
1. Gracefully terminate FastAPI subprocess
2. Clean up resources

**In Development:**
- FastAPI is NOT spawned; you must run it separately
- React app connects to manual FastAPI instance on `localhost:8000`

## Communicating with Tauri from React

Use `@tauri-apps/api` to call Rust commands:

```typescript
import { invoke } from '@tauri-apps/api/tauri'

// Check API status
const status = await invoke('api_status')

// Get the port the API is running on
const port = await invoke('get_api_port')
```

Note: The API client in `src/api/client.ts` already detects Tauri and routes requests appropriately.

## Troubleshooting

### Build errors: "cargo not found"
Install Rust: https://rustup.rs/

### Build errors: "realsense_api executable not found"
Ensure you've built the FastAPI executable and placed it in `src-tauri/resources/`.

### FastAPI subprocess fails to start
- Check that RealSense SDK is installed
- Verify the executable runs standalone: `./realsense_api`
- Check Tauri logs in `~/.config/realsense-viewer/`

### React can't connect to API
- Verify FastAPI is running on `localhost:8000`
- Check browser console for connection errors
- Ensure the API health endpoint exists: `GET /api/v1/health`

## Cross-Platform Considerations

### Windows
- Executable: `realsense_api.exe`
- No console window in production (Tauri suppresses it)
- Installer: `.msi` file

### macOS
- Executable: `realsense_api` (no extension)
- Code signing may be required for distribution
- Bundle: `.dmg` file
- Consider notarization for App Store distribution

### Linux
- Executable: `realsense_api` (no extension)
- Build on target platform for best compatibility
- Installer: `.AppImage` (single file, portable)

## Next Steps

1. **Implement icon assets** for all platforms (32x32, 128x128, 256x256, etc.)
2. **Add CI/CD workflows** (GitHub Actions) to build for all platforms
3. **Implement auto-updates** via Tauri's updater
4. **Add app signing/notarization** for macOS and Windows
5. **Test on all platforms** before release

## References

- Tauri Docs: https://tauri.app/
- PyInstaller Docs: https://pyinstaller.org/
- Rust Book: https://doc.rust-lang.org/book/
