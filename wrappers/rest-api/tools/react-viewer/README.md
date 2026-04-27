# RealSense React Viewer

A modern React-based web UI for Intel RealSense cameras, leveraging the REST API backend.

## Features

- **Device Management**: Discover, select, and reset RealSense devices
- **Stream Viewing**: Real-time video streams via WebRTC (Depth, Color, Infrared)
- **Camera Controls**: Adjust exposure, gain, laser power, and other camera options
- **3D Point Cloud**: Interactive point cloud visualization with Three.js
- **IMU Visualization**: Real-time accelerometer and gyroscope graphs
- **Export**: PLY point cloud export and CSV IMU data export

## Prerequisites

- Node.js 18+ and npm
- RealSense REST API server running (see `../rest-api`)

## Quick Start

1. **Install dependencies:**
   ```bash
   cd wrappers/rest-api/tools/react-viewer
   npm install
   ```

2. **Start the REST API server** (in another terminal):
   ```bash
   cd wrappers/rest-api
   pip install -r requirements.txt
   python main.py
   ```

3. **Start the development server:**
   ```bash
   npm run dev
   ```

4. **Open in browser:**
   Navigate to [http://localhost:3000](http://localhost:3000)

## Project Structure

```
react-viewer/
├── src/
│   ├── api/              # API clients
│   │   ├── client.ts     # REST API client
│   │   ├── socket.ts     # Socket.IO client
│   │   ├── webrtc.ts     # WebRTC handler
│   │   └── types.ts      # TypeScript types
│   ├── components/       # React components
│   │   ├── Header.tsx
│   │   ├── DevicePanel.tsx
│   │   ├── StreamViewer.tsx
│   │   ├── ControlsPanel.tsx
│   │   ├── PointCloudViewer.tsx
│   │   └── IMUViewer.tsx
│   ├── store/            # Zustand state management
│   │   └── index.ts
│   ├── App.tsx           # Main application
│   ├── main.tsx          # Entry point
│   └── index.css         # Global styles
├── scripts/
│   └── bundle-for-prod.js  # Production bundler
└── package.json
```

## Available Scripts

| Command | Description |
|---------|-------------|
| `npm run dev` | Start development server with hot reload |
| `npm run build` | Build for production |
| `npm run preview` | Preview production build locally |
| `npm run lint` | Run ESLint |
| `npm run bundle` | Copy build to FastAPI static folder |

## Testing

- `npm test`: Run unit and integration tests (Vitest)
- `npm run test:coverage`: Generate coverage report (HTML/LCOV)
- `npm run test:e2e`: Run Playwright E2E tests (headless)
- First time E2E setup: `npx playwright install`

See detailed instructions in `tests/README.md`.

## Desktop Application (Tauri)

Build a **standalone cross-platform desktop app** for Windows, macOS, and Linux that bundles both the React UI and FastAPI backend.

### Quick Start

Prerequisites: Rust (install from https://rustup.rs/)

```bash
# 1. Install Tauri CLI
npm run tauri:install

# 2. In one terminal, start the FastAPI backend
cd ../rest-api
python main.py

# 3. In another terminal, start Tauri dev mode
npm run tauri:dev
```

This opens a native desktop window with hot-reload.

### Production Build

See [DESKTOP_BUILD.md](DESKTOP_BUILD.md) for detailed build instructions, including:
- Building the FastAPI executable with PyInstaller
- Creating platform-specific installers (.msi, .dmg, .AppImage)
- Code signing and distribution

## Production Deployment

### Option 1: Web Browser

1. Start the FastAPI backend (separate)
2. Deploy React app on any static hosting (Vercel, Netlify, etc.)
3. Configure API URL for your backend server

### Option 2: Bundled Web (FastAPI serves React)

1. Build the React app:
   ```bash
   npm run build
   npm run bundle
   ```

2. This copies the build to `../rest-api/static/`

3. Add static file serving to `main.py`:
   ```python
   from fastapi.staticfiles import StaticFiles
   
   # Add at the end, after all API routes
   app.mount("/", StaticFiles(directory="static", html=True), name="static")
   ```

4. Run FastAPI server - it will serve both API and UI:
   ```bash
   python main.py
   ```

## Tech Stack

- **React 18** - UI framework
- **TypeScript** - Type safety
- **Vite** - Build tool
- **TailwindCSS** - Styling
- **Zustand** - State management
- **React Three Fiber** - 3D point cloud rendering
- **Recharts** - IMU data charts
- **Socket.IO Client** - Real-time metadata
- **WebRTC** - Low-latency video streaming

## API Integration

The viewer connects to the REST API at `/api/v1/`:

- `GET /devices` - List connected devices
- `GET /devices/{id}/sensors` - Get device sensors
- `PUT /devices/{id}/sensors/{sid}/options/{oid}` - Update camera option
- `POST /devices/{id}/streams/start` - Start streaming
- `POST /webrtc/offer` - WebRTC signaling

Real-time data is received via Socket.IO on the `/socket` path.

## License

Apache License 2.0 - See the main librealsense repository for details.
