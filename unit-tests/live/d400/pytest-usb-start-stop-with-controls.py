# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
USB start/stop stress with concurrent sync UVC controls (RSDEV-16582, escapee of RSDSO-21579).

FW up to 5.17.3.18 could wedge EP0 (and kill the USB host controller) when a UVC control transfer overlapped the
status phase of another one (RSDSO-20058, fixed in 5.17.3.20). Plain start/stop never hit it (6,043 clean
cycles); adding visual_preset and laser_power writes per cycle wedged a Jetson at cycle 19 (Gamma SET_CUR, errno 110).

Every cycle: open and start depth + IR 1 at 1280x720@30 (plus accel and gyro where the device has a motion sensor),
collect ~2 s, write visual_preset, laser_power and gamma in the middle of the stream, stop and close. The test fails on:
  - any exception from a control write or from open/start/stop/close (control-transfer timeout, errno 110, ...)
  - a cycle that does not finish within CYCLE_DEADLINE_S (a wedged EP0 blocks the call forever)
  - a stream that delivers fewer than MIN_FRAMES_PER_STREAM frames in a cycle, or fewer than
    MIN_FRAMES_AFTER_WRITES frames after the control writes
  - the device leaving the bus (devices-changed removal event or missing from the context)
The first failure ends the loop: continuing against a wedged device only hangs. Transient GET_CUR -32 kernel
messages are not visible to this test and are benign.

Cycles: CYCLES_NIGHTLY in the nightly test, CYCLES_WEEKLY in the weekly one (--context weekly). On Jetson,
CYCLES_JETSON in both: LRS development Python segfaults ~1 in 500 cycles there, so long runs would crash pytest;
30 still covers the cycle-19 wedge.
Frames are only counted in sensor callbacks (wait_for_frames fabricates drops, RSDSO-21096); Linux motion frame
numbers are host side (RSDSO-21711), so no frame-number check is done. A wedge can hang the whole USB bus: wedged
cameras need a PDU/VBUS power cycle.
Expected to fail on FW <= 5.17.3.18 within ~20 cycles and pass on 5.17.3.20 or newer.
"""

import threading
import time
import logging

import pytest
import pyrealsense2 as rs
from rspy.pytest.device_helpers import is_jetson_platform

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_type("USB"),
]

CYCLES_NIGHTLY = 100
CYCLES_WEEKLY = 1000
CYCLES_JETSON = 30           # LRS dev Python segfaults ~1 in 500 cycles on Jetson; the reference wedge hit at cycle 19
CYCLE_DEADLINE_S = 60        # a cycle takes ~4-7 s; beyond this the control channel is considered wedged
CANCEL_GRACE_S = 10          # after the deadline, time the cycle thread gets to finish its stop/close
HALF_CYCLE_S = 1.0           # streaming time before and after the control writes
MIN_FRAMES_PER_STREAM = 10
MIN_FRAMES_AFTER_WRITES = 5  # a stream that dies after the control writes must not pass on its first-half frames
WIDTH, HEIGHT, FPS = 1280, 720, 30

EP0_HINT = "RSDSO-21579 / RSDSO-20058: EP0 status-phase race, fixed in FW 5.17.3.20"


class CycleHung(Exception):
    pass


def _find_video_profile(sensor, stream, fmt, index):
    return next((p for p in sensor.profiles
                 if p.stream_type() == stream and p.stream_index() == index and p.format() == fmt and p.fps() == FPS
                 and p.as_video_stream_profile().width() == WIDTH and p.as_video_stream_profile().height() == HEIGHT),
                None)


def _pick_motion_profiles(motion):
    """Default accel + gyro profiles; else the highest fps exposed for both. Returns [] when not found."""
    accel = [p for p in motion.profiles if p.stream_type() == rs.stream.accel]
    gyro = [p for p in motion.profiles if p.stream_type() == rs.stream.gyro]
    defaults = [p for p in accel + gyro if p.is_default()]
    if len(defaults) == 2 and {p.stream_type() for p in defaults} == {rs.stream.accel, rs.stream.gyro}:
        return defaults
    common = {p.fps() for p in accel} & {p.fps() for p in gyro}
    if not common:
        return []
    fps = max(common)
    return [next(p for p in accel if p.fps() == fps), next(p for p in gyro if p.fps() == fps)]


def _build_control_writers(dev, depth_sensor):
    """[(name, sensor, option, writer)]; writer(cycle) sets the control to a value that alternates every cycle."""
    writers = []

    def add(sensor, option, values):
        def write(cycle):
            sensor.set_option(option, values[cycle % len(values)])
        writers.append((str(option).split('.')[-1], sensor, option, write))

    if depth_sensor.supports(rs.option.visual_preset):
        add(depth_sensor, rs.option.visual_preset,
            [int(rs.rs400_visual_preset.high_accuracy), int(rs.rs400_visual_preset.default)])
    if depth_sensor.supports(rs.option.laser_power):
        r = depth_sensor.get_option_range(rs.option.laser_power)
        add(depth_sensor, rs.option.laser_power, [r.max, r.min])
    gamma_sensor = next((s for s in dev.query_sensors() if s.supports(rs.option.gamma)), None)
    if gamma_sensor is not None:
        r = gamma_sensor.get_option_range(rs.option.gamma)
        add(gamma_sensor, rs.option.gamma, [r.max, r.min])
    return writers


def _run_with_deadline(fn, deadline_s):
    """Run fn(cancel) on a daemon thread. On timeout set cancel, give the thread CANCEL_GRACE_S to finish its
    native stop/close calls, then raise CycleHung (a call blocked on a wedged control endpoint never returns)."""
    result = {}
    cancel = threading.Event()

    def target():
        try:
            result["value"] = fn(cancel)
        except BaseException as e:  # reported to the caller
            result["error"] = e

    worker = threading.Thread(target=target, daemon=True)
    worker.start()
    worker.join(deadline_s)
    if worker.is_alive():
        cancel.set()
        worker.join(CANCEL_GRACE_S)
        raise CycleHung(f"no completion within {deadline_s} s" +
                        ("; cycle thread still blocked in a native call" if worker.is_alive() else ""))
    if "error" in result:
        raise result["error"]
    return result["value"]


def _teardown(cycle, started, opened):
    """Stop and close what the cycle opened; returns the first error (every error is logged)."""
    first_error = None
    for action, sensors in (("stop", started), ("close", opened)):
        for sensor in reversed(sensors):
            try:
                getattr(sensor, action)()
            except Exception as e:
                log.warning("[cycle %d] %s failed: %s", cycle, action, e)
                first_error = first_error or e
    return first_error


def _stream_cycle(cycle, cancel, depth_sensor, video_profiles, motion, motion_profiles, writers):
    """One open/start/write-controls/stop/close cycle. Returns ({stream: frames}, {stream: frames after writes})."""
    counts = {}
    lock = threading.Lock()

    def on_frame(frame):
        stream = frame.get_profile().stream_type()
        with lock:
            counts[stream] = counts.get(stream, 0) + 1

    opened, started = [], []
    before_writes = {}
    try:
        depth_sensor.open(video_profiles)
        opened.append(depth_sensor)
        if motion is not None:
            motion.open(motion_profiles)
            opened.append(motion)
        depth_sensor.start(on_frame)
        started.append(depth_sensor)
        if motion is not None:
            motion.start(on_frame)
            started.append(motion)

        time.sleep(HALF_CYCLE_S)
        with lock:
            before_writes = dict(counts)
        for name, _, _, write in writers:
            if cancel.is_set():
                break
            log.debug("[cycle %d] set %s", cycle, name)
            write(cycle)
        time.sleep(HALF_CYCLE_S)
    except BaseException:
        _teardown(cycle, started, opened)  # the original error wins over teardown errors
        raise
    # Teardown sends control traffic too: a stop/close error fails the cycle.
    teardown_error = _teardown(cycle, started, opened)
    if teardown_error is not None:
        raise teardown_error
    with lock:
        total = dict(counts)
    return total, {s: n - before_writes.get(s, 0) for s, n in total.items()}


def _run_stress(test_device, cycles):
    dev, ctx = test_device
    name = dev.get_info(rs.camera_info.name)
    sn = dev.get_info(rs.camera_info.serial_number)
    fw = dev.get_info(rs.camera_info.firmware_version)

    depth_sensor = dev.first_depth_sensor()
    depth_profile = _find_video_profile(depth_sensor, rs.stream.depth, rs.format.z16, 0)
    ir_profile = _find_video_profile(depth_sensor, rs.stream.infrared, rs.format.y8, 1)
    if depth_profile is None or ir_profile is None:
        pytest.skip(f"{name}: no depth z16 + infrared y8 {WIDTH}x{HEIGHT}@{FPS} profiles")
    video_profiles = [depth_profile, ir_profile]
    expected = {rs.stream.depth, rs.stream.infrared}

    try:
        motion = dev.first_motion_sensor()
    except RuntimeError:
        motion = None
    motion_profiles = _pick_motion_profiles(motion) if motion is not None else []
    if motion is not None and not motion_profiles:
        pytest.fail(f"{name}: motion sensor exposes no accel/gyro profile pair")
    if motion is not None:
        expected |= {rs.stream.accel, rs.stream.gyro}

    writers = _build_control_writers(dev, depth_sensor)
    if not writers:
        pytest.skip(f"{name}: none of visual_preset, laser_power, gamma is supported")
    original_options = [(sensor, option, sensor.get_option(option)) for _, sensor, option, _ in writers]

    removed = threading.Event()

    def on_devices_changed(info):
        try:
            if info.was_removed(dev):
                removed.set()
        except RuntimeError:
            pass
    ctx.set_devices_changed_callback(on_devices_changed)

    if is_jetson_platform():
        cycles = CYCLES_JETSON
    log.info("%s %s FW %s: %d cycles, streams %s, controls %s", name, sn, fw, cycles,
             sorted(s.name for s in expected), [w[0] for w in writers])

    def device_on_bus(_cancel):
        return any(d.supports(rs.camera_info.serial_number) and d.get_info(rs.camera_info.serial_number) == sn
                   for d in ctx.query_devices())

    def fail(cycle, reason):
        pytest.fail(f"{name} {sn} FW {fw}: cycle {cycle}/{cycles}: {reason}\n  {EP0_HINT}\n"
                    f"  A wedged camera needs a PDU/VBUS power cycle.", pytrace=False)

    try:
        for cycle in range(1, cycles + 1):
            try:
                counts, after_writes = _run_with_deadline(
                    lambda cancel: _stream_cycle(cycle, cancel, depth_sensor, video_profiles, motion,
                                                 motion_profiles, writers),
                    CYCLE_DEADLINE_S)
            except CycleHung as e:
                fail(cycle, f"cycle hung ({e}); control channel wedged")
            except Exception as e:
                fail(cycle, f"control transfer / streaming error: {e}")

            if removed.is_set():
                fail(cycle, "device removal event received")
            try:
                on_bus = _run_with_deadline(device_on_bus, CYCLE_DEADLINE_S)
            except Exception as e:
                fail(cycle, f"cannot enumerate devices: {e}")
            if not on_bus:
                fail(cycle, "device no longer on the bus")

            starved = {s.name: counts.get(s, 0) for s in expected if counts.get(s, 0) < MIN_FRAMES_PER_STREAM}
            if starved:
                fail(cycle, f"streams delivered fewer than {MIN_FRAMES_PER_STREAM} frames: {starved}")
            stalled = {s.name: after_writes.get(s, 0) for s in expected
                       if after_writes.get(s, 0) < MIN_FRAMES_AFTER_WRITES}
            if stalled:
                fail(cycle, f"streams delivered fewer than {MIN_FRAMES_AFTER_WRITES} frames after the control "
                            f"writes: {stalled}")

            if cycle % 20 == 0 or cycle == 1:
                log.info("cycle %d/%d ok: %s", cycle, cycles, {s.name: c for s, c in counts.items()})
    finally:
        ctx.set_devices_changed_callback(lambda info: None)
        for sensor, option, value in original_options:
            try:
                sensor.set_option(option, value)
            except Exception as e:  # device may be wedged or gone; the failure is already reported
                log.debug("could not restore %s: %s", option, e)


@pytest.mark.context("nightly")
@pytest.mark.timeout(1800)
def test_usb_start_stop_with_controls_nightly(test_device, test_context_var):
    if "weekly" in test_context_var:
        pytest.skip("covered by the weekly soak")
    _run_stress(test_device, CYCLES_NIGHTLY)


@pytest.mark.context("weekly")
@pytest.mark.timeout(10800)
def test_usb_start_stop_with_controls_weekly(test_device):
    _run_stress(test_device, CYCLES_WEEKLY)
