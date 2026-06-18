# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
from metadata_common import check_md_value, check_counter_and_timestamp_increase, reset_data
log = logging.getLogger(__name__)

pytestmark = [pytest.mark.device_each("D585S")]


occupancy_metadata_values = [rs.frame_metadata_value.frame_counter,
                             rs.frame_metadata_value.safety_depth_frame_counter,
                             rs.frame_metadata_value.frame_timestamp,
                             rs.frame_metadata_value.floor_detection,
                             rs.frame_metadata_value.diagnostic_zone_fill_rate,
                             rs.frame_metadata_value.depth_fill_rate,
                             rs.frame_metadata_value.sensor_angle_roll,
                             rs.frame_metadata_value.sensor_angle_pitch,
                             rs.frame_metadata_value.diagnostic_zone_median_height,
                             # rs.frame_metadata_value.depth_stdev, # (not used by HKR yet)
                             rs.frame_metadata_value.safety_preset_id,
                             rs.frame_metadata_value.safety_preset_error_type,
                             rs.frame_metadata_value.safety_preset_error_param_1,
                             rs.frame_metadata_value.safety_preset_error_param_2,
                             rs.frame_metadata_value.danger_zone_point_0_x_cord,
                             rs.frame_metadata_value.danger_zone_point_0_y_cord,
                             rs.frame_metadata_value.danger_zone_point_1_x_cord,
                             rs.frame_metadata_value.danger_zone_point_1_y_cord,
                             rs.frame_metadata_value.danger_zone_point_2_x_cord,
                             rs.frame_metadata_value.danger_zone_point_2_y_cord,
                             rs.frame_metadata_value.danger_zone_point_3_x_cord,
                             rs.frame_metadata_value.danger_zone_point_3_y_cord,
                             rs.frame_metadata_value.warning_zone_point_0_x_cord,
                             rs.frame_metadata_value.warning_zone_point_0_y_cord,
                             rs.frame_metadata_value.warning_zone_point_1_x_cord,
                             rs.frame_metadata_value.warning_zone_point_1_y_cord,
                             rs.frame_metadata_value.warning_zone_point_2_x_cord,
                             rs.frame_metadata_value.warning_zone_point_2_y_cord,
                             rs.frame_metadata_value.warning_zone_point_3_x_cord,
                             rs.frame_metadata_value.warning_zone_point_3_y_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_0_x_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_0_y_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_1_x_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_1_y_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_2_x_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_2_y_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_3_x_cord,
                             rs.frame_metadata_value.diagnostic_zone_point_3_y_cord,
                             rs.frame_metadata_value.occupancy_grid_rows,
                             rs.frame_metadata_value.occupancy_grid_columns,
                             rs.frame_metadata_value.occupancy_cell_size]

point_cloud_metadata_values = [rs.frame_metadata_value.frame_counter,
                               rs.frame_metadata_value.safety_depth_frame_counter,
                               rs.frame_metadata_value.frame_timestamp,
                               rs.frame_metadata_value.floor_detection,
                               rs.frame_metadata_value.diagnostic_zone_fill_rate,
                               rs.frame_metadata_value.depth_fill_rate,
                               rs.frame_metadata_value.sensor_angle_roll,
                               rs.frame_metadata_value.sensor_angle_pitch,
                               rs.frame_metadata_value.diagnostic_zone_median_height,
                               # rs.frame_metadata_value.depth_stdev, # (not used by HKR yet)
                               rs.frame_metadata_value.safety_preset_id,
                               rs.frame_metadata_value.safety_preset_error_type,
                               rs.frame_metadata_value.safety_preset_error_param_1,
                               rs.frame_metadata_value.safety_preset_error_param_2,
                               rs.frame_metadata_value.danger_zone_point_0_x_cord,
                               rs.frame_metadata_value.danger_zone_point_0_y_cord,
                               rs.frame_metadata_value.danger_zone_point_1_x_cord,
                               rs.frame_metadata_value.danger_zone_point_1_y_cord,
                               rs.frame_metadata_value.danger_zone_point_2_x_cord,
                               rs.frame_metadata_value.danger_zone_point_2_y_cord,
                               rs.frame_metadata_value.danger_zone_point_3_x_cord,
                               rs.frame_metadata_value.danger_zone_point_3_y_cord,
                               rs.frame_metadata_value.warning_zone_point_0_x_cord,
                               rs.frame_metadata_value.warning_zone_point_0_y_cord,
                               rs.frame_metadata_value.warning_zone_point_1_x_cord,
                               rs.frame_metadata_value.warning_zone_point_1_y_cord,
                               rs.frame_metadata_value.warning_zone_point_2_x_cord,
                               rs.frame_metadata_value.warning_zone_point_2_y_cord,
                               rs.frame_metadata_value.warning_zone_point_3_x_cord,
                               rs.frame_metadata_value.warning_zone_point_3_y_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_0_x_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_0_y_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_1_x_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_1_y_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_2_x_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_2_y_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_3_x_cord,
                               rs.frame_metadata_value.diagnostic_zone_point_3_y_cord,
                               rs.frame_metadata_value.number_of_3d_vertices]


def check_occupancy_metadata(frame):
    for md_value in occupancy_metadata_values:
        check_md_value(frame, md_value)


def check_point_cloud_metadata(frame):
    for md_value in point_cloud_metadata_values:
        check_md_value(frame, md_value)


def test_occupancy_stream_metadata_received(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.occupancy)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_occupancy_metadata(f)
    pipe.stop()


def test_labeled_point_cloud_stream_metadata_received(test_context):
    cfg = rs.config()
    cfg.enable_stream(rs.stream.labeled_point_cloud)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_point_cloud_metadata(f)
    pipe.stop()


def test_occupancy_counter_and_timestamp_increase(test_context):
    cfg = rs.config()
    fps = 30
    cfg.enable_stream(rs.stream.occupancy)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    reset_data()
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_counter_and_timestamp_increase(f, fps)
    pipe.stop()


def test_labeled_point_cloud_counter_and_timestamp_increase(test_context):
    cfg = rs.config()
    fps = 30
    cfg.enable_stream(rs.stream.labeled_point_cloud)
    pipe = rs.pipeline(test_context)
    pipe.start(cfg)
    iterations = 0
    reset_data()
    while iterations < 20:
        iterations += 1
        f = pipe.wait_for_frames()
        check_counter_and_timestamp_increase(f, fps)
    pipe.stop()
