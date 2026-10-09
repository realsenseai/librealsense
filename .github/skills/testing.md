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

Two frameworks, split by test language:

- **C++ tests** (`test-*.cpp`) are orchestrated by the legacy runner `unit-tests/run-unit-tests.py`.
  Keeping that runner for C++ testing is its only remaining purpose.
- **Python tests** (`pytest-*.py`) are pytest only. See `.github/skills/pytest-infra.md`.
  No legacy `test-*.py` scripts remain, and `run-unit-tests.py` never collects `pytest-*.py`.

Either way, run **from the `unit-tests/` directory**.

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

## Running the pytest Tests

```bash
cd unit-tests
python3 -m pytest -v                        # all collectable tests
python3 -m pytest -v live/frames            # one directory
python3 -m pytest -v pytest-fw-update.py    # one file
python3 -m pytest -v -k "hdr and not preset"   # filter by test name
```

Flags are registered in `conftest.py`: `--live`, `--not-live`, `--device`, `--exclude-device`,
`--context`, `--tag`, `--repeat`, `--reruns`, `--debug`, `--rslog`, `--test-dir`, `--device-map`. Note that `-s`
disables the per-test log files. See `.github/skills/pytest-infra.md` for fixtures and markers.

## Running the C++ Tests (legacy runner)

Navigate to the `unit-tests/` directory and run:

```bash
cd unit-tests
python3 run-unit-tests.py -s
```

The `-s` flag directs test output to stdout. You can also specify the build output directory (required when both Debug and Release builds exist, to avoid ambiguity):

```bash
# When only one build configuration exists:
python3 run-unit-tests.py -s

# When both Debug and Release exist, specify the build output directory:
python3 run-unit-tests.py -s <build-output-dir>
# e.g., python3 run-unit-tests.py -s C:\work\git\librealsense\build\Release
```

For full usage and all available flags:

```bash
python3 run-unit-tests.py --help
```

## Running Specific C++ Tests

The flags below belong to `run-unit-tests.py`, so they select among the C++ tests.
For pytest use `-k`, `-m` and the flags listed above.

### By Name (Regex)

Use `-r` / `--regex` to run tests whose names match a regular expression:

```bash
python3 run-unit-tests.py -s -r "test-log-vs-LOG"
python3 run-unit-tests.py -s --regex "rsutils-string.*"
```

**Test name derivation**: the orchestrator builds a test's name from its path relative to `unit-tests/`, replacing directory separators with `-` and stripping the leading `test-` from the filename. For example:
- `rsutils/string/test-hexarray.cpp` becomes `test-rsutils-string-hexarray`
- `log/test-vs-LOG-shared.cpp` becomes `test-log-vs-LOG-shared`

So when using `-r`, omit the `test-` filename prefix and join subdirectories with `-`.

### Skip Tests by Name (Regex)

Use `--skip-regex` to exclude tests whose names match:

```bash
python3 run-unit-tests.py -s --skip-regex "test-rsutils-string-hexarray"
```

### By Tag

Use `-t` / `--tag` to run tests with a specific tag. Tags come from:
- File type: `exe` (C++ binaries). The `py` tag matches nothing now that no legacy `test-*.py` remain.
- Directory location: e.g., tests in `unit-tests/live/` get the `live` tag
- An explicit `//#test:tag <name>` directive in the source file

```bash
python3 run-unit-tests.py -s -t live          # run only live tests
python3 run-unit-tests.py -s -t exe           # run only compiled C++ tests
python3 run-unit-tests.py -s -t live -t exe   # run tests that have BOTH tags
```

### By Device

Run tests only on a specific device (implies `--live`):

```bash
python3 run-unit-tests.py -s --device "D435"
python3 run-unit-tests.py -s --device "D455"
python3 run-unit-tests.py -s --exclude-device "D405"
```

### Live vs Non-Live

```bash
python3 run-unit-tests.py -s --live           # only tests requiring hardware
python3 run-unit-tests.py -s --not-live       # only tests that don't need hardware
```

## Listing Available Tests and Tags

```bash
# List all available tests
python3 run-unit-tests.py --list-tests

# List all available tags
python3 run-unit-tests.py --list-tags

# List tests with their tags
python3 run-unit-tests.py --list-tests --list-tags
```

## Output Control

```bash
python3 run-unit-tests.py -s               # direct output to stdout (not log files)
python3 run-unit-tests.py -v               # verbose — dump log on errors
python3 run-unit-tests.py -q               # quiet — rely on exit status only
python3 run-unit-tests.py --rslog          # enable LibRS debug logging in tests
python3 run-unit-tests.py --debug          # enable framework debug output; also prints "test took X seconds" per test
```

## Running Nightly-Only and Weekly Tests

Both runners take `--context`, which accepts a **space-separated list**.

pytest tests gated with `@pytest.mark.context("nightly")` are skipped unless the context is passed:

```bash
python3 -m pytest -v live/hw-reset/pytest-stress.py --context nightly
```

A test that also scales its iteration count or timeout for weekly runs reads the context through the
`test_context_var` fixture, so pass both words to get nightly collection plus weekly behaviour:

```bash
python3 -m pytest -v live/hw-reset/pytest-stress.py --context "nightly weekly"
```

C++ tests use a `//#test:donotrun:<context>` directive in the source file, evaluated by
`run-unit-tests.py` against the same `--context` list.

## Repeating and Retrying

```bash
python3 run-unit-tests.py --repeat 3       # repeat each test 3 times
python3 run-unit-tests.py --retry 2        # retry failed tests up to 2 times

python3 -m pytest --repeat 3               # pytest: repeat each file's tests 3 times
python3 -m pytest --reruns 2               # pytest: retry a failed test up to 2 times
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
cmake .. -DBUILD_UNIT_TESTS=ON -DUNIT_TESTS_ARGS="-t live -r rsutils-string.*"
```

## Using a Custom Test Directory

```bash
python3 run-unit-tests.py --test-dir /path/to/custom/tests
```

## Custom Firmware for Testing

The SDK no longer ships a bundled firmware blob, so `pytest-fw-update` **requires** a custom firmware path for the device under test. Without one it skips. Download a signed `.bin` from <https://dev.realsenseai.com/docs/firmware-updates>, then:

```bash
python3 -m pytest pytest-fw-update.py --custom-fw-d400 /path/to/firmware.bin
python3 -m pytest pytest-fw-update.py --custom-fw-d555 /path/to/firmware.bin
python3 -m pytest pytest-fw-update.py --custom-fw-d585 /path/to/firmware.bin
```

`--custom-fw-d585` targets the non-safety D585 only (e.g. "D585 Prototype"). The safety SKU is kept
out of the test by `pytest.mark.device_exclude("D585S")`, so it is never collected and never flashed.

## Troubleshooting

- If tests fail due to missing `pyrealsense2`, ensure `-DBUILD_PYTHON_BINDINGS=ON` was set during the CMake configure step
- `BUILD_EASYLOGGINGPP` must be `ON` (default) for unit tests to work
- Make sure you are running from the `unit-tests/` directory
- For live tests, make sure the RealSense device is connected and accessible (on Linux, udev rules may need to be installed)
- On Linux, you may need to run with appropriate permissions or install udev rules from `config/`
