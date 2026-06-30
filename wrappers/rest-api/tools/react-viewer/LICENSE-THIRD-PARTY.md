# Third-Party Licenses for RealSense Viewer Packaged Application

This application is built using only open source dependencies with permissive licenses (MIT, Apache-2.0, BSD, ISC or equivalent).

## Desktop Packaging

| Dependency         | Version   | License             | Notes                                                                |
|--------------------|-----------|---------------------|----------------------------------------------------------------------|
| Tauri              | 1.5.x     | Apache-2.0 OR MIT   | Desktop shell                                                        |
| tauri-build        | 1.5.x     | Apache-2.0 OR MIT   | Tauri build-script crate                                             |
| @tauri-apps/cli    | 1.5.x     | Apache-2.0 OR MIT   | CLI tooling                                                          |
| @tauri-apps/api    | 1.5.x     | Apache-2.0 OR MIT   | JS API bridge                                                        |
| PyInstaller        | 6.x       | GPL-2.0+            | *Exception: bootloader is BSD-3-Clause, generated binaries are not GPL* |
| Rust               | 1.91+     | Apache-2.0 OR MIT   | Toolchain                                                            |

## Tauri / Rust Runtime (direct Cargo dependencies)

| Dependency  | Version | License            | Notes                                |
|-------------|---------|--------------------|--------------------------------------|
| serde       | 1.0     | Apache-2.0 OR MIT  | Serialization framework              |
| serde_json  | 1.0     | Apache-2.0 OR MIT  | JSON support for serde               |
| tokio       | 1.x     | MIT                | Async runtime for the FastAPI spawner |
| reqwest     | 0.11    | Apache-2.0 OR MIT  | HTTP client for backend health check |

## Backend (Python runtime — bundled via pip / PyInstaller)

| Dependency         | Version   | License        | Notes                                          |
|--------------------|-----------|----------------|------------------------------------------------|
| FastAPI            | 0.120.x   | MIT            | REST API framework                             |
| Pydantic           | 2.x       | MIT            | Request / response validation                  |
| Uvicorn            | 0.34.x    | BSD-3-Clause   | ASGI server                                    |
| python-socketio    | 5.x       | MIT            | Socket.IO server                               |
| aiortc             | 1.11.x    | BSD-3-Clause   | WebRTC stack (PeerConnection, video tracks)    |
| PyAV (`av`)        | 14.x      | BSD-3-Clause   | Pulled transitively by aiortc (FFmpeg wheel binding) |
| OpenCV (`opencv-python`) | 4.11.x | Apache-2.0  | Image processing wheel                         |
| numpy              | 2.x       | BSD-3-Clause   | Numeric arrays                                 |
| pyrealsense2       | 2.x       | Apache-2.0     | RealSense SDK Python bindings (in-house)       |

## Frontend (React runtime — bundled into `dist/` by Vite)

| Dependency           | Version | License | Notes                              |
|----------------------|---------|---------|------------------------------------|
| React                | 18.x    | MIT     | UI framework                       |
| React DOM            | 18.x    | MIT     | DOM renderer for React             |
| @react-three/fiber   | 8.x     | MIT     | React renderer for Three.js        |
| @react-three/drei    | 9.x     | MIT     | Three.js helper components         |
| Three.js             | 0.158.x | MIT     | 3D engine (point cloud rendering)  |
| Recharts             | 2.x     | MIT     | Charts (IMU graphs)                |
| axios                | 1.x     | MIT     | HTTP client                        |
| socket.io-client     | 4.x     | MIT     | Socket.IO client                   |
| Zustand              | 4.x     | MIT     | State management                   |
| lucide-react         | 0.290.x | ISC     | Icon set                           |

## Build / Dev Tools (not shipped in production bundles)

| Dependency                    | Version | License              | Notes                          |
|-------------------------------|---------|----------------------|--------------------------------|
| Vite                          | 4.x     | MIT                  | Build tool                     |
| @vitejs/plugin-react          | 4.x     | MIT                  | React Fast Refresh             |
| TypeScript                    | 5.x     | Apache-2.0           | Compiler                       |
| @types/* (node, react, three) | various | MIT                  | DefinitelyTyped type stubs     |
| ESLint                        | 8.x     | MIT                  | Linter                         |
| @typescript-eslint/parser     | 6.x     | BSD-2-Clause         | TS parser for ESLint           |
| @typescript-eslint/eslint-plugin | 6.x  | MIT                  | TS rules for ESLint            |
| eslint-plugin-react-hooks     | 4.x     | MIT                  | React hooks rules              |
| eslint-plugin-react-refresh   | 0.4.x   | MIT                  | Fast Refresh rules             |
| Vitest                        | 0.34.x  | MIT                  | Unit testing                   |
| @vitest/coverage-v8           | 0.34.x  | MIT                  | Coverage reporter              |
| @testing-library/react        | 14.x    | MIT                  | React testing utilities        |
| @testing-library/jest-dom     | 6.x     | MIT                  | DOM-matcher assertions         |
| @testing-library/user-event   | 14.x    | MIT                  | User interaction simulation    |
| jsdom                         | 22.x    | MIT                  | Browser environment for tests  |
| MSW (Mock Service Worker)     | 2.x     | MIT                  | HTTP request mocking           |
| Playwright                    | 1.x     | Apache-2.0           | E2E testing                    |
| Tailwind CSS                  | 3.x     | MIT                  | Utility-first CSS              |
| PostCSS                       | 8.x     | MIT                  | CSS processor                  |
| Autoprefixer                  | 10.x    | MIT                  | CSS vendor prefixing           |

---

## Notes

- All shipped runtime dependencies are MIT, Apache-2.0, BSD or ISC — all OSI-approved permissive licenses.
- Rust ecosystem packages are commonly dual-licensed (`Apache-2.0 OR MIT`); recipients may choose either.
- **PyInstaller** is GPL-2.0+ but its [bootloader exception](https://pyinstaller.org/en/stable/license.html) means generated executables are **not subject to GPL**.
- No copyleft or non-commercial dependencies are bundled into the produced artifacts.
- The list above covers **direct** dependencies declared in `requirements.txt`, `package.json`, and `Cargo.toml`. Transitive dependencies inherit the same permissive license classes; the full transitive set is enumerated in the respective lockfiles (`package-lock.json`, `Cargo.lock`).

---

*This file summarizes third-party licenses for the RealSense Viewer packaged application as of this release.*
