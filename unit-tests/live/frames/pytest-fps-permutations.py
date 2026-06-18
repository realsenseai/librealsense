# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Test FPS accuracy for all pairwise sensor stream combinations.
Generates all (N choose 2) pairs of streams and verifies FPS for each.
"""

import pytest
import pyrealsense2 as rs
from itertools import combinations
import fps_helper
import logging
from rspy.snippets import is_dds_dev
log = logging.getLogger(__name__)

VGA_RESOLUTION = (640, 360)
HD_RESOLUTION = (1280, 720)

# Resolution tiers exercised by the test. DDS (GigE) devices run both tiers; USB devices
# keep the original HD-only matrix (see test_fps_permutations).
RES_TIERS = {"HD": HD_RESOLUTION, "VGA": VGA_RESOLUTION}

# On DDS (GigE) devices, Color + Depth (Z16) at HD reaches full 30 fps on an unburdened host
# (e.g. Linux CI), but is throttled to a stable ~22 fps on a CPU-burdened Windows CI host where
# background host processes steal CPU from DDS frame reception. Since the same pair legitimately
# runs at either rate depending on the host, assert a one-sided floor instead of the advertised
# 30 fps: pass at >= KPI (with fps_helper's 15% tolerance, i.e. >= ~18.7 fps), no upper bound.
# This tolerates the throttled host while still catching catastrophic frame loss.
COLOR_DEPTH_HD_DDS_FPS_KPI = 22

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D555"),
    pytest.mark.device_exclude("D401"),
    pytest.mark.context("nightly"),
]


def get_sensors_and_profiles(dev, color_resolution=HD_RESOLUTION, depth_resolution=None):
    """Returns an array of pairs of a (sensor, profile) for each of its profiles.

    :param color_resolution: resolution to use for the color stream.
    :param depth_resolution: resolution to use for the depth/infrared streams; None keeps the
                             legacy behavior of picking the first available depth resolution.
    """
    sensor_profiles_arr = []
    for sensor in dev.query_sensors():
        profile = None
        if sensor.is_depth_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            if depth_resolution is not None:
                depth_resolutions = [depth_resolution]
            else:
                depth_resolutions = []
                for p in sensor.get_stream_profiles():
                    res = fps_helper.get_resolution(p)
                    if res not in depth_resolutions:
                        depth_resolutions.append(res)
            for res in depth_resolutions:
                # Skip 1280x800 resolution for infrared since it's Y16 calibration format
                if res == (1280, 800):
                    log.debug(f"Skipping resolution {res} for infrared (calibration format)")
                    continue

                depth = fps_helper.get_profile(sensor, rs.stream.depth, res)
                irs = fps_helper.get_profiles(sensor, rs.stream.infrared, res)
                ir = next(irs, None)
                while ir is not None and ir.stream_index() != 1:
                    ir = next(irs, None)
                if ir and depth:
                    log.debug(f"{ir}, {depth}")
                    sensor_profiles_arr.append((sensor, depth))
                    sensor_profiles_arr.append((sensor, ir))
                    break
        elif sensor.is_color_sensor():
            if sensor.supports(rs.option.enable_auto_exposure):
                sensor.set_option(rs.option.enable_auto_exposure, 1)
            if sensor.supports(rs.option.auto_exposure_priority):
                sensor.set_option(rs.option.auto_exposure_priority, 0)
            profile = fps_helper.get_profile(sensor, rs.stream.color, color_resolution)
        elif sensor.is_motion_sensor():
            if is_dds_dev(dev):
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.motion)))
            else:
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.accel)))
                sensor_profiles_arr.append((sensor, fps_helper.get_profile(sensor, rs.stream.gyro)))

        if profile is not None:
            sensor_profiles_arr.append((sensor, profile))
    return sensor_profiles_arr


@pytest.mark.parametrize("res_tier", list(RES_TIERS))
@pytest.mark.timeout(300)
def test_fps_permutations(test_device, res_tier):
    dev, ctx = test_device
    is_dds = is_dds_dev(dev)

    # The VGA tier characterizes the full-rate path on bandwidth-limited DDS (GigE) devices;
    # USB devices have ample bandwidth, so keep their original HD-only matrix.
    if res_tier == "VGA" and not is_dds:
        pytest.skip("VGA permutation tier runs only on DDS (GigE) devices")

    resolution = RES_TIERS[res_tier]
    # Pin the depth resolution on DDS so each tier is well-defined; preserve the legacy
    # first-available depth resolution on USB devices.
    depth_resolution = resolution if is_dds else None
    sensor_profiles_array = get_sensors_and_profiles(dev, color_resolution=resolution,
                                                     depth_resolution=depth_resolution)
    all_pairs = [[a[1].stream_name(), b[1].stream_name()] for a, b in combinations(sensor_profiles_array, 2)]

    fps_kpi = {}
    if is_dds and res_tier == "HD":
        # Color + Depth at HD may be throttled to ~22 fps on a CPU-burdened host (see KPI note);
        # assert a floor instead of the advertised 30 so both throttled and full-rate hosts pass.
        fps_kpi[frozenset({"Color", "Depth"})] = {
            "Color": COLOR_DEPTH_HD_DDS_FPS_KPI,
            "Depth": COLOR_DEPTH_HD_DDS_FPS_KPI,
        }

    fps_helper.perform_fps_test(sensor_profiles_array, all_pairs, fps_kpi=fps_kpi)
