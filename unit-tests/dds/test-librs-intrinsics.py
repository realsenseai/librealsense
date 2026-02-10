# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2022-4 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:retries 2

from rspy import log, test, config_file

with test.remote.fork( nested_indent=None ) as remote:
    if remote is None:  # we're the fork

        import pyrealdds as dds
        import d435i
        import d455

        dds.debug( log.is_debug_on(), log.nested )

        participant = dds.participant()
        participant.init( config_file.get_domain_from_config_file_or_default(), "intrinsics-server" )

        # These are the servers currently broadcast
        servers = dict()

        def broadcast_device( server, device_info ):
            global servers
            instance = device_info.serial
            if not instance:
                raise RuntimeError( "serial-number must be filled out" )
            servers[instance] = {
                'info' : device_info,
                'server' : server
                }
            server.broadcast( device_info )
            return instance

        def build_scaled_intrinsics_server():
            color = dds.color_stream_server( 'Color', 'RGB Camera' )
            color.init_profiles( d455.color_stream_profiles(), 5 )
            color.init_options( [] )
            depth = dds.depth_stream_server( 'depth', 'Depth Module' )
            depth.init_profiles( d455.depth_stream_profiles(), 5 )
            depth.init_options( [] )

            # Add only a single set of intrinsics for 1920x1080, from which we expect to scale to all resolutions:
            i = dds.video_intrinsics();
            i.width = 1280
            i.height = 720
            i.principal_point.x = 648.580993652344
            i.principal_point.y = 358.821014404297
            i.focal_length.x = 636.450012207031
            i.focal_length.y = 635.622009277344
            i.distortion.model = dds.distortion_model.inverse_brown
            i.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
            color.set_intrinsics( set( [i] ) )

            dev = dds.device_server( participant, d455.device_info.topic_root )
            dev.init( [color, depth], [], {} ) # Color intrinsincs are defined for d455 so using it for this test, but depth sensor is also expected on our cameras
            return dev

        def close_server( instance ):
            """
            Close the instance returned by broadcast_device()
            """
            global servers
            del servers[instance]  # throws if does not exist

        raise StopIteration()  # the remote is now interactive


    ###############################################################################################################
    # The client is LibRS
    #

    log.nested = 'C  '

    from rspy import librs as rs
    if log.is_debug_on():
        rs.log_to_console( rs.log_severity.debug )

    context = rs.context( { 'dds': { 'enabled': True, 'domain': config_file.get_domain_from_config_file_or_default() }} )

    #############################################################################################
    #
    with test.closure( "D435i intrinsics" ):

        remote.run( 'instance = broadcast_device( d435i.build( participant ), d435i.device_info )' )
        dev = rs.wait_for_devices( context, rs.only_sw_devices, n=1. )

        sensors = {sensor.get_info( rs.camera_info.name ) : sensor for sensor in dev.query_sensors()}

        sensor = sensors['Stereo Module']
        for profile in sensor.get_stream_profiles() :
            if profile.stream_type() == rs.stream.depth :
                depth_profile = profile.as_video_stream_profile()
                if depth_profile.width() == 256 and depth_profile.height() == 144 :
                    test.check_equal( depth_profile.get_intrinsics().ppx, 128.2379150390625 )
                    test.check_equal( depth_profile.get_intrinsics().ppy, 69.3431396484375 )
                    test.check_equal( depth_profile.get_intrinsics().fx, 631.3428955078125 )
                    test.check_equal( depth_profile.get_intrinsics().fy, 631.3428955078125 )

                if depth_profile.width() == 848 and depth_profile.height() == 100 :
                    test.check_equal( depth_profile.get_intrinsics().ppx, 424.1576232910156 )
                    test.check_equal( depth_profile.get_intrinsics().ppy, 48.239837646484375 )
                    test.check_equal( depth_profile.get_intrinsics().fx, 418.2646789550781 )
                    test.check_equal( depth_profile.get_intrinsics().fy, 418.2646789550781 )

            if profile.stream_type() == rs.stream.infrared and profile.stream_index() == 1 :
                ir1_profile = profile.as_video_stream_profile()
                if ir1_profile.width() == 1280 and ir1_profile.height() == 720 :
                    test.check_equal( ir1_profile.get_intrinsics().ppx, 640.2379150390625 )
                    test.check_equal( ir1_profile.get_intrinsics().ppy, 357.3431396484375 )
                    test.check_equal( ir1_profile.get_intrinsics().fx, 631.3428955078125 )
                    test.check_equal( ir1_profile.get_intrinsics().fy, 631.3428955078125 )
                    test.check_equal( ir1_profile.get_intrinsics().model, rs.distortion.brown_conrady )
                    test.check_equal_lists( ir1_profile.get_intrinsics().coeffs, [0.0,0.0,0.0,0.0,0.0] )

                if ir1_profile.width() == 1280 and ir1_profile.height() == 800 :
                    test.check_equal( ir1_profile.get_intrinsics().ppx, 640.2379150390625 )
                    test.check_equal( ir1_profile.get_intrinsics().ppy, 397.3431396484375 )
                    test.check_equal( ir1_profile.get_intrinsics().fx, 631.3428955078125 )
                    test.check_equal( ir1_profile.get_intrinsics().fy, 631.3428955078125 )

                if ir1_profile.width() == 424 and ir1_profile.height() == 240 :
                    test.check_equal( ir1_profile.get_intrinsics().ppx, 212.0788116455078 )
                    test.check_equal( ir1_profile.get_intrinsics().ppy, 119.07991790771484 )
                    test.check_equal( ir1_profile.get_intrinsics().fx, 209.13233947753906 )
                    test.check_equal( ir1_profile.get_intrinsics().fy, 209.13233947753906 )

            if profile.stream_type() == rs.stream.infrared and profile.stream_index() == 2 :
                ir2_profile = profile.as_video_stream_profile()
                if ir1_profile.width() == 480 and ir2_profile.height() == 270 :
                    test.check_equal( ir2_profile.get_intrinsics().ppx, 240.08921813964844 )
                    test.check_equal( ir2_profile.get_intrinsics().ppy, 134.00367736816406 )
                    test.check_equal( ir2_profile.get_intrinsics().fx, 236.7535858154297 )
                    test.check_equal( ir2_profile.get_intrinsics().fy, 236.7535858154297 )

        sensor = sensors['RGB Camera']
        for profile in sensor.get_stream_profiles():
            if profile.stream_type() == rs.stream.color:
                color_profile = profile.as_video_stream_profile()
                if color_profile.width() == 320 and color_profile.height() == 180 :
                    test.check_equal( color_profile.get_intrinsics().ppx, 161.7417755126953 )
                    test.check_equal( color_profile.get_intrinsics().ppy, 90.47455596923828 )
                    test.check_equal( color_profile.get_intrinsics().fx, 227.0221710205078 )
                    test.check_equal( color_profile.get_intrinsics().fy, 227.1049346923828 )
                    test.check_equal( color_profile.get_intrinsics().model, rs.distortion.inverse_brown_conrady )
                    test.check_equal_lists( color_profile.get_intrinsics().coeffs, [0.0,0.0,0.0,0.0,0.0] )

                if color_profile.width() == 960 and color_profile.height() == 540 :
                    test.check_equal( color_profile.get_intrinsics().ppx, 485.2253112792969 )
                    test.check_equal( color_profile.get_intrinsics().ppy, 271.4236755371094 )
                    test.check_equal( color_profile.get_intrinsics().fx, 681.0665283203125 )
                    test.check_equal( color_profile.get_intrinsics().fy, 681.3148193359375 )

                if color_profile.width() == 1920 and color_profile.height() == 1080:
                    test.check_equal( color_profile.get_intrinsics().ppx, 970.4506225585938 )
                    test.check_equal( color_profile.get_intrinsics().ppy, 542.8473510742188 )
                    test.check_equal( color_profile.get_intrinsics().fx, 1362.133056640625 )
                    test.check_equal( color_profile.get_intrinsics().fy, 1362.629638671875 )

        sensor = sensors['Motion Module']
        for profile in sensor.get_stream_profiles():
            if test.check_equal( profile.stream_type(), rs.stream.motion ):
                gyro_profile = profile.as_motion_stream_profile()
                test.check_equal_lists( gyro_profile.get_motion_intrinsics().data, [[1.0,0.0,0.0,0.0],[0.0,1.0,0.0,0.0],[0.0,0.0,1.0,0.0]] )
                test.check_equal_lists( gyro_profile.get_motion_intrinsics().noise_variances, [0.0,0.0,0.0] )
                test.check_equal_lists( gyro_profile.get_motion_intrinsics().bias_variances, [0.0,0.0,0.0] )
                # There's currently no way to get the accelerometer intrinsics (which are set the same anyway)

        remote.run( 'close_server( instance )' )
        dev = None

    with test.closure( 'Scaled intrinsics' ):
        remote.run( 'instance = broadcast_device( build_scaled_intrinsics_server(), d455.device_info )' )
        dev = rs.wait_for_devices( context, rs.only_sw_devices, n=1. )
        sensors = dev.query_sensors()
        test.check_equal( len(sensors), 2 )
        color = sensors[0]
        profiles = color.profiles
        resolutions = set()
        for p in profiles:
            vp = p.as_video_stream_profile()
            if not vp:
                continue
            if vp.fps() != 30:
                continue
            if vp.format() != rs.format.yuyv:
                continue
            res = ( vp.width(), vp.height() )
            if res not in resolutions:
                log.out( f'resolution: {res} -> {vp.get_intrinsics()}' )  # throws if there's no intrinsics
                resolutions.add( res )
        test.check( len(resolutions) > 5 )

        remote.run( 'close_server( instance )' )

context = None
test.print_results()
