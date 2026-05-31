# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:device D585S

import os
import pyrealsense2 as rs
from rspy import test, log
from rspy.d500_log import start_cdc_log
from rspy.host_trace import start_dmesg_log
import time

device, _ = test.find_first_device_or_exit();

sensor_names = [s.get_info(rs.camera_info.name) for s in device.sensors
                if s.supports(rs.camera_info.name)]
log.d("Enumerated sensors:", sensor_names)

_test_base = os.path.splitext(os.path.basename(__file__))[0]
_cdc   = start_cdc_log(_test_base + "-cdc")
_smcu  = start_cdc_log(_test_base + "-smcu", device_path="/dev/ttyUSB1", baud=460800)
_dmesg = start_dmesg_log(_test_base)

def verify_frames_received(pipe, count):
    for i in range(count):
        # no check is needed, assume wait_for_frames will raise exception if not frames arrive
        fs = pipe.wait_for_frames()
        if len(fs) > 1:
            for f in fs:
                log.d(f)
        else:
            log.d(fs)

########################### SRS - 3.3.1.14.b ##############################################

with test.closure("Pause / Resume - no impact on streaming"):

    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    pipe = rs.pipeline()
    profile = pipe.start(cfg)
    f = pipe.wait_for_frames()

    pipeline_device = profile.get_device()
    safety_sensor = pipeline_device.first_safety_sensor()
    log.d( "Verify default is run mode" )
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run)) # verify default

    log.d( "Command standby mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.standby)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.standby))
    verify_frames_received(pipe, count = 10)

    pipe.stop()
    time.sleep(1) # allow some time for the streaming to actually stop
    pipe.start(cfg)
    verify_frames_received(pipe, count = 10)

    log.d( "Command run mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
    verify_frames_received(pipe, count = 10)

    pipe.stop()

########################### SRS - 3.3.1.14.c ##############################################

with test.closure("Resume --> Maintenance keep video streaming"):

    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)

    pipe = rs.pipeline()
    profile = pipe.start(cfg)

    f = pipe.wait_for_frames()

    pipeline_device = profile.get_device()
    safety_sensor = pipeline_device.first_safety_sensor()

    log.d( "Command run mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
    # Verify that on RUN mode we get frames
    verify_frames_received(pipe, count = 10)

    log.d( "Command service mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.service))
    verify_frames_received(pipe, count = 10)

    # Restore Run mode
    log.d( "Command run mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
    # Verify that on RUN mode we get frames
    verify_frames_received(pipe, count = 10)

    pipe.stop()

########################### SRS - 3.3.1.14.c ##############################################

with test.closure("Resume --> Maintenance keeps safety streaming on"):

    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)

    pipe = rs.pipeline()
    profile = pipe.start(cfg)

    f = pipe.wait_for_frames()

    pipeline_device = profile.get_device()
    safety_sensor = pipeline_device.first_safety_sensor()

    log.d( "Command run mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))
    # Verify that on RUN mode we get frames
    verify_frames_received(pipe, count = 10)

    log.d( "Command service mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.service)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.service))
    # Verify that on SERVICE mode we still get frames
    verify_frames_received(pipe, count = 10)

    # Restore Run mode
    log.d( "Command run mode" )
    safety_sensor.set_option(rs.option.safety_mode, rs.safety_mode.run)
    test.check_equal( safety_sensor.get_option(rs.option.safety_mode), float(rs.safety_mode.run))

    # We know that returning to run mode will not restart the safety stream.
    # FW expect the user to restart the stream at host side
    pipe.stop()
    time.sleep(1) # allow some time for the streaming to actually stop
    pipe.start(cfg)

    # Verify that on RUN mode we get frames
    verify_frames_received(pipe, count = 10)

    pipe.stop()

############################## DIAGNOSTIC PROBES ##############################################
# Each probe primes the failure state with a baseline safety+depth+color session,
# stops it cleanly, sleeps, then starts a variant configuration and tries to
# receive a single frame. The variant that does NOT time out localizes the
# trigger of "Resume --> Maintenance keep video streaming" failing on some
# hosts (e.g. vtglnx163, vtglnx164) but not others (e.g. rslnx391, vtgu24).
#
# Interpretation matrix (assuming the canonical probe fails):
#   "drop safety" passes      -> safety stream specifically is the trigger
#   "drop color"  passes      -> color stream specifically is the trigger
#   "drop depth"  passes      -> depth stream specifically is the trigger
#   "5s settle"   passes      -> race/cleanup-time issue, not stream-set
#   "no priming"  passes      -> needs prior multi-stream session to break

def _drain_one_frame(pipe, label):
    log.d(f"diagnostic {label}: waiting for first frame")
    f = pipe.wait_for_frames()  # default 5s timeout -- raises if it doesn't arrive
    log.d(f"diagnostic {label}: got frame {f}")

def _prime_baseline():
    """Run a quick safety+depth+color session and stop it cleanly. Models the
    state Closure 2 sees when it tries to start the same combo a second time."""
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth,  rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color,  rs.format.rgb8, 30)
    pipe = rs.pipeline()
    pipe.start(cfg)
    pipe.wait_for_frames()
    pipe.stop()

def _run_probe(label, probe_cfg, prime=True, sleep_sec=1):
    if prime:
        _prime_baseline()
    time.sleep(sleep_sec)
    pipe = rs.pipeline()
    pipe.start(probe_cfg)
    try:
        _drain_one_frame(pipe, label)
    finally:
        pipe.stop()

with test.closure("diag: canonical (safety+depth+color restart, 1s settle)"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth,  rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color,  rs.format.rgb8, 30)
    _run_probe("canonical", cfg)

with test.closure("diag: drop safety (depth+color restart)"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.depth, rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color, rs.format.rgb8, 30)
    _run_probe("drop-safety", cfg)

with test.closure("diag: drop color (safety+depth restart)"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth,  rs.format.z16, 30)
    _run_probe("drop-color", cfg)

with test.closure("diag: drop depth (safety+color restart)"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.color,  rs.format.rgb8, 30)
    _run_probe("drop-depth", cfg)

with test.closure("diag: full combo with 5s settle"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth,  rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color,  rs.format.rgb8, 30)
    _run_probe("5s-settle", cfg, sleep_sec=5)

with test.closure("diag: full combo without priming (first-ever start)"):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.safety, rs.format.y8, 30)
    cfg.enable_stream(rs.stream.depth,  rs.format.z16, 30)
    cfg.enable_stream(rs.stream.color,  rs.format.rgb8, 30)
    _run_probe("no-priming", cfg, prime=False)

################################################################################################
if _cdc is not None:
    _cdc.stop()
if _smcu is not None:
    _smcu.stop()
if _dmesg is not None:
    _dmesg.stop()

test.print_results_and_exit()
