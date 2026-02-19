# Running Tests in librealsense

## Prerequisites

1. Build with tests and Python bindings enabled:
   ```bash
   cmake .. -DBUILD_UNIT_TESTS=ON -DBUILD_PYTHON_BINDINGS=ON
   cmake --build . --config Release
   ```
2. **Python 3** must be available
3. The `pyrealsense2` Python module must be built (ensured by `-DBUILD_PYTHON_BINDINGS=ON`)
4. For **live tests**: an Intel RealSense device must be connected

## Test Framework

- librealsense uses a **proprietary Python-based test framework**
- The test orchestrator is `unit-tests/run-unit-tests.py`
- Tests must be run **from the `unit-tests/` directory**

## Test Categories

| Directory | Type | Description |
|---|---|---|
| `unit-tests/live/` | Live (C++ & Python) | Require a connected RealSense device |
| `unit-tests/sw-dev/` | Software | Software-device tests, no hardware needed |
| `unit-tests/dds/` | DDS | DDS-related tests (require `BUILD_WITH_DDS=ON`) |
| `unit-tests/algo/` | Algorithm | Algorithm-level unit tests |
| `unit-tests/post-processing/` | Post-processing | Post-processing filter tests |
| `unit-tests/syncer/` | Syncer | Frame synchronization tests |
| `unit-tests/3D/` | 3D | Projection and 3D-related tests |
| `unit-tests/log/` | Logging | Logging infrastructure tests |
| `unit-tests/types/` | Types | Type system tests |

## Running All Tests

Navigate to the `unit-tests/` directory and run:

```bash
cd unit-tests
py -3 run-unit-tests.py -s
```

The `-s` flag directs test output to stdout. You can also specify the build output directory (required when both Debug and Release builds exist, to avoid ambiguity):

```bash
# When only one build configuration exists:
py -3 run-unit-tests.py -s

# When both Debug and Release exist, specify the build output directory:
py -3 run-unit-tests.py -s <build-output-dir>
# e.g., py -3 run-unit-tests.py -s C:\work\git\librealsense\build\Release
```

For full usage and all available flags:

```bash
py -3 run-unit-tests.py --help
```

## Running Specific Tests

### By Name (Regex)

Use `-r` / `--regex` to run tests whose names match a regular expression:

```bash
py -3 run-unit-tests.py -s -r "test-hdr"
py -3 run-unit-tests.py -s -r "test-metadata"
py -3 run-unit-tests.py -s --regex "test-stream.*"
```

### Skip Tests by Name (Regex)

Use `--skip-regex` to exclude tests whose names match:

```bash
py -3 run-unit-tests.py -s --skip-regex "test-fw-update"
```

### By Tag

Use `-t` / `--tag` to run tests with a specific tag. Tags are assigned automatically based on:
- File type: `exe` (C++ binaries) or `py` (Python scripts)
- Directory location: e.g., tests in `unit-tests/live/` get the `live` tag

```bash
py -3 run-unit-tests.py -s -t live          # run only live tests
py -3 run-unit-tests.py -s -t py            # run only Python tests
py -3 run-unit-tests.py -s -t exe           # run only compiled C++ tests
py -3 run-unit-tests.py -s -t live -t exe   # run tests that have BOTH tags
```

### By Device

Run tests only on a specific device (implies `--live`):

```bash
py -3 run-unit-tests.py -s --device "D435"
py -3 run-unit-tests.py -s --device "D455"
py -3 run-unit-tests.py -s --exclude-device "D405"
```

### Live vs Non-Live

```bash
py -3 run-unit-tests.py -s --live           # only tests requiring hardware
py -3 run-unit-tests.py -s --not-live       # only tests that don't need hardware
```

## Listing Available Tests and Tags

```bash
# List all available tests
py -3 run-unit-tests.py --list-tests

# List all available tags
py -3 run-unit-tests.py --list-tags

# List tests with their tags
py -3 run-unit-tests.py --list-tests --list-tags
```

## Output Control

```bash
py -3 run-unit-tests.py -s               # direct output to stdout (not log files)
py -3 run-unit-tests.py -v               # verbose — dump log on errors
py -3 run-unit-tests.py -q               # quiet — rely on exit status only
py -3 run-unit-tests.py --rslog          # enable LibRS debug logging in tests
```

## Repeating and Retrying

```bash
py -3 run-unit-tests.py --repeat 3       # repeat each test 3 times
py -3 run-unit-tests.py --retry 2        # retry failed tests up to 2 times
```

## Recording and Playback (Mock Hardware)

Tests can be recorded and replayed without a physical device:

```bash
# Record test execution to a file
./live-test into <filename>

# Replay tests from recorded data (no hardware needed)
./live-test from <filename>
```

This is useful for:
- Debugging without a device
- Distinguishing hardware issues from software bugs
- Running tests in CI without physical cameras

## CMake-Level Test Configuration

The `UNIT_TESTS_ARGS` CMake variable passes arguments to `unit-test-config.py` during configuration:

```bash
cmake .. -DBUILD_UNIT_TESTS=ON -DUNIT_TESTS_ARGS="-t live -r test-streaming"
```

## Using a Custom Test Directory

```bash
py -3 run-unit-tests.py --test-dir /path/to/custom/tests
```

## Custom Firmware for Testing

```bash
py -3 run-unit-tests.py --custom-fw-d400 /path/to/firmware.bin
py -3 run-unit-tests.py --custom-fw-d555 /path/to/firmware.bin
```

## Troubleshooting

- If tests fail due to missing `pyrealsense2`, ensure `-DBUILD_PYTHON_BINDINGS=ON` was set during the CMake configure step
- `BUILD_EASYLOGGINGPP` must be `ON` (default) for unit tests to work
- Make sure you are running from the `unit-tests/` directory
- For live tests, make sure the RealSense device is connected and accessible (on Linux, udev rules may need to be installed)
- On Linux, you may need to run with appropriate permissions or install udev rules from `config/`
