# LibRS Rest API server
## Overview

This Python library provides a convenient wrapper to interact with the [RealSense REST API], a FastAPI-based service that exposes the functionality of the RealSense SDK (librealsense) over a network.

It simplifies remote control and data streaming from RealSense devices by handling communication protocols for:

1.  **RESTful Operations:** For device discovery, configuration, and control.
2.  **WebRTC:** For efficient, browser-compatible live video streaming (RGB, Depth).
3.  **Socket.IO:** For real-time streaming of frame metadata and point cloud data.
    * In order to decode point cloud data in the client, follow those steps:
        * Decode Base64 string to Uint8Array
        * 'bytes' now holds the raw byte data that originated from the NumPy array
        * Interpret bytes as Float32Array.
        IMPORTANT ASSUMPTION: We assume the original NumPy array used float32.
        If it used float64 (Python's default float), use Float64Array here.
        The ArrayBuffer gives access to the raw binary data buffer of the Uint8Array.
        The Float32Array constructor creates a *view* over that buffer, interpreting
        groups of 4 bytes as 32-bit floats without copying data.
        * Now you can use the 'vertices' Float32Array
    * Motion raw data is also propogated using this socket.io channel

## Support Features

*   **Device Management:**
    *   List connected RealSense devices.
    *   Get detailed information about specific devices (name, serial, firmware, etc.).
    *   Perform hardware resets remotely.
*   **Sensor & Option Control:**
    *   List available sensors on a device.
    *   Get detailed sensor information and supported stream profiles.
    *   List and retrieve current values for sensor options (e.g., exposure, gain).
    *   Update writable sensor options.
*   **REST-based Stream Control:**
    *   Start/Stop streams with specific configurations (resolution, format, FPS).
    *   Check the streaming status of a device.
*   **WebRTC Video Streaming:**
    *   Initiate WebRTC sessions with the API to receive live video feeds.
    *   Handles the offer/answer and ICE candidate exchange mechanisms required for WebRTC setup (via REST endpoints).
*   **Socket.IO Data Streaming:**
    *   Establish a Socket.IO connection to the API.
    *   Receive real-time events containing:
        *   Frame metadata (timestamps, frame numbers, etc.).
        *   Point cloud data streams.

## Missing Features
* Not all SDK capabilities are yet supported(e.g. Texture Mapping, Post processing filters,...)

**All endpoints and supported features are documented and available for interactive exploration at the `/docs` endpoint using the built-in OpenAPI (Swagger) UI.**

## Installation

### API Server Setup

1. **Create a virtual environment:**

   ```bash
   python3 -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```

2. **Install dependencies:**

   ```bash
   python3 install.py
   ```

   This installs the packages listed in `requirements.txt` and, if `pyrealsense2`
   is not already importable, also pulls it from PyPI. If you already have
   `pyrealsense2` installed (locally built, system package, or any pip install),
   the script leaves it untouched.

3. **Run API server:**

   You can start the server using either method:
   
   ```bash
   python main.py
   ```
   
   Or use the shell script (Linux/macOS):
   
   ```bash
   ./start_server.sh
   ```

   The API server will start on `http://localhost:8000`

4. **Verify the API:**

   Open your browser and navigate to `http://localhost:8000/docs` to explore the API documentation via Swagger UI.

### React Viewer Setup (Optional)

For a modern web-based UI to interact with RealSense devices:

1. **Prerequisites:**
   - Node.js 18+ and npm installed

2. **Navigate to the React viewer directory:**

   ```bash
   cd tools/react-viewer
   ```

3. **Install dependencies:**

   ```bash
   npm install
   ```

4. **Configure environment (if needed):**

   Copy `.env.example` to `.env` and adjust settings if your API server runs on a different port.

## Usage

### Running with React Viewer

1. **Start the REST API server** (in one terminal):

   ```bash
   # From wrappers/rest-api directory
   python main.py
   ```

2. **Start the React viewer** (in another terminal):

   ```bash
   # From wrappers/rest-api/tools/react-viewer directory
   npm run dev
   ```

3. **Open in browser:**

   Navigate to `http://localhost:3000`

For more details about the React viewer features and development, see `tools/react-viewer/README.md`.

### Running Tests

The `run_tests.py` helper at the rest-api root runs both the backend pytest suite and the React viewer Vitest suite:

```bash
# From wrappers/rest-api:
python run_tests.py             # run both backend + frontend
python run_tests.py --backend   # backend pytest only
python run_tests.py --frontend  # react-viewer Vitest only
```

You can also run each suite directly:

```bash
# Backend (FastAPI / pytest)
pytest tests/

# Frontend (React viewer / Vitest)
cd tools/react-viewer && npm test
```

For React viewer end-to-end Playwright tests and full setup details, see
[`tools/react-viewer/tests/INSTALLATION.md`](tools/react-viewer/tests/INSTALLATION.md).

## Advanced Testing

**For comprehensive testing with test extras (from the repository root):**

```bash
pip3 install -r wrappers/rest-api/requirements.txt -r unit-tests/wrappers/rest-api/requirements.txt
```

## Missing features that may be added in the future

1. Post Processing Filters (Advanced)
2. Record and Playback
3. Agent / Ask for assistant
4. Advanced texture mapping
5. Metadata

**Note:** Basic 3D point cloud visualization and IMU data viewing are available in the React viewer.