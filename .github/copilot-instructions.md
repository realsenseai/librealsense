# Copilot Instructions for librealsense

## Skills

Detailed how-to guides for common tasks are maintained as agent skills under `.claude/skills/<name>/SKILL.md` (loaded automatically by Claude Code and by Copilot agents that support skills). **Before performing any of the tasks below, you MUST open and read the corresponding skill file.** Do not rely on prior knowledge or assumptions — the skill file is the source of truth.

| Skill file | Read before |
|---|---|
| `.claude/skills/add-api/SKILL.md` | Adding API to librealsense SDK |
| `.claude/skills/file-creation/SKILL.md` | Adding files to librealsense SDK |
| `.claude/skills/cpp-coding/SKILL.md` | Adding or modifying CPP code |
| `.claude/skills/build/SKILL.md` | Building the project (CMake configure, compile, flags) |
| `.claude/skills/testing/SKILL.md` | Running, filtering, and debugging unit tests |
| `.claude/skills/pytest-infra/SKILL.md` | Writing a new pytest test, modifying pytest/hub infrastructure |
| `.claude/skills/pr-create/SKILL.md` | Opening a pull request, pushing commits to it, updating its description |
| `.claude/skills/pr-review/SKILL.md` | Replying to review comments on a pull request |

If a skill exists for the task at hand, follow its instructions precisely. New skills may be added to `.claude/skills/` over time — check its contents before assuming none applies.

## Git Workflow (quick reference)

These rules apply to all git operations.

- **Base branch**: `development`
- **Branch naming**: short descriptive name, **no username prefix** (e.g. `fix-platform-camera`, not `nir/fix-platform-camera`)
- **PR target**: `development` branch
- **Push to**: `fork` remote (if no `fork` remote, ask the user)
- **Commits**: short one-sentence message, no Co-Authored-By, plain `git commit -m "message"`

## Project Overview

**librealsense** is the RealSense cross-platform open-source SDK for working with RealSense depth cameras (D400, D500 series and others). It provides C, C++, Python, C#, and other language bindings.

## Architecture

- **Core library** (`src/`): The `realsense2` shared/static library written in C++14 (public API requires only C++11)
- **Public API headers** (`include/librealsense2/`): C and C++ headers; version is defined in `rs.h`
- **Common UI code** (`common/`): Shared code for the viewer and graphical tools
- **Examples** (`examples/`): Sample applications demonstrating SDK usage
- **Tools** (`tools/`): Utilities like `realsense-viewer`, `fw-updater`, `enumerate-devices`, etc.
- **Wrappers** (`wrappers/`): Language bindings — Python (pybind11), C#, Unity, OpenCV, PCL, etc.
- **Unit tests** (`unit-tests/`): Python tests run under pytest (`pytest-*.py`); C++ tests run by the legacy `run-unit-tests.py` runner
- **Third-party** (`third-party/`): Vendored dependencies (`rsutils`, `realsense-file`, `json`, `glfw`, etc.)
- **CMake modules** (`CMake/`): Build configuration, platform detection, and external dependency management

## Code Conventions

- **Copyright header**: every new source file must start with `License: Apache 2.0. See LICENSE file in root directory.` followed by `Copyright(c) <current year> RealSense, Inc. All Rights Reserved.`, using the file's comment syntax (`//` or `#`) — always use the actual current year, not the year of a nearby file being used as a template
- The core library compiles as **C++14** (`cxx_std_14` — see `CMake/lrs_macros.cmake`)
- The public interface only requires **C++11** (`cxx_std_11`)
- Examples and wrappers generally use **C++11**
- CMake minimum version: **3.10** (3.16.3 when `BUILD_WITH_DDS` is enabled), enforced by the "Minimal CMake version" CI job — CMake changes must keep working with it
- Use the existing code style in surrounding files; the project does not enforce a formatter
- Logging uses EasyLogging++ (controlled by `BUILD_EASYLOGGINGPP` option)

## Code Change Discipline

These rules apply to every code change.

- **Keep changes minimal.** Every added line must earn its place. Prefer single-line expressions over multi-line blocks, reuse existing helpers and code paths instead of adding new ones, and use the shortest form that doesn't lose clarity.
- **Comments are sparse.** Keep comments to 1–2 lines unless the code is large and genuinely complex. Don't restate what the code already says. Never reference Jenkins build numbers or Jira tickets in comments.
- **Check before you add.** Before introducing a check, loop, or helper, verify the same logic doesn't already exist elsewhere in the call chain. If a function has a single caller, consider inlining it there. Never add a second copy of logic that can be merged with an existing one — consolidate first, don't layer.
- **Clean up after removal.** When deleting a function, block, or feature, search the same patch for everything that only existed to serve it — defines, variables, struct fields, forward declarations, comments — and remove them too. Leave no dead code behind.
- **Avoid environment variables.** Don't introduce new environment variables to gate or configure behavior; prefer existing options, parameters, and config mechanisms.

## Build System

See the `build` skill (`.claude/skills/build/SKILL.md`) for full build instructions. The project uses **CMake**. Key build options are defined in `CMake/lrs_options.cmake`. Platform-specific configuration lives in:
- `CMake/windows_config.cmake` — Windows (MSVC)
- `CMake/unix_config.cmake` — Linux / macOS
- `CMake/android_config.cmake` — Android NDK

Supported platforms, compilers and all CMake flags are listed in the `build` skill. Supported: Windows 10/11 (MSVC), Ubuntu 20.04–26.04 (GCC), macOS 15+, NVIDIA Jetson, Raspberry Pi, Android NDK.

## Testing

See the `testing` skill (`.claude/skills/testing/SKILL.md`) for full instructions. Build with `-DBUILD_UNIT_TESTS=ON -DBUILD_PYTHON_BINDINGS=ON` first, then from the `unit-tests/` directory run Python tests with `python -m pytest` and C++ tests with `python run-unit-tests.py`.

## Naming Conventions

snake_case for code, kebab-case for files, `rs2_` prefix for the public C API. Full rules are in the `cpp-coding` skill.

## Key Classes & Interfaces

Core class hierarchy:

```
librealsense::context           // Device discovery & management
└── librealsense::device        // Hardware device representation
    └── librealsense::sensor    // Individual camera sensor
        ├── stream_profile      // Stream configuration
        └── frame               // Data frame
```

Key base interfaces:
- `device_interface` — base for all devices
- `sensor_interface` — base for all sensors
- `frame_interface` — base for frame data
- `option_interface` — configuration option abstraction
- `backend_interface` — platform backend abstraction

## API Contracts & Error Handling

- **C API**: Functions report failures via `rs2_error*` out-parameters
- **C++ API**: Wrapper throws `rs2::error` exceptions on failure
- Public C API maintains **ABI compatibility** within major versions; new C functions must be exported in `src/realsense.def` (see the `add-api` skill)
- Not all stream formats are supported on all platforms/devices
- Some features require minimum firmware versions on the device

## Working with This Repo

- Platform-specific code should be guarded appropriately (check `CMake/include_os.cmake` for the pattern)
- `rsutils` (under `third-party/rsutils/`) is a foundational utility library linked publicly into `realsense2`
