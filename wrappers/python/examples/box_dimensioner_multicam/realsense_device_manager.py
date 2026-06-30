##################################################################################################
##       License: Apache 2.0. See LICENSE file in root directory.                             ####
##################################################################################################
##                  Box Dimensioner with multiple cameras: Helper files                       ####
##################################################################################################


import pyrealsense2 as rs
import numpy as np

"""
  _   _        _                      _____                     _    _
 | | | |  ___ | | _ __    ___  _ __  |  ___|_   _  _ __    ___ | |_ (_)  ___   _ __   ___
 | |_| | / _ \| || '_ \  / _ \| '__| | |_  | | | || '_ \  / __|| __|| | / _ \ | '_ \ / __|
 |  _  ||  __/| || |_) ||  __/| |    |  _| | |_| || | | || (__ | |_ | || (_) || | | |\__ \
 |_| |_| \___||_|| .__/  \___||_|    |_|    \__,_||_| |_| \___| \__||_| \___/ |_| |_||___/
                 |_|
"""


class Device:
    def __init__(self, pipeline, pipeline_profile, product_line):
        self.pipeline = pipeline
        self.pipeline_profile = pipeline_profile
        self.product_line = product_line

def enumerate_connected_devices(context):
    """
    Enumerate the connected RealSense devices

    Parameters:
    -----------
    context 	   : rs.context()
                     The context created for using the realsense library

    Return:
    -----------
    connect_device : array
                     Array of (serial, product-line) tuples of devices which are connected to the PC

    """
    connect_device = []

    for d in context.devices:
        if d.get_info(rs.camera_info.name).lower() != 'platform camera':
            serial = d.get_info(rs.camera_info.serial_number)
            product_line = d.get_info(rs.camera_info.product_line)
            device_info = (serial, product_line) # (serial_number, product_line)
            connect_device.append( device_info )
    return connect_device


def post_process_depth_frame(depth_frame, decimation_magnitude=1.0, spatial_magnitude=2.0, spatial_smooth_alpha=0.5,
                             spatial_smooth_delta=20, temporal_smooth_alpha=0.4, temporal_smooth_delta=20):
    """
    Filter the depth frame acquired using the RealSense device

    Parameters:
    -----------
    depth_frame          : rs.frame()
                           The depth frame to be post-processed
    decimation_magnitude : double
                           The magnitude of the decimation filter
    spatial_magnitude    : double
                           The magnitude of the spatial filter
    spatial_smooth_alpha : double
                           The alpha value for spatial filter based smoothening
    spatial_smooth_delta : double
                           The delta value for spatial filter based smoothening
    temporal_smooth_alpha: double
                           The alpha value for temporal filter based smoothening
    temporal_smooth_delta: double
                           The delta value for temporal filter based smoothening

    Return:
    ----------
    filtered_frame : rs.frame()
                     The post-processed depth frame
    """

    # Post processing possible only on the depth_frame
    assert (depth_frame.is_depth_frame())

    # Available filters and control options for the filters
    decimation_filter = rs.decimation_filter()
    spatial_filter = rs.spatial_filter()
    temporal_filter = rs.temporal_filter()

    filter_magnitude = rs.option.filter_magnitude
    filter_smooth_alpha = rs.option.filter_smooth_alpha
    filter_smooth_delta = rs.option.filter_smooth_delta

    # Apply the control parameters for the filter
    decimation_filter.set_option(filter_magnitude, decimation_magnitude)
    spatial_filter.set_option(filter_magnitude, spatial_magnitude)
    spatial_filter.set_option(filter_smooth_alpha, spatial_smooth_alpha)
    spatial_filter.set_option(filter_smooth_delta, spatial_smooth_delta)
    temporal_filter.set_option(filter_smooth_alpha, temporal_smooth_alpha)
    temporal_filter.set_option(filter_smooth_delta, temporal_smooth_delta)

    # Apply the filters
    filtered_frame = decimation_filter.process(depth_frame)
    filtered_frame = spatial_filter.process(filtered_frame)
    filtered_frame = temporal_filter.process(filtered_frame)

    return filtered_frame


"""
  __  __         _           ____               _                _
 |  \/  |  __ _ (_) _ __    / ___| ___   _ __  | |_  ___  _ __  | |_
 | |\/| | / _` || || '_ \  | |    / _ \ | '_ \ | __|/ _ \| '_ \ | __|
 | |  | || (_| || || | | | | |___| (_) || | | || |_|  __/| | | || |_
 |_|  |_| \__,_||_||_| |_|  \____|\___/ |_| |_| \__|\___||_| |_| \__|

"""


class DeviceManager:
    def __init__(self, context, D400_pipeline_configuration):
        """
        Class to manage the RealSense devices

        Parameters:
        -----------
        context                 : rs.context()
                                  The context created for using the realsense library
        D400_pipeline_configuration  : rs.config()
                                  The realsense library configuration to be used for the application when D400 product is attached.

        """
        assert isinstance(context, type(rs.context()))
        assert isinstance(D400_pipeline_configuration, type(rs.config()))
        self._context = context
        self._available_devices = enumerate_connected_devices(context)
        self._enabled_devices = {} #serial numbers of te enabled devices
        self.D400_config = D400_pipeline_configuration
        self._frame_counter = 0

    def enable_device(self, device_info, enable_ir_emitter):
        """
        Enable a RealSense Device

        Parameters:
        -----------
        device_info     : Tuple of strings (serial_number, product_line)
                            Serial number and product line of the realsense device
        enable_ir_emitter : bool
                            Enable/Disable the IR-Emitter of the device

        """
        pipeline = rs.pipeline()

        device_serial = device_info[0]
        product_line = device_info[1]

        if product_line == "D400":
            # Enable D400 device
            self.D400_config.enable_device(device_serial)
            pipeline_profile = pipeline.start(self.D400_config)
        else:
            raise RuntimeError( f'unknown product line {product_line}' )


        # Set the acquisition parameters
        sensor = pipeline_profile.get_device().first_depth_sensor()
        if sensor.supports(rs.option.emitter_enabled):
            sensor.set_option(rs.option.emitter_enabled, 1 if enable_ir_emitter else 0)
        self._enabled_devices[device_serial] = (Device(pipeline, pipeline_profile, product_line))

    def enable_all_devices(self, enable_ir_emitter=False):
        """
        Enable all the RealSense Devices which are connected to the PC

        """
        print(str(len(self._available_devices)) + " devices have been found")

        for device_info in self._available_devices:
            self.enable_device(device_info, enable_ir_emitter)

    def enable_emitter(self, enable_ir_emitter=True):
        """
        Enable/Disable the emitter of the intel realsense device

        """
        for (device_serial, device) in self._enabled_devices.items():
            # Get the active profile and enable the emitter for all the connected devices
            sensor = device.pipeline_profile.get_device().first_depth_sensor()
            if not sensor.supports(rs.option.emitter_enabled):
                continue
            sensor.set_option(rs.option.emitter_enabled, 1 if enable_ir_emitter else 0)
            if enable_ir_emitter:
                sensor.set_option(rs.option.laser_power, 330)

    def load_settings_json(self, path_to_settings_file):
        """
        Load the settings stored in the JSON file

        """

        with open(path_to_settings_file, 'r') as file:
        	json_text = file.read().strip()

        for (device_serial, device) in self._enabled_devices.items():
            if device.product_line != "D400":
                continue
            # Get the active profile and load the json file which contains settings readable by the realsense
            device = device.pipeline_profile.get_device()
            advanced_mode = rs.rs400_advanced_mode(device)
            advanced_mode.load_json(json_text)

    def poll_frames(self):
        """
        Poll for frames from the enabled RealSense devices. This will return at least one frame from each device.
        If temporal post processing is enabled, the depth stream is averaged over a certain amount of frames

        Parameters:
        -----------
        """
        frames = {}
        while len(frames) < len(self._enabled_devices.items()) :
            for (serial, device) in self._enabled_devices.items():
                streams = device.pipeline_profile.get_streams()
                frameset = device.pipeline.poll_for_frames() #frameset will be a pyrealsense2.composite_frame object
                if frameset.size() == len(streams):
                    dev_info = (serial, device.product_line)
                    frames[dev_info] = {}
                    for stream in streams:
                        if (rs.stream.infrared == stream.stream_type()):
                            frame = frameset.get_infrared_frame(stream.stream_index())
                            key_ = (stream.stream_type(), stream.stream_index())
                        else:
                            frame = frameset.first_or_default(stream.stream_type())
                            key_ = stream.stream_type()
                        frames[dev_info][key_] = frame

        return frames

    def get_depth_shape(self):
        """
        Retruns width and height of the depth stream for one arbitrary device

        Returns:
        -----------
        width : int
        height: int
        """
        width = -1
        height = -1
        for (serial, device) in self._enabled_devices.items():
            for stream in device.pipeline_profile.get_streams():
                if (rs.stream.depth == stream.stream_type()):
                    width = stream.as_video_stream_profile().width()
                    height = stream.as_video_stream_profile().height()
        return width, height

    def get_device_intrinsics(self, frames):
        """
        Get the intrinsics of the imager using its frame delivered by the realsense device

        Parameters:
        -----------
        frames : rs::frame
                 The frame grabbed from the imager inside the RealSense for which the intrinsic is needed

        Return:
        -----------
        device_intrinsics : dict
        keys  : serial
                Serial number of the device
        values: [key]
                Intrinsics of the corresponding device
        """
        device_intrinsics = {}
        for (dev_info, frameset) in frames.items():
            serial = dev_info[0]
            device_intrinsics[serial] = {}
            for key, value in frameset.items():
                device_intrinsics[serial][key] = value.get_profile().as_video_stream_profile().get_intrinsics()
        return device_intrinsics

    def get_depth_to_color_extrinsics(self, frames):
        """
        Get the extrinsics between the depth imager 1 and the color imager using its frame delivered by the realsense device

        Parameters:
        -----------
        frames : rs::frame
                 The frame grabbed from the imager inside the RealSense for which the intrinsic is needed

        Return:
        -----------
        device_intrinsics : dict
        keys  : serial
                Serial number of the device
        values: [key]
                Extrinsics of the corresponding device
        """
        device_extrinsics = {}
        for (dev_info, frameset) in frames.items():
            serial = dev_info[0]
            device_extrinsics[serial] = frameset[
                rs.stream.depth].get_profile().as_video_stream_profile().get_extrinsics_to(
                frameset[rs.stream.color].get_profile())
        return device_extrinsics

    def disable_streams(self):
        self.D400_config.disable_all_streams()


"""
  _____           _    _
 |_   _|___  ___ | |_ (_) _ __    __ _
   | | / _ \/ __|| __|| || '_ \  / _` |
   | ||  __/\__ \| |_ | || | | || (_| |
   |_| \___||___/ \__||_||_| |_| \__, |
                                  |___/

"""
if __name__ == "__main__":
    try:
        c = rs.config()
        c.enable_stream(rs.stream.depth, 1280, 720, rs.format.z16, 6)
        c.enable_stream(rs.stream.infrared, 1, 1280, 720, rs.format.y8, 6)
        c.enable_stream(rs.stream.infrared, 2, 1280, 720, rs.format.y8, 6)
        c.enable_stream(rs.stream.color, 1280, 720, rs.format.rgb8, 6)
        device_manager = DeviceManager(rs.context(), c)
        device_manager.enable_all_devices()
        for k in range(150):
            frames = device_manager.poll_frames()
        device_manager.enable_emitter(True)
        device_extrinsics = device_manager.get_depth_to_color_extrinsics(frames)
    finally:
        device_manager.disable_streams()
