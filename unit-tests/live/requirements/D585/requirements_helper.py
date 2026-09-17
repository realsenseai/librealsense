# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Shared engine for the D585 HAS D5X5 §6.5 (Table 19) stream-profile conformance tests.

Each per-SKU test file (2C, 3C, ...) defines its own requirement set + device marker
and calls compare_and_assert() (enumeration) / stream_each_profile() (weekly streaming).
This module handles enumeration, comparison, and streaming.

References:
  HAS D5X5 V0p65 §6.5 Table 19:
    https://realsensecloud.sharepoint.com/:w:/r/sites/allcompany/_layouts/15/Doc.aspx?sourcedoc=%7B68C45603-CBDD-48E2-8A7F-F405018CF6DF%7D&file=D5X5_HAS_V0p65.docx
  RSDEV-9250 (D585 all-streams res/fps): https://rsjira.realsenseai.com/browse/RSDEV-9250

Format handling
---------------
The requirement is written in raw FW format names (Z16 / Y8I / NV12 / YUY2 / MJPEG /
Y12I), but the SDK exposes the same capability under several fourccs that differ by
stream (e.g. the IR pin advertises y8 + y8i + y16i + m420 + uyvy for the same frame).
So a requirement matches on a FORMAT FAMILY plus the (stream, width, height, fps) tuple.
The full raw fourcc list is dumped to the log regardless.

Enumeration mode
----------------
We prefer 'format-conversion' = 'raw' so the SDK does not add/aggregate profiles via
its converters - what we enumerate is what the firmware publishes. If the raw path
raises on a given device we fall back to 'full' and flag it.
"""

import pytest
import pyrealsense2 as rs
import logging
import time
from collections import defaultdict
from rspy.pytest.device_helpers import select_target_device

log = logging.getLogger(__name__)

# Stream types covered by the section 3.4 / HAS 6.4 requirement (depth, IR L+R, color;
# the calibration entries are carried on these same stream types).
_REQ_STREAMS = {rs.stream.depth, rs.stream.infrared, rs.stream.color}


# Resolution -> required frame rates. 90 fps is required only on the small resolutions.
# 896x504 is intentionally omitted: dropped from the D585 requirement (see RSDEV-9250).
RES_FPS = {
    (1280, 960): [5, 15, 30, 60],
    (1280, 720): [5, 15, 30, 60],
    (848,  480): [5, 15, 30, 60],
    (640,  480): [5, 15, 30, 60, 90],
    (640,  360): [5, 15, 30, 60, 90],
    (480,  270): [5, 15, 30, 60, 90],
    (424,  240): [5, 15, 30, 60, 90],
}

# Format families: spec format -> SDK-equivalent rs.format set.
FAM_DEPTH = {rs.format.z16}
FAM_IR    = {rs.format.y8, rs.format.y8i}                                       # L/R imager luminance
FAM_CALIB = {rs.format.y12i, rs.format.y16i, rs.format.y16}                     # calibration IR imager
FAM_COLOR = {rs.format.nv12, rs.format.yuyv, rs.format.m420, rs.format.rgb8, rs.format.uyvy}
FAM_COLOR_COMPRESSED = FAM_COLOR | {rs.format.mjpeg}                            # 3rd RGB camera (+MJPEG)


def video_reqs(stream, fam_name, fam_set, res_fps=RES_FPS):
    """Expand a {resolution: [fps,...]} table into requirement tuples
    (stream, family_name, frozenset(family), w, h, fps)."""
    return {(stream, fam_name, frozenset(fam_set), w, h, fps)
            for (w, h), fps_list in res_fps.items()
            for fps in fps_list}


def calib_reqs(include_selfcal=True):
    """Calibration formats:
      - IR imager Y12I : 1600x1300 @15/25  (interleaved calibration format)
      - Self-cal/tare  : 256x144   @90     (published as Z16/Y8, both accepted)

    The 256x144 self-cal/tare format is only published in dedicated-color (3C) mode;
    2C (Dual RGB) mode exposes no 256x144 profiles, so pass include_selfcal=False there."""
    calib = frozenset(FAM_CALIB)
    reqs = {
        (rs.stream.infrared, "Y12I", calib, 1600, 1300, 15),
        (rs.stream.infrared, "Y12I", calib, 1600, 1300, 25),
    }
    if include_selfcal:
        selfcal = frozenset(FAM_DEPTH | FAM_IR)  # Z16 / Y8 both OK for the 256x144 tare format
        reqs.add((rs.stream.infrared, "Y8", selfcal, 256, 144, 90))
    return reqs


def enumerate_profiles(module_device_setup):
    """Return (device, ctx, profiles, mode). profiles: list of (sensor, stream, index, format, w, h, fps).
    Caller must keep `ctx` referenced for the device handle's lifetime."""
    for mode in ("raw", "full"):
        ctx = rs.context({"device-mask": 0xfe, "format-conversion": mode})
        dev = select_target_device(list(ctx.devices), module_device_setup)
        profiles = []
        try:
            for sensor in dev.query_sensors():
                sname = sensor.get_info(rs.camera_info.name) if sensor.supports(rs.camera_info.name) else "?"
                for p in sensor.get_stream_profiles():
                    vp = p.as_video_stream_profile()
                    w = vp.width() if vp else 0
                    h = vp.height() if vp else 0
                    profiles.append((sname, p.stream_type(), p.stream_index(), p.format(), w, h, p.fps()))
        except RuntimeError as e:
            log.warning(f"'{mode}' format-conversion enumeration failed ({e}); falling back")
            continue
        return dev, ctx, profiles, mode
    pytest.fail("Could not enumerate stream profiles in either 'raw' or 'full' mode")


def _skip_if_usb2(dev, name):
    """The HAS 6.5 matrix assumes full USB3 bandwidth. On a USB2 link the FW throttles the
    high-fps profiles, so both enumeration and streaming would false-fail."""
    if dev.supports(rs.camera_info.usb_type_descriptor):
        usb = dev.get_info(rs.camera_info.usb_type_descriptor)
        if usb.startswith("2"):
            pytest.skip(f"{name} enumerated on USB {usb}; requirements assume USB3 - reconnect on a USB3 link")


def _log_profiles(raw_profiles, mode):
    """Dump the FW-published profiles grouped by sensor; return the set of color stream indices."""
    by_sensor = defaultdict(list)
    color_indices = set()
    for sname, st, idx, fmt, w, h, fps in raw_profiles:
        by_sensor[sname].append((st, idx, fmt, w, h, fps))
        if st == rs.stream.color:
            color_indices.add(idx)
    log.info(f"FW-published profiles ({mode}):")
    for sname in sorted(by_sensor):
        log.info(f"  Sensor: {sname}")
        for st, idx, fmt, w, h, fps in sorted(by_sensor[sname], key=lambda e: (str(e[0]), e[1], str(e[2]), e[3], e[4], e[5])):
            log.info(f"    {st} idx={idx} {fmt} {w}x{h} @{fps}")
    return color_indices


def _color_full_indices(device_tuples, color_indices, required):
    """Color must be verified PER INDEX: the dual-RGB SKU exposes the matrix on each imager
    (Color 1 and Color 2), so a profile on one index must not mask a gap on another. Returns
    the sorted color indices that individually carry every color requirement."""
    color_reqs = [(fam_set, w, h, fps) for (stream, _n, fam_set, w, h, fps) in required if stream == rs.stream.color]

    def covers(idx):
        return all(any(dst == rs.stream.color and di == idx and dfmt in fam and dw == w and dh == h and dfps == fps
                       for (dst, di, dfmt, dw, dh, dfps) in device_tuples)
                   for (fam, w, h, fps) in color_reqs)

    return sorted(i for i in color_indices if covers(i)), len(color_reqs)


def _missing_non_color(device_tuples, required):
    """Non-color streams (depth, IR) are carried on a single index, so they match
    index-agnostically. Returns the sorted list of missing (stream, family, w, h, fps)."""
    missing = [(stream, fam_name, w, h, fps)
               for (stream, fam_name, fam_set, w, h, fps) in required
               if stream != rs.stream.color
               and not any(dst == stream and dfmt in fam_set and dw == w and dh == h and dfps == fps
                           for (dst, di, dfmt, dw, dh, dfps) in device_tuples)]
    missing.sort(key=lambda r: (str(r[0]), r[1], r[2], r[3], r[4]))
    return missing


def compare_and_assert(module_device_setup, required, expected_color_streams):
    """Enumerate the target device, compare its profiles to `required`, and assert. Fails if any
    non-color capability is missing, or fewer than `expected_color_streams` color imagers expose
    the full color matrix."""
    dev, ctx, raw_profiles, mode = enumerate_profiles(module_device_setup)  # keep `ctx` alive
    name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
    serial = dev.get_info(rs.camera_info.serial_number) if dev.supports(rs.camera_info.serial_number) else "?"
    fw = dev.get_info(rs.camera_info.firmware_version) if dev.supports(rs.camera_info.firmware_version) else "?"
    _skip_if_usb2(dev, name)
    log.info(f"Device under test: {name} (s/n {serial}), FW {fw}  [format-conversion={mode}]")
    if mode != "raw":
        log.warning("Raw enumeration unavailable on this device; profile/format list reflects SDK conversion.")

    color_indices = _log_profiles(raw_profiles, mode)
    device_tuples = [(st, idx, fmt, w, h, fps) for _, st, idx, fmt, w, h, fps in raw_profiles]
    color_full, n_color = _color_full_indices(device_tuples, color_indices, required)
    missing = _missing_non_color(device_tuples, required)

    log.info(f"Requirement coverage: non-color {len(required) - n_color - len(missing)}/{len(required) - n_color}; "
             f"color imagers with full matrix: {len(color_full)}/{expected_color_streams} (indices {color_full})")
    for stream, fam_name, w, h, fps in missing:
        log.error(f"    MISSING {stream} [{fam_name}] {w}x{h} @{fps}")

    failures = []
    if missing:
        failures.append(f"{len(missing)} required non-color capabilities missing (see log)")
    if len(color_full) < expected_color_streams:
        failures.append(f"expected {expected_color_streams} color imager(s) exposing the full matrix, "
                        f"found {len(color_full)} (color indices present: {sorted(color_indices)})")

    assert not failures, f"{name} profile requirements not met:\n" + "\n".join(failures)


def _stream_one(sensor, profile, first_frame_timeout, count_window):
    """Open+start one profile, wait up to first_frame_timeout for the first frame, then
    collect for count_window more seconds. Returns (frame_count, time_to_first_frame|None)."""
    state = {"n": 0, "t0": None}

    def on_frame(f, state=state):
        state["n"] += 1
        if state["t0"] is None:
            state["t0"] = time.time()

    # open pairs with close, start pairs with stop - nested so a failure in start() (or the
    # wait) still releases the sensor and never leaves it open/streaming for the next profile.
    t_start = None
    sensor.open(profile)
    try:
        t_start = time.time()
        sensor.start(on_frame)
        try:
            while state["t0"] is None and time.time() - t_start < first_frame_timeout:
                time.sleep(0.02)
            if state["t0"] is not None:
                time.sleep(count_window)  # let a few more frames accumulate
        finally:
            sensor.stop()
    finally:
        sensor.close()
    ttff = (state["t0"] - t_start) if state["t0"] else None
    return state["n"], ttff


def stream_each_profile(module_device_setup, first_frame_timeout=4.0, count_window=0.3, min_frames=1):
    """Nightly stage: open every depth/IR/color profile the device exposes and verify frames
    arrive. One profile per (stream, index, w, h, fps) - the first format - is streamed.

    Uses the default ('full') context: 'raw' format-conversion cannot stream. Skips on USB2
    (the requirement assumes USB3 bandwidth). Fails listing any profile that produced no frame."""
    ctx = rs.context({"device-mask": 0xfe})  # keep `ctx` alive for the device handle
    dev = select_target_device(list(ctx.devices), module_device_setup)
    name = dev.get_info(rs.camera_info.name) if dev.supports(rs.camera_info.name) else "Unknown"
    fw = dev.get_info(rs.camera_info.firmware_version) if dev.supports(rs.camera_info.firmware_version) else "?"
    _skip_if_usb2(dev, name)
    log.info(f"Streaming check: {name}, FW {fw}")

    # One representative profile per (stream, index, w, h, fps).
    cells = {}
    for sensor in dev.query_sensors():
        for p in sensor.get_stream_profiles():
            if p.stream_type() not in _REQ_STREAMS:
                continue
            vp = p.as_video_stream_profile()
            if not vp:
                continue
            key = (p.stream_type(), p.stream_index(), vp.width(), vp.height(), p.fps())
            cells.setdefault(key, (sensor, p))

    log.info(f"Streaming {len(cells)} profiles (>= {min_frames} frame each)")
    failures = []
    for (st, idx, w, h, fps), (sensor, p) in cells.items():
        tag = f"{st} idx{idx} {w}x{h}@{fps} [{p.format()}]"
        try:
            got, ttff = _stream_one(sensor, p, first_frame_timeout, count_window)
        except RuntimeError as e:
            failures.append(f"{tag}: open/stream error - {e}")
            log.error(f"    ERROR   {tag}: {e}")
            continue
        if got < min_frames:
            failures.append(f"{tag}: {got} frames (need >= {min_frames})")
            log.error(f"    NOFRAME {tag}: {got} frames")
        else:
            log.debug(f"    ok {tag}: {got} frames, ttff {ttff:.2f}s")

    log.info(f"Streamed {len(cells) - len(failures)}/{len(cells)} profiles OK")
    assert not failures, f"{name} - profiles that failed to stream:\n" + "\n".join(failures)
