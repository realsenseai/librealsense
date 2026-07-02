# rs-safety-stream-collect Tool

## Goal

Stream the Safety stream from a connected device and dump every frame's metadata to
stdout, a file, or CSV - live.

## Description

The Safety stream (`RS2_STREAM_SAFETY`, `RS2_FORMAT_Y8`) carries almost all of its
useful signal (level1/level2, verdicts, HaRa events, preset integrity, SMCU state,
FuSa events/actions, GPIOs, timings, etc.) as **per-frame metadata**. This tool
mirrors what `realsense-viewer` does when it renders that panel: iterate every
`rs2_frame_metadata_value`, keep the ones the frame supports, and print the raw
integer value.

The tool intentionally has **no hardcoded knowledge of individual safety fields**.
When a new `RS2_FRAME_METADATA_SAFETY_*` entry is added to the C API it appears in
the output automatically - no code change here. For human-readable decoding of
bitmasks and enums use the `realsense-viewer`.

## Command Line Parameters

| Flag | Description | Default |
|---|---|---|
| `-f <path>` | Output file (stdout if omitted, in console mode) | |
| `-F <fmt>` | Output format: `console` or `csv` | `console` |
| `-s <sn>` | Device serial number (defaults to first device) | |
| `-t <sec>` | Stop after N seconds (0 = until Ctrl-C) | 0 |
| `-m <n>` | Stop after N frames (0 = unlimited) | 0 |
| `-r <fps>` | Safety stream FPS | 30 |
| `-w <ms>` | `wait_for_frames` timeout | 5000 |

## Examples

Stream to console until Ctrl-C:

```
rs-safety-stream-collect
```

Stream 100 frames and exit:

```
rs-safety-stream-collect -m 100
```

CSV, 60 seconds:

```
rs-safety-stream-collect -F csv -f safety.csv -t 60
```

Pick device by serial:

```
rs-safety-stream-collect -s 123456789012 -m 50
```

## Notes

- The Safety stream is only exposed on D585S-class devices with the safety sensor
  enumerated (see `src/ds/d500/d500-safety.cpp`).
- CSV column set is fixed on the first frame received; subsequent frames use the
  same column layout.
- For a Python equivalent, see `wrappers/python/examples/safety-stream-collect.py`.
