# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test


device_info = dds.message.device_info.from_json( {
    "name": "RealSense D405",
    "serial": "123622270732",
    "product-line": "D400",
    "topic-root": "realdds/D405/123622270732"
} )


def build( participant ):
    """
    Build a D405 device server for use in DDS
    """
    depth = depth_stream()
    # ir0 = colored_infrared_stream() # Colored infrared currently not sent on DDS
    ir1 = ir_stream( 1 )
    ir2 = ir_stream( 2 )
    color = color_stream()
    extrinsics = get_extrinsics()
    #
    d405 = dds.device_server( participant, device_info.topic_root )
    # d405.init( [ir0, ir1, ir2, color, depth], [], extrinsics ) # Colored infrared currently not sent on DDS
    d405.init( [ir1, ir2, color, depth], [], extrinsics )
    return d405


def colored_infrared_stream():
    stream = dds.ir_stream_server( "Infrared", "Stereo Module" )
    stream.init_profiles( colored_infrared_stream_profiles(), 11 )
    #stream.init_options( stereo_module_options() )
    return stream


def colored_infrared_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 5,  dds.video_encoding.uyvy, 256, 144 )
        ]


def depth_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 5,  dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 256, 144 )
        ]


def depth_stream():
    stream = dds.depth_stream_server( "Depth", "Stereo Module" )
    stream.init_profiles( depth_stream_profiles(), 5 )
    stream.init_options( stereo_module_options() )
    stream.set_intrinsics( depth_stream_intrinsics() )
    return stream


def ir_stream_profiles():
    return [
        dds.video_stream_profile( 25, dds.video_encoding.y16,  1288, 808 ),
        dds.video_stream_profile( 15, dds.video_encoding.y16,  1288, 808 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 5,  dds.video_encoding.y8, 424, 240 )
        ]


def ir_stream( number ):
    stream = dds.ir_stream_server( "Infrared_" + str(number), "Stereo Module" )
    profiles = ir_stream_profiles()
    if number == 1:
        calibration_profile = dds.video_stream_profile( 90,  dds.video_encoding.y8, 256, 144 )
        profiles.append( calibration_profile )
    stream.init_profiles( profiles, 7 )
    #stream.init_options( stereo_module_options() )
    stream.set_intrinsics( ir_stream_intrinsics() )
    return stream


def color_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.yuyv, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 640, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.yuyv, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 640, 360 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.yuyv, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 480, 270 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.yuyv, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 424, 240 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 424, 240 )
        ]


def color_stream():
    stream = dds.color_stream_server( "Color",  "Stereo Module" )
    stream.init_profiles( color_stream_profiles(), 1 )
    #stream.init_options( stereo_module_options() )
    stream.set_intrinsics( color_stream_intrinsics() )
    return stream


def stereo_module_options():
    options = []

    option = dds.option.from_json( ["Backlight Compensation", 0, 0, 1, 1, 0, "Enable / disable backlight compensation"] )
    options.append( option )
    option = dds.option.from_json( ["Brightness", 0, -64, 64, 1, 0, "UVC image brightness"] )
    options.append( option )
    option = dds.option.from_json( ["Contrast", 50, 0, 100, 1, 50, "UVC image contrast"] )
    options.append( option )
    option = dds.option.from_json( ["Exposure", 33000, 1, 165000, 1, 33000, "Depth Exposure (usec)"] )
    options.append( option )
    option = dds.option.from_json( ["Gain", 16, 16, 248, 1, 16, "UVC image gain"] )
    options.append( option )
    option = dds.option.from_json( ["Gamma", 300, 100, 500, 1, 300, "UVC image gamma setting"] )
    options.append( option )
    option = dds.option.from_json( ["Hue", 0, -180, 180, 1, 0, "UVC image hue"] )
    options.append( option )
    option = dds.option.from_json( ["Saturation", 64, 0, 100, 1, 64, "UVC image saturation setting"] )
    options.append( option )
    option = dds.option.from_json( ["Sharpness", 50, 0, 100, 1, 50, "UVC image sharpness setting"] )
    options.append( option )
    option = dds.option.from_json( ["White Balance", 4600, 2800, 6500, 10, 4600,
        "Controls white balance of color image. Setting any value will disable auto white balance"] )
    options.append( option )
    option = dds.option.from_json( ["Enable Auto Exposure", 1, 0, 1, 1, 1, "Enable Auto Exposure"] )
    options.append( option )
    option = dds.option.from_json( ["Enable Auto White Balance", 1, 0, 1, 1, 1, "Enable / disable auto-white-balance"] )
    options.append( option )
    option = dds.option.from_json( ["Visual Preset", 0, 0, 5, 1, 0, "Advanced-Mode Preset"] )
    options.append( option )
    option = dds.option.from_json( ["Power Line Frequency", 3, 0, 3, 1, 3, "Power Line Frequency"] )
    options.append( option )
    option = dds.option.from_json( ["Error Polling Enabled", 1, 0, 1, 1, 0, "Enable / disable polling of camera internal errors"] )
    options.append( option )
    option = dds.option.from_json( ["Output Trigger Enabled", 0, 0, 1, 1, 0, "Generate trigger from the camera to external device once per frame"] )
    options.append( option )
    option = dds.option.from_json( ["Depth Units", 0.0001, 1e-06, 0.01, 1e-06, 0.001, "Number of meters represented by a single depth unit"] )
    options.append( option )
    option = dds.option.from_json( ["Stereo Baseline", 17.9998, 17.9998, 17.9998, 0, 17.9998, "Distance in mm between the stereo imagers"] )
    options.append( option )
    option = dds.option.from_json( ["Global Time Enabled", 1, 0, 1, 1, 1, "Enable/Disable global timestamp"] )
    options.append( option )
    option = dds.option.from_json( ["Hdr Enabled", 0, 0, 1, 1, 0, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Name", 0, 0, 3, 1, 1, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Size", 2, 2, 2, 1, 2, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Id", 0, 0, 2, 1, 0, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Auto Exposure Limit", 165000, 1, 165000, 1, 33000,
        "Exposure limit is in microseconds. If the requested exposure limit is greater than frame time, it will be set to frame time at runtime. Setting will not take effect until next streaming session."] )
    options.append( option )
    option = dds.option.from_json( ["Auto Gain Limit", 248, 16, 248, 1, 16,
        "Gain limits ranges from 16 to 248. If the requested gain limit is less than 16, it will be set to 16. If the requested gain limit is greater than 248, it will be set to 248. Setting will not take effect until next streaming session."] )
    options.append( option )
    option = dds.option.from_json( ["Auto Exposure Limit Toggle", 0, 0, 1, 1, 0, "Toggle Auto-Exposure Limit"] )
    options.append( option )
    option = dds.option.from_json( ["Auto Gain Limit Toggle", 0, 0, 1, 1, 0, "Toggle Auto-Gain Limit"] )
    options.append( option )

    return options


def color_stream_intrinsics():
    intrinsics = []

    intr = dds.video_intrinsics();
    intr.width = 424
    intr.height = 240
    intr.principal_point.x = 210.73512268066406
    intr.principal_point.y = 118.6335678100586
    intr.focal_length.x = 215.58554077148438
    intr.focal_length.y = 214.98973083496094
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 480
    intr.height = 270
    intr.principal_point.x = 238.57701110839844
    intr.principal_point.y = 133.4627685546875
    intr.focal_length.x = 242.53375244140625
    intr.focal_length.y = 241.86343383789063
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 480
    intr.principal_point.x = 318.1026916503906
    intr.principal_point.y = 177.95034790039063
    intr.focal_length.x = 323.3783264160156
    intr.focal_length.y = 322.4845886230469
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 480
    intr.principal_point.x = 317.4702453613281
    intr.principal_point.y = 237.2671356201172
    intr.focal_length.x = 431.1711120605469
    intr.focal_length.y = 429.9794616699219
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 848
    intr.height = 480
    intr.principal_point.x = 421.4702453613281
    intr.principal_point.y = 237.2671356201172
    intr.focal_length.x = 431.17108154296875
    intr.focal_length.y = 429.9794616699219
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 1280
    intr.height = 720
    intr.principal_point.x = 636.2053833007813
    intr.principal_point.y = 355.90069580078125
    intr.focal_length.x = 646.7566528320313
    intr.focal_length.y = 644.9691772460938
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.05454780161380768,0.056755296885967255,0.0010132350726053119,0.0003381881397217512,-0.01852494664490223]
    intrinsics.append( intr )

    return set( intrinsics )


def depth_ir_common_intrinsics():
    intrinsics = []

    intr = dds.video_intrinsics();
    intr.width = 424
    intr.height = 240
    intr.principal_point.x = 214.33639526367188
    intr.principal_point.y = 119.0371322631836
    intr.focal_length.x = 210.80271911621094
    intr.focal_length.y = 210.80271911621094
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 480
    intr.height = 270
    intr.principal_point.x = 242.6449737548828
    intr.principal_point.y = 133.9099578857422
    intr.focal_length.x = 238.64459228515625
    intr.focal_length.y = 238.64459228515625
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 360
    intr.principal_point.x = 323.5266418457031
    intr.principal_point.y = 178.54661560058594
    intr.focal_length.x = 318.1927795410156
    intr.focal_length.y = 318.1927795410156
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 480
    intr.principal_point.x = 324.21624755859375
    intr.principal_point.y = 238.26242065429688
    intr.focal_length.x = 380.41363525390625
    intr.focal_length.y = 380.41363525390625
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 848
    intr.height = 480
    intr.principal_point.x = 428.67279052734375
    intr.principal_point.y = 238.0742645263672
    intr.focal_length.x = 421.6054382324219
    intr.focal_length.y = 421.6054382324219
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 1280
    intr.height = 720
    intr.principal_point.x = 647.0532836914063
    intr.principal_point.y = 357.0932312011719
    intr.focal_length.x = 636.3855590820313
    intr.focal_length.y = 636.3855590820313
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    return intrinsics


def depth_stream_intrinsics():
    intrinsics = []

    intr = dds.video_intrinsics();
    intr.width = 256
    intr.height = 144
    intr.principal_point.x = 135.05328369140625
    intr.principal_point.y = 69.09323120117188
    intr.focal_length.x = 636.3855590820313
    intr.focal_length.y = 636.3855590820313
    intr.distortion.model = dds.distortion_model.brown
    intr.distortion.coeffs = [0.0,0.0,0.0,0.0,0.0]
    intrinsics.append( intr )

    intrinsics.extend( depth_ir_common_intrinsics() )

    return set( intrinsics )


def ir_stream_intrinsics():
    return set( depth_ir_common_intrinsics() )


def get_extrinsics():
    extrinsics = {}

    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,0.0014127078466117382,0.0053943851962685585,-0.001408747979439795,0.9999987483024597,-0.0007378021255135536,-0.005395420826971531,0.000730191299226135,0.9999851584434509)
    extr.translation = (6.110243703005835e-05,1.1573569281608798e-05,6.581118213944137e-06)
    extrinsics[("Color","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,0.0014127078466117382,0.0053943851962685585,-0.001408747979439795,0.9999987483024597,-0.0007378021255135536,-0.005395420826971531,0.000730191299226135,0.9999851584434509)
    extr.translation = (6.110243703005835e-05,1.1573569281608798e-05,6.581118213944137e-06)
    extrinsics[("Color","Infrared") ] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,0.0014127078466117382,0.0053943851962685585,-0.001408747979439795,0.9999987483024597,-0.0007378021255135536,-0.005395420826971531,0.000730191299226135,0.9999851584434509)
    extr.translation = (6.110243703005835e-05,1.1573569281608798e-05,6.581118213944137e-06)
    extrinsics[("Color","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,0.0014127078466117382,0.0053943851962685585,-0.001408747979439795,0.9999987483024597,-0.0007378021255135536,-0.005395420826971531,0.000730191299226135,0.9999851584434509)
    extr.translation = (-0.017938679084181786,1.1573569281608798e-05,6.581118213944137e-06)
    extrinsics[("Color","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,-0.001408747979439795,-0.005395420826971531,0.0014127078466117382,0.9999987483024597,0.000730191299226135,0.0053943851962685585,-0.0007378021255135536,0.9999851584434509)
    extr.translation = (-6.115333235356957e-05,-1.1482620720926207e-05,-6.2597978285339195e-06)
    extrinsics[("Depth","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Depth","Infrared")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Depth","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.017999781295657158,0.0,0.0)
    extrinsics[("Depth","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,-0.001408747979439795,-0.005395420826971531,0.0014127078466117382,0.9999987483024597,0.000730191299226135,0.0053943851962685585,-0.0007378021255135536,0.9999851584434509)
    extr.translation = (-6.115333235356957e-05,-1.1482620720926207e-05,-6.2597978285339195e-06)
    extrinsics[("Infrared","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Infrared","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Infrared","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.017999781295657158,0.0,0.0)
    extrinsics[("Infrared","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,-0.001408747979439795,-0.005395420826971531,0.0014127078466117382,0.9999987483024597,0.000730191299226135,0.0053943851962685585,-0.0007378021255135536,0.9999851584434509)
    extr.translation = (-6.115333235356957e-05,-1.1482620720926207e-05,-6.2597978285339195e-06)
    extrinsics[("Infrared_1","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Infrared_1","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Infrared_1","Infrared")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.017999781295657158,0.0,0.0)
    extrinsics[("Infrared_1","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999844431877136,-0.001408747979439795,-0.005395420826971531,0.0014127078466117382,0.9999987483024597,0.000730191299226135,0.0053943851962685585,-0.0007378021255135536,0.9999851584434509)
    extr.translation = (0.01793834939599037,-3.683977774926461e-05,-0.00010337619460187852)
    extrinsics[("Infrared_2","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.017999781295657158,0.0,0.0)
    extrinsics[("Infrared_2","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.017999781295657158,0.0,0.0)
    extrinsics[("Infrared_2","Infrared")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.017999781295657158,0.0,0.0)
    extrinsics[("Infrared_2","Infrared_1")] = extr

    return extrinsics

