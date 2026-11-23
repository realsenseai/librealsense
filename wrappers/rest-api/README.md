# LibRS Rest API server
## Overview

This Python library provides a convenient wrapper to interact with the [RealSense REST API], a FastAPI-based service that exposes the functionality of the RealSense SDK (librealsense) over a network.

It simplifies remote control and data streaming from RealSense devices by handling communication protocols for:

1.  **RESTful Operations:** For device discovery, configuration, and control.
2.  **WebSocket frames:** Each stream's frames as the SDK gives them, one WebSocket per stream.
3.  **WebSocket events:** `/api/v1/events` tells every client what changed (devices, streaming, firmware writes).

Frames are sent raw (720p depth + color is about 1 Gbit/s), so a slower link drops the FPS; compression for remote viewing is planned (RSDEV-16485).

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
    *   Start/Stop streams with specific configurations (resolution, format, FPS), through the pipeline.
    *   Open, start, stop and close each sensor on its own, as the SDK's sensor.
*   **Frame Streaming:**
    *   Open a WebSocket to `/api/v1/devices/{device_id}/frames/{stream_type}/{stream_index}` at any time: it gets the stream's frames whenever the stream runs, started by any client, and only the client closes it. `streaming_changed` on the events socket says when frames start and stop.
    *   Each binary message is one frame: a JSON header (number, timestamp, metadata, profile, size, intrinsics) and the frame's bytes, unchanged; see `app/services/frame_sender.py`.
*   **Events:**
    *   Open a plain WebSocket to `/api/v1/events`; each message is JSON, `{"event": name, ...fields}`.
    *   `devices_changed`, `streaming_changed`, `firmware_progress`, `options_changed`; see `app/services/events.py`.
    *   Events are not replayed: read the current state through the REST API once connected.

## Changes from v2.59.2
The API now mirrors the SDK's own objects and calls. Breaking changes:
*   **Sensors:**
    *   A sensor is addressed by `sensor_index` (int) instead of `sensor_id`, and driven by `POST .../sensors/{index}/open`, `/start`, `/stop`, `/close`, as `rs2::sensor`, instead of the combined `start` and `stop`.
    *   `SensorInfo` loses `sensor_id`, `type` and `options`; `supported_stream_profiles` is `stream_profiles`, `active_profiles` is `active_streams`.
    *   The `streaming_changed` event's `sensors` is a list by sensor index instead of a dict by `sensor_id`.
    *   `/health` no longer returns `status` and `service`.
*   **Options:**
    *   Sensor, colorizer and filter options are a map by option ID instead of a list; `OptionInfo` loses `option_id` (it is the key) and `units`.
    *   An option the camera supports but can't read now is listed with a null `current_value`; one whose range the SDK can't read has null `min_value`, `max_value`, `step` and `default_value`.
    *   A write (option, filter, advanced-mode control or toggle) answers nothing: read the value back with its GET; sensor option changes reach every client as `options_changed`.
    *   A route to one option has no trailing slash: `.../options/{option_id}`, `.../filters/{name}/{field}`, `.../advanced_mode/controls/{group}/{field}`.
    *   Advanced-mode controls are `AdvancedControl` (`current_value`, `min_value`, `max_value`, `integer`) by group, instead of `OptionInfo` lists.
*   **Devices and frames:**
    *   A device is the SDK's camera info by name (`serial_number`, `firmware_version`, `advanced_mode`, ...) instead of `DeviceInfo`; its ID is its firmware-update ID, which a camera in recovery mode also has. `force_refresh` and `POST /devices/refresh` are gone.
    *   `GET .../advanced_mode/` is gone: read the camera info `advanced_mode`, YES or NO.
    *   Metadata status is `GET .../is_metadata_enabled`; `POST /system/enable-metadata` is `enable_metadata` and answers nothing.
    *   `extrinsics` takes `from_stream`, `from_index`, `to_stream`, `to_index` and answers one `Extrinsics`.
    *   `POST .../hw_reset/` is `POST .../hardware_reset` and answers nothing.
    *   HWM answers the firmware's raw reply bytes instead of `HwmResponse`: its first uint32 echoes the opcode, or is the firmware's error code.
*   **Firmware:**
    *   `GET .../firmware/`, `POST .../firmware/update_from_file` and `update_from_recommended` are gone: the client finds and downloads the image itself, from the online versions list, as the legacy viewer does, and drives the SDK's calls: `POST .../firmware/check_firmware_compatibility`, `enter_update_state` and `update`, each taking the image as the request body (`application/octet-stream`).
    *   A camera in recovery mode is listed, under the same ID; `GET .../is_update_device` says whether it is in recovery mode.

## Missing Features
* Not all SDK capabilities are yet supported(e.g. Texture Mapping, Post processing filters,...)

**All endpoints and supported features are documented and available for interactive exploration at the `/docs` endpoint using the built-in OpenAPI (Swagger) UI.**

## Installation

### API Server Setup

Requires Python 3.10+ (tested through 3.14).

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