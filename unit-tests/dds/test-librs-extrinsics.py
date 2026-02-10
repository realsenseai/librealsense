# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config_file
log.nested = 'C  '

from rspy import librs as rs
if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )
from time import sleep

context = rs.context( {
    'dds': {
        'enabled': True,
        'domain': config_file.get_domain_from_config_file()
       },
    'device-mask': rs.only_sw_devices
    } )

import os.path
cwd = os.path.dirname(os.path.realpath(__file__))
remote_script = os.path.join( cwd, 'device-broadcaster.py' )
with test.remote( remote_script, nested_indent="  S" ) as remote:
    remote.wait_until_ready()
    #
    #############################################################################################
    #
    test.start( "D435i extrinsics" )
    try:
        remote.run( 'instance = broadcast_device( d435i, d435i.device_info )' )
        n_devs = 0
        for dev in rs.wait_for_devices( context ):
            n_devs += 1
        test.check_equal( n_devs, 1 )

        sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}
        depth_profile = rs.stream_profile()
        ir1_profile = rs.stream_profile()
        ir2_profile = rs.stream_profile()
        color_profile = rs.stream_profile()
        gyro_profile = rs.stream_profile()
        accel_profile = rs.stream_profile()

        sensor = sensors['Stereo Module']
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.depth :
                depth_profile = profile
                break
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.infrared and profile.stream_index() == 1 : # Currently stream index does not match source
                ir1_profile = profile
                break
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.infrared and profile.stream_index() == 2 :
                ir2_profile = profile
                break
        sensor = sensors['RGB Camera']
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.color :
                color_profile = profile
                break
        sensor = sensors['Motion Module']
        for profile in sensor.get_stream_profiles() :
            if test.check_equal( profile.stream_type(), rs.stream.motion ):
                gyro_profile = profile
                break

        depth_to_ir1_extrinsics = depth_profile.get_extrinsics_to( ir1_profile )
        expected_rotation = [1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0]
        expected_translation = [0.0,0.0,0.0]
        test.check_float_lists( depth_to_ir1_extrinsics.rotation, expected_rotation )
        test.check_float_lists( depth_to_ir1_extrinsics.translation, expected_translation )

        depth_to_ir2_extrinsics = depth_profile.get_extrinsics_to( ir2_profile )
        expected_rotation = [1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0]
        expected_translation = [-0.04986396059393883,0.0,0.0]
        test.check_float_lists( depth_to_ir2_extrinsics.rotation, expected_rotation )
        test.check_float_lists( depth_to_ir2_extrinsics.translation, expected_translation )

        depth_to_color_extrinsics = depth_profile.get_extrinsics_to( color_profile )
        expected_rotation = [0.9999951720237732,-0.0004076171899214387,-0.00308464583940804,0.00040659401565790176,0.9999998807907104,-0.0003323106502648443,0.0030847808811813593,0.0003310548490844667,0.9999951720237732]
        expected_translation = [0.015078714117407799,4.601718956109835e-06,0.00017121469136327505]
        test.check_float_lists( depth_to_color_extrinsics.rotation, expected_rotation )
        test.check_float_lists( depth_to_color_extrinsics.translation, expected_translation )

        color_to_depth_extrinsics = color_profile.get_extrinsics_to( depth_profile )
        expected_rotation = [0.9999951720237732,0.00040659401565790176,0.0030847808811813593,-0.0004076171899214387,0.9999998807907104,0.0003310548490844667,-0.00308464583940804,-0.0003323106502648443,0.9999951720237732]
        expected_translation = [-0.015078110620379448,-1.0675736120902002e-05,-0.00021772991749458015]
        test.check_float_lists( color_to_depth_extrinsics.rotation, expected_rotation )
        test.check_float_lists( color_to_depth_extrinsics.translation, expected_translation )

        #color_to_accel_extrinsics = color_profile.get_extrinsics_to( accel_profile )
        #expected_rotation = [0.9999951720237732,0.00040659401565790176,0.0030847808811813593,-0.0004076171899214387,0.9999998807907104,0.0003310548490844667,-0.00308464583940804,-0.0003323106502648443,0.9999951720237732]
        #expected_translation = [-0.02059810981154442,0.0050893244333565235,0.011522269807755947]
        #test.check_float_lists( color_to_accel_extrinsics.rotation, expected_rotation )
        #test.check_float_lists( color_to_accel_extrinsics.translation, expected_translation )

        gyro_to_ir1_extrinsics = gyro_profile.get_extrinsics_to( ir1_profile )
        expected_rotation = [1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0]
        expected_translation = [0.005520000122487545,-0.005100000184029341,-0.011739999987185001]
        test.check_float_lists( gyro_to_ir1_extrinsics.rotation, expected_rotation )
        test.check_float_lists( gyro_to_ir1_extrinsics.translation, expected_translation )

        remote.run( 'close_server( instance )' )
    except:
        test.unexpected_exception()
    dev = None
    test.finish()
    #
    #############################################################################################

context = None
test.print_results_and_exit()
