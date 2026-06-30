# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

import pyrealdds as dds
from rspy import log, test


device_info = dds.message.device_info.from_json( {
    "name": "RealSense D455",
    "serial": "114222251267",
    "product-line": "D400",
    "topic-root": "realdds/D455/114222251267"
} )


def build( participant ):
    """
    Build a D455 device server for use in DDS
    """
    motion = motion_stream()
    depth = depth_stream()
    ir1 = ir_stream( 1 )
    ir2 = ir_stream( 2 )
    color = color_stream()
    extrinsics = get_extrinsics()
    #
    d455 = dds.device_server( participant, device_info.topic_root )
    d455.init( [motion, depth, ir1, ir2, color], [], extrinsics )
    return d455


def gyro_stream_profiles():
    return [
        dds.motion_stream_profile( 200 ),
        dds.motion_stream_profile( 400 )
        ]


def motion_stream():
    stream = dds.motion_stream_server( "Motion", "Motion Module" )
    stream.init_profiles( gyro_stream_profiles(), 0 )
    stream.init_options( motion_module_options() )
    return stream


def colored_infrared_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 848, 480 ),
        dds.video_stream_profile( 100, dds.video_encoding.uyvy, 848, 100 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.uyvy, 424, 240 ),
        dds.video_stream_profile( 5 , dds.video_encoding.uyvy, 424, 240 ),
        ]


def depth_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 848, 480 ),
        dds.video_stream_profile( 100, dds.video_encoding.z16, 848, 100 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 640, 480 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 5 , dds.video_encoding.z16, 424, 240 ),
        dds.video_stream_profile( 90, dds.video_encoding.z16, 256, 144 )
        ]


def depth_stream():
    stream = dds.depth_stream_server( "Depth", "Stereo Module" )
    stream.init_profiles( depth_stream_profiles(), 5 )
    stream.init_options( stereo_module_options() )
    return stream


def ir_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.y8, 1280, 800 ),
        dds.video_stream_profile( 25, dds.video_encoding.y16, 1280, 800 ),
        dds.video_stream_profile( 15, dds.video_encoding.y16, 1280, 800 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 1280, 800 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 1280, 720 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 848, 480 ),
        dds.video_stream_profile( 100, dds.video_encoding.y8, 848, 100 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 640, 480 ),
        dds.video_stream_profile( 25, dds.video_encoding.y16, 640, 400 ),
        dds.video_stream_profile( 15, dds.video_encoding.y16, 640, 400 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 640, 360 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 480, 270 ),
        dds.video_stream_profile( 90, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 60, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 30, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 15, dds.video_encoding.y8, 424, 240 ),
        dds.video_stream_profile( 5 , dds.video_encoding.y8, 424, 240 )
        ]


def ir_stream( number ):
    stream = dds.ir_stream_server( f"Infrared_{number}", "Stereo Module" )
    stream.init_profiles( ir_stream_profiles(), 9 )
    #stream.init_options( stereo_module_options() )
    return stream


def color_stream_profiles():
    return [
        dds.video_stream_profile( 30, dds.video_encoding.byr2, 1280, 800 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 1280, 800 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 1280, 800 ),
        dds.video_stream_profile( 10, dds.video_encoding.yuyv, 1280, 800 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 1280, 800 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 10, dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 1280, 720 ),
        dds.video_stream_profile( 60, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 30, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 15, dds.video_encoding.yuyv, 848, 480 ),
        dds.video_stream_profile( 5 , dds.video_encoding.yuyv, 848, 480 ),
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
    stream = dds.color_stream_server( "Color",  "RGB Camera" )
    stream.init_profiles( color_stream_profiles(), 10 )
    stream.init_options( rgb_camera_options() )
    stream.set_intrinsics( color_stream_intrinsics() )
    return stream


def stereo_module_options():
    options = []

    option = dds.option.from_json( ["Exposure", 33000, 1, 200000, 1, 33000, "Depth Exposure (usec)"] )
    options.append( option )
    option = dds.option.from_json( ["Gain", 16, 16, 248, 1, 16, "UVC image gain"] )
    options.append( option )
    option = dds.option.from_json( ["Enable Auto Exposure", 1, 0, 1, 1, 1, "Enable Auto Exposure"] )
    options.append( option )
    option = dds.option.from_json( ["Visual Preset", 0, 0, 5, 1, 0, "Advanced-Mode Preset"] )
    options.append( option )
    option = dds.option.from_json( ["Laser Power", 150, 0, 360, 30, 150, "Manual laser power in mw. applicable only when laser power mode is set to Manual"] )
    options.append( option )
    option = dds.option.from_json( ["Emitter Enabled", 1, 0, 2, 1, 1, "Emitter select, 0-disable all emitters, 1-enable laser, 2-enable laser auto (opt), 3-enable LED (opt)"] )
    options.append( option )
    option = dds.option.from_json( ["Error Polling Enabled", 1, 0, 1, 1, 0, "Enable / disable polling of camera internal errors"] )
    options.append( option )
    option = dds.option.from_json( ["Output Trigger Enabled", 0, 0, 1, 1, 0, "Generate trigger from the camera to external device once per frame"] )
    options.append( option )
    option = dds.option.from_json( ["Depth Units", 0.001, 1e-06, 0.01, 1e-06, 0.001, "Number of meters represented by a single depth unit"] )
    options.append( option )
    option = dds.option.from_json( ["Stereo Baseline", 94.9609, 94.9609, 94.9609, 0, 94.9609, "Distance in mm between the stereo imagers"] )
    options.append( option )
    option = dds.option.from_json( ["Inter Cam Sync Mode", 0, 0, 260, 1, 0,
        "Inter-camera synchronization mode: 0:Default, 1:Master, 2:Slave, 3:Full Salve, 4-258:Genlock with burst count of 1-255 frames for each trigger, 259 and 260 for two frames per trigger with laser ON-OFF and OFF-ON."] )
    options.append( option )
    option = dds.option.from_json( ["Emitter On Off", 0, 0, 1, 1, 0, "Alternating emitter pattern, toggled on/off on per-frame basis"] )
    options.append( option )
    option = dds.option.from_json( ["Global Time Enabled", 1, 0, 1, 1, 1, "Enable/Disable global timestamp"] )
    options.append( option )
    option = dds.option.from_json( ["Emitter Always On", 0, 0, 1, 1, 0, "Emitter always on mode: 0:disabled(default), 1:enabled"] )
    options.append( option )
    option = dds.option.from_json( ["Thermal Compensation", 1, 0, 1, 1, 0, "Toggle thermal compensation adjustments mechanism"] )
    options.append( option )
    option = dds.option.from_json( ["Hdr Enabled", 0, 0, 1, 1, 0, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Name", 0, 0, 3, 1, 1, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Size", 2, 2, 2, 1, 2, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Sequence Id", 0, 0, 2, 1, 0, "HDR Option"] )
    options.append( option )
    option = dds.option.from_json( ["Auto Exposure Limit", 200000, 1, 200000, 1, 33000,
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


def rgb_camera_options():
    options = []

    option = dds.option.from_json( ["Backlight Compensation", 0, 0, 1, 1, 0, "Enable / disable backlight compensation"] )
    options.append( option )
    option = dds.option.from_json( ["Brightness", 0, -64, 64, 1, 0, "UVC image brightness"] )
    options.append( option )
    option = dds.option.from_json( ["Contrast", 50, 0, 100, 1, 50, "UVC image contrast"] )
    options.append( option )
    option = dds.option.from_json( ["Exposure", 156, 1, 10000, 1, 156, "Controls exposure time of color camera. Setting any value will disable auto exposure"] )
    options.append( option )
    option = dds.option.from_json( ["Gain", 64, 0, 128, 1, 64, "UVC image gain"] )
    options.append( option )
    option = dds.option.from_json( ["Gamma", 300, 100, 500, 1, 300, "UVC image gamma setting"] )
    options.append( option )
    option = dds.option.from_json( ["Hue", 0, -180, 180, 1, 0, "UVC image hue"] )
    options.append( option )
    option = dds.option.from_json( ["Saturation", 64, 0, 100, 1, 64, "UVC image saturation setting"] )
    options.append( option )
    option = dds.option.from_json( ["Sharpness", 50, 0, 100, 1, 50, "UVC image sharpness setting"] )
    options.append( option )
    option = dds.option.from_json( ["White Balance", 4600, 2800, 6500, 10, 4600, "Controls white balance of color image. Setting any value will disable auto white balance"] )
    options.append( option )
    option = dds.option.from_json( ["Enable Auto Exposure", 1, 0, 1, 1, 1, "Enable / disable auto-exposure"] )
    options.append( option )
    option = dds.option.from_json( ["Enable Auto White Balance", 1, 0, 1, 1, 1, "Enable / disable auto-white-balance"] )
    options.append( option )
    option = dds.option.from_json( ["Power Line Frequency", 3, 0, 3, 1, 3, "Power Line Frequency"] )
    options.append( option )
    option = dds.option.from_json( ["Auto Exposure Priority", 0, 0, 1, 1, 0, "Restrict Auto-Exposure to enforce constant FPS rate. Turn ON to remove the restrictions (may result in FPS drop)"] )
    options.append( option )
    option = dds.option.from_json( ["Global Time Enabled", 1, 0, 1, 1, 1, "Enable/Disable global timestamp"] )
    options.append( option )

    return options


def motion_module_options():
    options = []

    option = dds.option.from_json( ["Enable Motion Correction", 1, 0, 1, 1, 1, "Enable/Disable Automatic Motion Data Correction"] )
    options.append( option )
    option = dds.option.from_json( ["Global Time Enabled", 1, 0, 1, 1, 1, "Enable/Disable global timestamp"] )
    options.append( option )

    return options


def get_extrinsics():
    extrinsics = {}
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,-0.0011767612304538488,0.0007593693444505334,0.0011775976745411754,0.9999986886978149,-0.0011019925586879253,-0.0007580715464428067,0.0011028856970369816,0.9999991059303284)
    extr.translation = (0.05909481644630432,9.327199222752824e-05,-0.00016530796710867435)
    extrinsics[("Color","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,-0.0011767612304538488,0.0007593693444505334,0.0011775976745411754,0.9999986886978149,-0.0011019925586879253,-0.0007580715464428067,0.0011028856970369816,0.9999991059303284)
    extr.translation = (0.028874816372990608,0.007493271958082914,0.015854692086577415)
    extrinsics[("Color","Motion")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,-0.0011767612304538488,0.0007593693444505334,0.0011775976745411754,0.9999986886978149,-0.0011019925586879253,-0.0007580715464428067,0.0011028856970369816,0.9999991059303284)
    extr.translation = (0.05909481644630432,9.327199222752824e-05,-0.00016530796710867435)
    extrinsics[("Color","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,-0.0011767612304538488,0.0007593693444505334,0.0011775976745411754,0.9999986886978149,-0.0011019925586879253,-0.0007580715464428067,0.0011028856970369816,0.9999991059303284)
    extr.translation = (-0.035866111516952515,9.327199222752824e-05,-0.00016530796710867435)
    extrinsics[("Color","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,0.0011775976745411754,-0.0007580715464428067,-0.0011767612304538488,0.9999986886978149,0.0011028856970369816,0.0007593693444505334,-0.0011019925586879253,0.9999991059303284)
    extr.translation = (-0.05909452587366104,-0.0001630439655855298,0.00021000305423513055)
    extrinsics[("Depth","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.030220000073313713,0.007400000002235174,0.016019999980926514)
    extrinsics[("Depth","Motion")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Depth","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.09496092796325684,0.0,0.0)
    extrinsics[("Depth","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,0.0011775976745411754,-0.0007580715464428067,-0.0011767612304538488,0.9999986886978149,0.0011028856970369816,0.0007593693444505334,-0.0011019925586879253,0.9999991059303284)
    extr.translation = (-0.02887801080942154,-0.007509792689234018,-0.015841051936149597)
    extrinsics[("Motion","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.030220000073313713,-0.007400000002235174,-0.016019999980926514)
    extrinsics[("Motion","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.030220000073313713,-0.007400000002235174,-0.016019999980926514)
    extrinsics[("Motion","Infrared_1")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.06474092602729797,-0.007400000002235174,-0.016019999980926514)
    extrinsics[("Motion","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,0.0011775976745411754,-0.0007580715464428067,-0.0011767612304538488,0.9999986886978149,0.0011028856970369816,0.0007593693444505334,-0.0011019925586879253,0.9999991059303284)
    extr.translation = (-0.05909452587366104,-0.0001630439655855298,0.00021000305423513055)
    extrinsics[("Infrared_1","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.0,0.0,0.0)
    extrinsics[("Infrared_1","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.030220000073313713,0.007400000002235174,0.016019999980926514)
    extrinsics[("Infrared_1","Motion")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (-0.09496092796325684,0.0,0.0)
    extrinsics[("Infrared_1","Infrared_2")] = extr
    extr = dds.extrinsics();
    extr.rotation = (0.9999990463256836,0.0011775976745411754,-0.0007580715464428067,-0.0011767612304538488,0.9999986886978149,0.0011028856970369816,0.0007593693444505334,-0.0011019925586879253,0.9999991059303284)
    extr.translation = (0.03586631268262863,-5.1218194130342454e-05,0.00013801587920170277)
    extrinsics[("Infrared_2","Color")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.09496092796325684,0.0,0.0)
    extrinsics[("Infrared_2","Depth")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.06474092602729797,0.007400000002235174,0.016019999980926514)
    extrinsics[("Infrared_2","Motion")] = extr
    extr = dds.extrinsics();
    extr.rotation = (1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0)
    extr.translation = (0.09496092796325684,0.0,0.0)
    extrinsics[("Infrared_2","Infrared_1")] = extr

    return extrinsics
  
def color_stream_intrinsics():
    intrinsics = []

    intr = dds.video_intrinsics();
    intr.width = 1280
    intr.height = 800
    intr.principal_point.x = 648.580993652344
    intr.principal_point.y = 398.820983886719
    intr.focal_length.x = 636.450012207031
    intr.focal_length.y = 635.621948242188
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 1280
    intr.height = 720
    intr.principal_point.x = 648.580993652344
    intr.principal_point.y = 358.821014404297
    intr.focal_length.x = 636.450012207031
    intr.focal_length.y = 635.622009277344
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 848
    intr.height = 480
    intr.principal_point.x = 429.684906005859
    intr.principal_point.y = 239.21891784668
    intr.focal_length.x = 421.648132324219
    intr.focal_length.y = 421.099578857422
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 480
    intr.principal_point.x = 325.148590087891
    intr.principal_point.y = 239.292602539062
    intr.focal_length.x = 381.869995117188
    intr.focal_length.y = 381.373199462891
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 640
    intr.height = 360
    intr.principal_point.x = 324.290496826172
    intr.principal_point.y = 179.410507202148
    intr.focal_length.x = 318.225006103516
    intr.focal_length.y = 317.811004638672
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 480
    intr.height = 270
    intr.principal_point.x = 243.217880249023
    intr.principal_point.y = 134.557876586914
    intr.focal_length.x = 238.668746948242
    intr.focal_length.y = 238.358245849609
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    intr = dds.video_intrinsics();
    intr.width = 424
    intr.height = 240
    intr.principal_point.x = 214.84245300293
    intr.principal_point.y = 119.60945892334
    intr.focal_length.x = 210.824066162109
    intr.focal_length.y = 210.549789428711
    intr.distortion.model = dds.distortion_model.inverse_brown
    intr.distortion.coeffs = [-0.0569772012531757,0.0660239011049271,0.000211432998185046,0.00068545201793313,-0.0208512991666794]
    intrinsics.append( intr )

    return set( intrinsics )
