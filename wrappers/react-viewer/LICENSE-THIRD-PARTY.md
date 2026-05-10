# Third-Party Licenses for RealSense Viewer Packaged Application

This application is built using only open source dependencies with permissive licenses (MIT, Apache-2.0, BSD, or equivalent).

## Desktop Packaging

| Dependency         | Version   | License      | Notes                        |
|--------------------|-----------|-------------|------------------------------|
| Tauri              | 1.5.x     | Apache-2.0   | Desktop shell                |
| @tauri-apps/cli    | 1.5.x     | Apache-2.0   | CLI tooling                  |
| @tauri-apps/api    | 1.5.x     | Apache-2.0   | JS API bridge                |
| PyInstaller        | 6.x       | GPL-2.0+     | *Exception: bootloader is BSD-3-Clause, generated binaries are not GPL* |
| Rust               | 1.91+     | Apache-2.0/MIT | Toolchain                  |

## Backend

| Dependency         | Version   | License      | Notes                        |
|--------------------|-----------|-------------|------------------------------|
| FastAPI            | 0.104+    | MIT          | REST API                     |
| Uvicorn            | 0.23+     | BSD-3-Clause | ASGI server                  |
| python-socketio    | 5.x       | MIT          | WebSocket support            |
| numpy              | 1.26+     | BSD          | Numeric computing            |
| pyrealsense2       | 2.x       | Apache-2.0   | Intel RealSense SDK Python   |

## Frontend

| Dependency         | Version   | License      | Notes                        |
|--------------------|-----------|-------------|------------------------------|
| React              | 18.x      | MIT          | UI framework                 |
| Vite               | 5.x       | MIT          | Build tool                   |
| socket.io-client   | 4.x       | MIT          | WebSocket client             |
| @testing-library/* | 14.x      | MIT          | Testing utilities            |

## Build/Dev Tools

| Dependency         | Version   | License      | Notes                        |
|--------------------|-----------|-------------|------------------------------|
| Vitest             | 1.x       | MIT          | Unit testing                 |
| Playwright         | 1.x       | Apache-2.0   | E2E testing                  |

---

## Notes

- All dependencies are MIT, Apache-2.0, or BSD, except PyInstaller, which is GPL-2.0+ but **generated executables are not subject to GPL** due to the bootloader exception (see [PyInstaller FAQ](https://pyinstaller.org/en/stable/license.html)).
- No copyleft or non-commercial dependencies are included.
- All licenses are OSI-approved and compatible with commercial and open source distribution.

---

*This file summarizes third-party licenses for the RealSense Viewer packaged application as of this release.*
