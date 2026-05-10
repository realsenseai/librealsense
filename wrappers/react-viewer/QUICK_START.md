# Quick Start: Building & Testing RealSense Viewer

## One-Command Build

```powershell
cd c:\work\librealsense\wrappers\react-viewer
.\build-all.ps1
```

**That's it!** The script will:
✅ Build FastAPI executable (PyInstaller)
✅ Build React UI (Vite)
✅ Build Tauri bundles (MSI + NSIS installers)
✅ Report total build time

---

## Installation & Testing

### Option A: MSI Installer (Recommended)
```powershell
msiexec /i "C:\work\librealsense\wrappers\react-viewer\build\tauri\release\bundle\msi\RealSense Viewer_0.5.0_x64_en-US.msi"
```

### Option B: NSIS Installer
```powershell
& "C:\work\librealsense\wrappers\react-viewer\build\tauri\release\bundle\nsis\RealSense Viewer_0.5.0_x64-setup.exe"
```

Then:
1. Launch **RealSense Viewer** from Start Menu
2. Press **F12** to open Developer Tools
3. Check Console for connection status:
   - ✅ `🖥️ Desktop app detected`
   - ✅ `✅ Socket.IO connected successfully`

---

## Troubleshooting

### Devices Not Showing
1. Open DevTools (F12)
2. Check bottom-right corner for **ApiDiagnostics** panel
3. Click **▶** to expand error details
4. Follow troubleshooting steps

### Build Fails
```powershell
# Full rebuild (cleans artifacts first)
.\build-all.ps1 -Clean
```

### Manual Backend Test
```powershell
cd "C:\Program Files\RealSense Viewer"
.\realsense_api.exe
# Should show: Uvicorn running on http://0.0.0.0:8000
```

---

## Development Workflow

For **development** (hot-reload), keep using:

```powershell
# Terminal 1: FastAPI backend
cd wrappers/rest-api
python main.py

# Terminal 2: React dev server
cd wrappers/react-viewer
npm run dev

# Terminal 3: Tauri dev window (optional)
npm run tauri:dev
```

**For production:** Use the unified `build-all.ps1` script above.

---

## Build Artifacts

After `.\build-all.ps1`:

```
C:\work\librealsense\
├── wrappers/
│   ├── rest-api/
│   │   └── dist/realsense_api/          ← FastAPI executable
│   └── react-viewer/
│       ├── dist/                         ← React build
│       └── build/tauri/release/bundle/
│           ├── msi/                      ← MSI installer ✓
│           └── nsis/                     ← NSIS installer ✓
```

---

## Version Info

- **Tauri**: 1.5.0
- **Rust**: 1.91.1+
- **Node.js**: 18+
- **Python**: 3.13+
- **FastAPI**: 0.104+

