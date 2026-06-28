# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import logging
from rspy import test, config_file
import d435i
import d405
import d455
from rspy import librs as rs
from time import sleep
from pytest_check import check

log = logging.getLogger(__name__)
log.nested = 'C  '

pytestmark = [
    pytest.mark.dds,
    pytest.mark.flaky( retries=2 ),
]

if log.isEnabledFor(logging.DEBUG):
    rs.log_to_console( rs.log_severity.debug )

@pytest.fixture(scope='module')
def remote_and_context():
    context = rs.context( {
        'dds': {
            'enabled': True,
            'domain': config_file.get_domain_from_config_file_or_default()
           },
        'device-mask': rs.only_sw_devices
        } )

    import os.path
    cwd = os.path.dirname(os.path.realpath(__file__))
    remote_script = os.path.join( cwd, 'device-broadcaster.py' )
    with test.remote( remote_script, nested_indent="  S" ) as remote:
        remote.wait_until_ready()
        try:
            yield remote, context
        finally:
            del context

#############################################################################################
#
def test_d435i(remote_and_context):
    remote, context = remote_and_context
    remote.run( 'instance = broadcast_device( d435i, d435i.device_info )' )
    dev = rs.wait_for_devices( context, n=1. )
    check.equal( dev.get_info( rs.camera_info.name ), d435i.device_info.name )
    check.equal( dev.get_info( rs.camera_info.serial_number ), d435i.device_info.serial )
    check.equal( dev.get_info( rs.camera_info.physical_port ), d435i.device_info.topic_root )
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}
    check.equal( len(sensors), 3 )
    sensor = dev.first_depth_sensor()
    if check.is_true( sensor ):
        check.is_true( rs.depth_sensor( sensor ))
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'Stereo Module' )
        check.equal( len(sensor.get_stream_profiles()), 104 ) # As measured running rs-sensor-control example
    sensor = dev.first_color_sensor()
    if check.is_true( sensor ):
        check.is_true( rs.color_sensor( sensor ))
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'RGB Camera' )
        check.equal( len(sensor.get_stream_profiles()), 193 ) # As measured running rs-sensor-control example
    sensor = dev.first_motion_sensor()
    if check.is_true( sensor ):
        check.is_true( rs.motion_sensor( sensor ))
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'Motion Module' )
        check.equal( len(sensor.get_stream_profiles()), 2 ) # Only the Gyro profiles
    remote.run( 'close_server( instance )' )
    dev = None
#
#############################################################################################
#
def test_d405(remote_and_context):
    remote, context = remote_and_context
    remote.run( 'instance = broadcast_device( d405, d405.device_info )' )
    dev = rs.wait_for_devices( context, n=1. )
    check.equal( dev.get_info( rs.camera_info.name ), d405.device_info.name )
    check.equal( dev.get_info( rs.camera_info.serial_number ), d405.device_info.serial )
    check.equal( dev.get_info( rs.camera_info.physical_port ), d405.device_info.topic_root )
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}
    check.equal( len(sensors), 1 )
    sensor = dev.first_depth_sensor()
    if check.is_true( sensor ):
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'Stereo Module' )
        check.equal( len(sensor.get_stream_profiles()), 258 ) # As measured running rs-sensor-control example
    remote.run( 'close_server( instance )' )
    dev = None
#
#############################################################################################
#
def test_d455(remote_and_context):
    remote, context = remote_and_context
    remote.run( 'instance = broadcast_device( d455, d455.device_info )' )
    dev = rs.wait_for_devices( context, n=1. )
    check.equal( dev.get_info( rs.camera_info.name ), d455.device_info.name )
    check.equal( dev.get_info( rs.camera_info.serial_number ), d455.device_info.serial )
    check.equal( dev.get_info( rs.camera_info.physical_port ), d455.device_info.topic_root )
    sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}
    check.equal( len(sensors), 3 )
    sensor = dev.first_depth_sensor()
    if check.is_true( sensor ):
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'Stereo Module' )
        check.equal( len(sensor.get_stream_profiles()), 100 ) # As measured running rs-sensor-control example
    sensor = dev.first_color_sensor()
    if check.is_true( sensor ):
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'RGB Camera' )
        check.equal( len(sensor.get_stream_profiles()), 187 ) # As measured running rs-sensor-control example
    sensor = dev.first_motion_sensor()
    if check.is_true( sensor ):
        check.is_true( sensors[sensor.name] )
        check.equal( sensor.name, 'Motion Module' )
        check.equal( len(sensor.get_stream_profiles()), 2 ) # Only the Gyro profiles
    remote.run( 'close_server( instance )' )
    dev = None
    #
    #############################################################################################
#
