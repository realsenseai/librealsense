# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
from pytest_check import check
from rspy import test, config_file
from rspy import librs as rs

log = logging.getLogger(__name__)
log.nested = 'C  '

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if log.isEnabledFor(logging.DEBUG):
    rs.log_to_console( rs.log_severity.debug )

@pytest.fixture(scope='module')
def remote_and_sensors():
    context = rs.context( {
        'dds': {
            'enabled': True,
            'domain': config_file.get_domain_from_config_file_or_default()
            },
        'device-mask': rs.only_sw_devices
        } )

    import os.path
    cwd = os.path.dirname(os.path.realpath(__file__))
    remote_script = os.path.join( cwd, 'formats-conversion-server.py' )
    with test.remote( remote_script, nested_indent="  S" ) as remote:
        remote.wait_until_ready()
        remote.run( 'create_server()' )
        try:
            dev = rs.wait_for_devices( context, n=1. )
            sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}
            yield remote, context, sensors
        finally:
            # Tests tear down
            remote.run( 'close_server()' )
            dev = None
            del context

#
#############################################################################################
#
def test_y8_conversion(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'Y8-sensor' in sensors ):
        sensor = sensors.get('Y8-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 1 )
        check.equal( profiles[0].format(), rs.format.y8 )
        check.equal( profiles[0].stream_index(), 0 )
#
#############################################################################################
#
def test_yuyv_conversion(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'YUYV-sensor' in sensors ):
        sensor = sensors.get('YUYV-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 6 ) # YUYV -> YUYV/RGB8/RGBA8/BGR8/BGRA8/Y8
        check.equal( profiles[0].format(), rs.format.rgb8 )
        check.equal( profiles[1].format(), rs.format.y8 )
        check.equal( profiles[2].format(), rs.format.bgra8 )
        check.equal( profiles[3].format(), rs.format.rgba8 )
        check.equal( profiles[4].format(), rs.format.bgr8 )
        check.equal( profiles[5].format(), rs.format.yuyv )
#
#############################################################################################
#
def test_uyvy_conversion(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'UYVY-sensor' in sensors ):
        sensor = sensors.get('UYVY-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 7 ) # UYVY -> UYVY/YUYV/RGB8/RGBA8/BGR8/BGRA8/Y8
        check.equal( profiles[0].format(), rs.format.rgb8 )
        check.equal( profiles[1].format(), rs.format.uyvy )
        check.equal( profiles[2].format(), rs.format.y8 )
        check.equal( profiles[3].format(), rs.format.bgra8 )
        check.equal( profiles[4].format(), rs.format.rgba8 )
        check.equal( profiles[5].format(), rs.format.bgr8 )
        check.equal( profiles[6].format(), rs.format.yuyv )
#
#############################################################################################
#
def test_z16_conversion(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'Z16-sensor' in sensors ):
        sensor = sensors.get('Z16-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 1 ) # Z16 stays Z16, for depth stream type
        check.equal( profiles[0].format(), rs.format.z16 )
        check.equal( profiles[0].stream_type(), rs.stream.depth )
#
#############################################################################################
#
def test_motion_conversion(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'motion-sensor' in sensors ):
        sensor = sensors.get('motion-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 1 ) # MXYZ stays MXYZ with type based on the dds_stream type
        check.equal( profiles[0].stream_type(), rs.stream.motion )
#
#############################################################################################
#
def test_multiple_motion_profiles_one_stream(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'multiple-motion-sensor' in sensors ):
        sensor = sensors.get('multiple-motion-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 4 )
        for i in range( len( profiles ) ):
            check.equal( profiles[i].stream_type(), rs.stream.motion )
#
#############################################################################################
#
def test_multiple_color_profiles_one_stream(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'multiple-color-sensor' in sensors ):
        sensor = sensors.get('multiple-color-sensor')
        profiles = sensor.get_stream_profiles()

        # Streams are sorted by format then fps:
        #     RGB8 @ 30/15/5 Hz
        #     BGRA8 @ 30/15/5 Hz
        #     RGBA8 @ 30/15/5 Hz
        #     BGR8 @ 30/15/5 Hz
        #     YUYV @ 30/15/5 Hz
        check.equal( len( profiles ), 18 )
        for i, f in zip( range(3), (30,15,5) ):
            check.equal( profiles[0 + i].format(), rs.format.rgb8 )
            check.equal( profiles[0 + i].fps(), f )
            check.equal( profiles[3 + i].format(), rs.format.y8 )
            check.equal( profiles[3 + i].fps(), f )
            check.equal( profiles[6 + i].format(), rs.format.bgra8 )
            check.equal( profiles[6 + i].fps(), f )
            check.equal( profiles[9 + i].format(), rs.format.rgba8 )
            check.equal( profiles[9 + i].fps(), f )
            check.equal( profiles[12 + i].format(), rs.format.bgr8 )
            check.equal( profiles[12 + i].fps(), f )
            check.equal( profiles[15 + i].format(), rs.format.yuyv )
            check.equal( profiles[15 + i].fps(), f )
#
#############################################################################################
#
def test_multiple_depth_profiles_one_stream(remote_and_sensors):
    _, _, sensors = remote_and_sensors
    if check.is_true( 'multiple-depth-sensor' in sensors ):
        sensor = sensors.get('multiple-depth-sensor')
        profiles = sensor.get_stream_profiles()

        check.equal( len( profiles ), 5 )
        for i in range(5):
            check.equal( profiles[i].format(), rs.format.z16 )
#
#############################################################################################
