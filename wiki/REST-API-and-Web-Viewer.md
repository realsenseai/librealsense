The RealSense REST API is a Python server, built with [FastAPI](https://fastapi.tiangolo.com/) on top of the `pyrealsense2` module, that exposes RealSense devices over HTTP: REST endpoints for device, sensor, option and stream control, WebRTC for live video, and Socket.IO for frame metadata, point clouds and device events. The React web viewer is a browser front end for that server. Both live in [`wrappers/rest-api`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api). This page is for anyone who wants to control a camera remotely, from a browser, or from a language without native librealsense bindings.

## Overview

| Part | Location | What it is |
|---|---|---|
| REST API server | [`wrappers/rest-api`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api) | FastAPI app ([`main.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/main.py)) combined with a Socket.IO server into one ASGI app, served by Uvicorn. |
| Route definitions | [`app/api/router.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/api/router.py), [`app/api/endpoints`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api/app/api/endpoints) | All REST routes, mounted under `/api/v1`. |
| Device logic | [`app/services/rs_manager.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/services/rs_manager.py) | Wraps `pyrealsense2` (device enumeration, streaming, options, firmware). |
| React viewer | [`wrappers/rest-api/tools/react-viewer`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api/tools/react-viewer) | Vite + React + TypeScript web UI (package `realsense-react-viewer`), optionally packaged as a Tauri desktop app. |

The server talks to cameras through `pyrealsense2`, so the device support is that of the SDK build it loads; see [[Python Wrapper]] and [[Supported Devices]].

## Installing the server

From `wrappers/rest-api`, create a virtual environment and run the install script:

```bash
cd wrappers/rest-api
python3 -m venv venv
source venv/bin/activate        # Windows: venv\Scripts\activate
python3 install.py
```

[`install.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/install.py) does two things:

1. If `import pyrealsense2` fails, it runs `pip install pyrealsense2` (from PyPI). If `pyrealsense2` is already importable (built from source, system package or an earlier pip install), it prints `pyrealsense2 already available - skipping PyPI install` and leaves it alone.
2. It runs `pip install -r requirements.txt`.

[`requirements.txt`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/requirements.txt) pins:

| Package | Version |
|---|---|
| `fastapi` | 0.120.1 |
| `uvicorn` | 0.34.0 |
| `pydantic` | 2.12.5 |
| `python-multipart` | 0.0.29 (used to parse the firmware file upload) |
| `aiortc` | 1.15.0 (installed with `--only-binary=av`, so the prebuilt `av` wheel is used) |
| `numpy` | 2.2.4 on Python older than 3.11; 2.3.5 on Python 3.11 and later |
| `python-socketio` | 5.13.0 |

Every requirement carries the marker `platform_machine != "aarch64"`, so on aarch64 machines `pip` skips all of them and you must install the dependencies yourself.

`opencv-python` was dropped from the requirements *(changed in the next release)*; in v2.59.1 `requirements.txt` also installed `opencv-python==4.11.0.86`.

### Using a locally built pyrealsense2

At start-up, [`main.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/main.py) looks for a `build/` directory at the repository root. If it finds `pyrealsense2*.pyd`, `pyrealsense2*.so` or `pyrealsense2/__init__.py` under it, it puts the directories holding them at the front of `sys.path` (newest first), so a module built from your checkout is preferred over a PyPI wheel. This lets the server see devices that the published wheel does not know yet. It prints `[pyrealsense2] preferring local build: <dir>`. If the chosen module is under a `Debug` directory, it logs a warning instead, because a Debug SDK makes streaming slow; the same condition is reported in the `warnings` list returned by `/api/v1/health`.

To build `pyrealsense2` from source, see [[Python Wrapper]] (CMake option `BUILD_PYTHON_BINDINGS`).

## Starting the server

There are two ways to start it from `wrappers/rest-api`:

```bash
python main.py                             # 127.0.0.1:8000, auto-reload, debug logging
python main.py --host 0.0.0.0 --port 8080  # listen on all interfaces, other port
```

```bash
./start_server.sh                          # Linux/macOS: uvicorn on 0.0.0.0:8000
```

| Launcher | Host | Port | Notes |
|---|---|---|---|
| `python main.py` | `--host`, default `127.0.0.1` | `--port`, default `8000` (1-65535) | Runs `uvicorn.run("main:combined_app", ..., reload=True, log_level="debug")`. When frozen with PyInstaller, reload is off and the log level is `info`. |
| [`start_server.sh`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/start_server.sh) | `0.0.0.0` | `8000` | Runs `uvicorn main:combined_app --host 0.0.0.0 --port 8000`. |

The `--host` and `--port` arguments are *(new in the next release)*. In v2.59.1, `python main.py` always listened on `127.0.0.1:8000`.

The default host `127.0.0.1` only accepts local clients. Use `--host 0.0.0.0` (or `start_server.sh`) to accept connections from other machines. The server has no authentication, so only do this on a trusted network.

Once it is running:

- Interactive API documentation (FastAPI's Swagger UI) is at `http://localhost:8000/docs`.
- The OpenAPI schema is at `http://localhost:8000/api/v1/openapi.json`.
- `GET http://localhost:8000/api/v1/health` returns `status`, `service` (`realsense-api`), `sdk_version` (the loaded `pyrealsense2` version) and `warnings`.

### Server settings

Settings are class attributes in [`config.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/config.py). They are not read from environment variables; change them by editing the file.

| Setting | Default | Used for |
|---|---|---|
| `API_V1_STR` | `"/api/v1"` | Prefix of all REST routes and of the OpenAPI URL. |
| `PROJECT_NAME` | `"RealSense REST API"` | FastAPI title and the `main.py` argument parser description. |
| `CORS_ORIGINS` | `["*"]` | Allowed origins for the CORS middleware. |
| `STUN_SERVER` | `None` | If set, added as an ICE server for WebRTC sessions. |
| `TURN_SERVER`, `TURN_USERNAME`, `TURN_PASSWORD` | `None` | If `TURN_SERVER` is set, added as an ICE server with these credentials. |
| `SECRET_KEY`, `ALGORITHM`, `ACCESS_TOKEN_EXPIRE_MINUTES` | `"your-secret-key-here"`, `"HS256"`, `30` | Token helpers in `app/core/security.py`; no route requires a token. |

## Running the web viewer

The viewer needs Node.js and npm. It takes the backend origin from [`src/api/backend.ts`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/src/api/backend.ts): the page origin in a browser, `http://localhost:8000` in the desktop app. It calls REST routes under `/api/v1` on that origin and connects Socket.IO on the path `/socket`.

### Development server

Start the REST API server in one terminal, then in another:

```bash
cd wrappers/rest-api/tools/react-viewer
npm install
npm run dev
```

Open `http://localhost:3000`. The Vite dev server ([`vite.config.ts`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/vite.config.ts)) runs on port 3000 and proxies `/api` and `/socket` (with WebSocket support) to `http://localhost:8000`, so the API server must be on port 8000 on the same machine.

### Production build

```bash
npm run build      # tsc && vite build --logLevel warn -> dist/
npm run preview    # serve dist/ on port 4173, same proxy to localhost:8000
```

The `preview` proxy *(new in the next release)* lets the production bundle reach the backend; dev mode React is slower than the production build.

### Serving the viewer from the API server

```bash
npm run build
npm run bundle     # copies dist/ to wrappers/rest-api/static/
```

[`scripts/bundle-for-prod.js`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/scripts/bundle-for-prod.js) replaces `wrappers/rest-api/static/` with the build output. When `static/index.html` exists, `main.py` serves that directory at the root, so the viewer and the API share one port: open `http://localhost:8000/`. Serving `static/` from `main.py` is *(new in the next release)*; in v2.59.1 the bundle script only printed instructions for mounting it yourself.

### Viewer features

The main screen ([`src/App.tsx`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/src/App.tsx)) has a device panel for selecting devices, streams and options, a stream view (video over WebRTC) and a 3D point cloud view. It also shows motion (gyro/accel) streams as charts and an orientation view *(changed in the next release)*, firmware update progress, server warnings from `/api/v1/health`, and an AI assistant panel. In v2.59.1, motion streams were shown as X/Y/Z values with bars.

The assistant panel calls external services directly from the browser. Its default mode, "RealSense AI Assistant", uses a hosted RealSense AI service and needs no key. A second mode, "Device Config Chatbot", calls an LLM provider. It is enabled by Vite environment variables in a `.env` file (see [`.env.example`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/.env.example)), read in [`src/api/chat.ts`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/src/api/chat.ts) in this order:

| Variable | Provider |
|---|---|
| `VITE_GROQ_API_KEY` | Groq |
| `VITE_OPENAI_API_KEY` | OpenAI (model `gpt-4o-mini`) |
| `VITE_LLM_API_KEY` + `VITE_LLM_API_URL` (optional `VITE_LLM_MODEL`, default `gpt-4o-mini`) | Any OpenAI-compatible endpoint |

Without any of these, or if the viewer's check request to the provider fails or returns an error status, the panel does not offer the chatbot mode; when no key is set, the viewer only logs `No chat API key configured. Set VITE_GROQ_API_KEY (free) or VITE_OPENAI_API_KEY` to the browser console.

### Desktop app

[`build-all.sh`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/build-all.sh) (Linux/macOS) and [`build-all.ps1`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/build-all.ps1) (Windows) build a standalone "RealSense Viewer" desktop app: the API server as a PyInstaller executable (`realsense_api`), the React build, and a Tauri bundle that starts the bundled server on `127.0.0.1:8000`. Run `./build-all.sh --help` for requirements and output locations, and see [`DESKTOP_BUILD.md`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/DESKTOP_BUILD.md).

## Typical REST workflow

A client usually lists devices, reads a sensor's supported profiles, starts streaming, and then receives frames over WebRTC and metadata over Socket.IO:

```bash
# List devices; device_id is the serial number
curl http://localhost:8000/api/v1/devices/

# List sensors with their supported_stream_profiles and options
curl http://localhost:8000/api/v1/devices/<serial>/sensors/

# Start streaming through the pipeline
curl -X POST http://localhost:8000/api/v1/devices/<serial>/stream/start \
  -H "Content-Type: application/json" \
  -d '{"configs":[{"sensor_id":"<sensor_id>","stream_type":"depth","format":"z16",
       "resolution":{"width":640,"height":480},"framerate":30}]}'

# Stop
curl -X POST http://localhost:8000/api/v1/devices/<serial>/stream/stop
```

Take `sensor_id`, `stream_type`, `format`, resolution and framerate values from the `GET .../sensors/` response for your device. The request body model is `StreamStart` in [`app/models/stream.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/models/stream.py) (`configs`, optional `align_to`, `apply_filters`).

There are two ways to stream, and they cannot be used at the same time: the pipeline routes (`.../stream/start`) and the per-sensor routes (`.../sensors/{sensor_id}/start`). Stop all streams before switching between them.

## REST routes

All routes below are prefixed with `/api/v1`. They are listed in the order the routers are registered in [`app/api/router.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/api/router.py), and within each router in source order. Trailing slashes are as written in the code. Request and response models are in [`app/models`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api/app/models); the Swagger UI at `/docs` shows the full schemas.

| Method | Path | Purpose |
|---|---|---|
| GET | `/health` | Service status, loaded SDK version and environment warnings. |
| GET | `/devices/` | List connected devices; query `force_refresh=true` re-enumerates. |
| GET | `/devices/{device_id}` | Details of one device. |
| POST | `/devices/refresh` | Re-enumerate and return the device list. |
| POST | `/devices/{device_id}/hw_reset/` | Hardware reset. |
| GET | `/devices/{device_id}/advanced_mode/` | Advanced mode status. |
| POST | `/devices/{device_id}/advanced_mode/` | Enable or disable advanced mode (body `{"enable": bool}`); restarts the device. |
| GET | `/devices/{device_id}/advanced_mode/controls/` | Advanced mode controls, grouped. |
| PUT | `/devices/{device_id}/advanced_mode/controls/{group}/{field}/` | Set one advanced mode control (body `{"value": number}`). |
| GET | `/devices/{device_id}/colorizer/` | Depth colorizer options. |
| PUT | `/devices/{device_id}/colorizer/{field}/` | Set a colorizer option (body `{"value": number}`). |
| GET | `/devices/{device_id}/sensors/{sensor_id}/filters/` | Post-processing filters of a sensor. |
| PUT | `/devices/{device_id}/sensors/{sensor_id}/filters/{filter_name}/enabled/` | Enable or disable a filter (body `{"value": number}`). |
| PUT | `/devices/{device_id}/sensors/{sensor_id}/filters/{filter_name}/{field}/` | Set a filter option (body `{"value": number}`). |
| GET | `/devices/{device_id}/firmware/` | Firmware version recommended for the device by the online versions database. |
| POST | `/devices/{device_id}/firmware/update_from_file` | Flash a `.bin` image sent as multipart form field `file` (max 64 MiB). |
| POST | `/devices/{device_id}/firmware/update_from_recommended` | Download the recommended firmware and flash it. |
| POST | `/devices/{device_id}/hwm/` | Send a hardware monitor command (`opcode`, `param1`-`param4`, `data`); returns raw response bytes. |
| POST | `/devices/{device_id}/point_cloud/activate/` | Turn on point cloud generation. |
| POST | `/devices/{device_id}/point_cloud/deactivate/` | Turn off point cloud generation. |
| GET | `/devices/{device_id}/point_cloud/status` | Point cloud status. |
| GET | `/devices/{device_id}/sensors/` | List sensors, with supported stream profiles and options. |
| GET | `/devices/{device_id}/sensors/{sensor_id}` | Details of one sensor. |
| POST | `/devices/{device_id}/sensors/{sensor_id}/start` | Open and start one sensor with a list of `profiles` taken from its `supported_stream_profiles`. |
| POST | `/devices/{device_id}/sensors/{sensor_id}/stop` | Stop one sensor; other sensors keep streaming. |
| GET | `/devices/{device_id}/sensors/{sensor_id}/options/` | List sensor options. |
| GET | `/devices/{device_id}/sensors/{sensor_id}/options/{option_id}/` | Read one option. |
| PUT | `/devices/{device_id}/sensors/{sensor_id}/options/{option_id}/` | Set one option (body `{"value": number}`). |
| POST | `/devices/{device_id}/stream/start` | Start pipeline streaming (body `StreamStart`); returns the stream status. |
| POST | `/devices/{device_id}/stream/stop` | Stop pipeline streaming. |
| GET | `/devices/{device_id}/stream/status` | Streaming status (`is_streaming`, `active_streams`, ...). |
| GET | `/devices/{device_id}/stream/depth-at-pixel` | Depth in meters at query `x`, `y`. |
| GET | `/devices/{device_id}/stream/depth-range` | Depth range for the colour legend, computed from the current frame. |
| POST | `/system/enable-metadata` | On Windows, enables UVC frame metadata through an elevated (UAC) call to `rs.enable_metadata()`; on other systems returns `{"status": "noop"}`. |
| POST | `/webrtc/offer` | Create a WebRTC session for `device_id` and `stream_types`; returns `session_id`, `sdp`, `type`. |
| POST | `/webrtc/answer` | Send the client's SDP answer (`session_id`, `sdp`, `type`). |
| POST | `/webrtc/ice-candidates` | Add a client ICE candidate (`session_id`, `candidate`, `sdpMid`, `sdpMLineIndex`). |
| GET | `/webrtc/sessions/{session_id}` | Session status. |
| GET | `/webrtc/sessions/{session_id}/ice-candidates` | Server ICE candidates for the session. |
| DELETE | `/webrtc/sessions/{session_id}` | Close the session. |

Differences from v2.59.1 *(changed in the next release)*:

- `.../sensors/{sensor_id}/start` takes `{"profiles": [...]}`. In v2.59.1 it took `config` or `configs` and returned a status object, and a `GET .../sensors/{sensor_id}/status` route existed; that route has been removed.
- `supported_stream_profiles` in `GET .../sensors/` lists one entry per stream profile (`stream_type`, `stream_index`, `format`, `width`, `height`, `fps`, `default`). In v2.59.1 each entry grouped the `resolutions`, `fps` and `formats` lists of one stream type.
- `.../stream/start` returns the stream status. In v2.59.1 it returned timing information and accepted a `reuse_cache` field.
- The point cloud routes are `activate/` and `deactivate/` with a trailing slash. In v2.59.1 they had no trailing slash.

For firmware updates in general, see [[Firmware Update]]; for the filters, see [[Post Processing Filters]]; for advanced mode, see [[D400 Advanced Mode]].

## Socket.IO events

The Socket.IO server ([`app/services/socketio.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/services/socketio.py)) is mounted on the same port as the REST API, at path `/socket`, and allows any origin. The server emits:

| Event | When | Payload |
|---|---|---|
| `welcome` | On connect, to the new client | `{"message": "Connected to RealSense Metadata Server"}` |
| `metadata_update` | About 30 times per second for each streaming device | `device_id`, `is_streaming`, `timestamp_server`, `metadata_streams` (per stream type) |
| `devices_changed` | Devices added or removed | `{"added": [...], "removed": [...]}` |
| `firmware_progress_<device_id>` | During a firmware update | `device_id`, `progress`, `phase` |
| `firmware_update_success_<device_id>` | Firmware update finished | `device_id`, `firmware_version` |
| `firmware_update_failed_<device_id>` | Firmware update failed | `device_id`, `error` |

Point cloud data rides on `metadata_update` in the depth entry, in every third broadcast ([`metadata_socket_server.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/services/metadata_socket_server.py)). `point_cloud.vertices` (and `colors`, if present) are Base64-encoded raw NumPy array bytes. Motion samples are in `motion_data`.

## WebRTC streaming

Video is delivered over WebRTC by [`app/services/webrtc_manager.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/app/services/webrtc_manager.py) (using `aiortc`). The server creates the offer:

1. Start streaming on the device (`POST .../stream/start`).
2. `POST /api/v1/webrtc/offer` with `{"device_id": "<serial>", "stream_types": ["color", "depth"]}`. The device must be streaming and every requested type must be in `active_streams` of `.../stream/status`, otherwise the server returns 400.
3. Set the returned SDP as the remote description in the client, create an answer, and `POST /api/v1/webrtc/answer` with the `session_id`, `sdp` and `type`.
4. Exchange ICE candidates through `POST /api/v1/webrtc/ice-candidates` and `GET /api/v1/webrtc/sessions/{session_id}/ice-candidates`.
5. `DELETE /api/v1/webrtc/sessions/{session_id}` when done.

To use WebRTC across networks, set `STUN_SERVER` or `TURN_SERVER` in `config.py`. The React viewer's client code in [`src/api/webrtc.ts`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/tools/react-viewer/src/api/webrtc.ts) is a working example.

## Running tests

[`run_tests.py`](https://github.com/realsenseai/librealsense/blob/development/wrappers/rest-api/run_tests.py) runs the backend pytest suite in [`tests/`](https://github.com/realsenseai/librealsense/tree/development/wrappers/rest-api/tests) and the viewer's Vitest suite:

```bash
cd wrappers/rest-api
python run_tests.py             # both suites
python run_tests.py --backend   # python -m pytest tests/ -v
python run_tests.py --frontend  # npm test in tools/react-viewer
```

The frontend suite needs `npm install` in `tools/react-viewer` first; otherwise the script prints that `node_modules` is missing and fails. The viewer also has Playwright end-to-end tests (`npm run test:e2e`).

## Related pages

- [[Python Wrapper]] - the `pyrealsense2` module the server is built on.
- [[Realsense Viewer Manual]] - the native desktop viewer.
- [[Frame Metadata]] - the metadata reported in `metadata_update`.
- [[Ethernet and DDS Cameras]] - cameras that connect over the network natively.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`46cf1a6`](https://github.com/realsenseai/librealsense/commit/46cf1a67cc441ad9f55ce339a0de30f2a1c6fed0)._
<!-- /wikigen:footer -->
