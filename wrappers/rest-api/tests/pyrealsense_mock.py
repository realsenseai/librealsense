# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Mock implementation of pyrealsense2 for testing purposes.
This mock implements all the classes and functions used in rs_manager.py.
"""
from unittest.mock import MagicMock
import enum
import numpy as np

# Enums used in the library
class stream(enum.Enum):
    depth = 1
    color = 2
    infrared = 3
    infrared2 = 4
    fisheye = 5
    gyro = 6
    accel = 7
    gpio = 8
    pose = 9
    confidence = 10
    motion = 11

class format(enum.Enum):
    z16 = 1
    y8 = 2
    rgb8 = 3
    bgr8 = 4
    rgba8 = 5
    bgra8 = 6
    yuyv = 7
    raw8 = 8
    raw10 = 9
    raw16 = 10
    xyz32f = 11
    any = 12
    motion_xyz32f = 13

class camera_info(enum.Enum):
    name = 1
    serial_number = 2
    firmware_version = 3
    physical_port = 4
    debug_op_code = 5
    advanced_mode = 6
    product_id = 7
    product_line = 8
    usb_type_descriptor = 9
    product_line_usb_type = 10
    recommended_firmware_version = 11
    count = 12

# Define option enums used in the options system
class option(enum.Enum):
    backlight_compensation = 1
    brightness = 2
    contrast = 3
    exposure = 4
    gain = 5
    gamma = 6
    hue = 7
    saturation = 8
    sharpness = 9
    white_balance = 10
    enable_auto_exposure = 11
    enable_auto_white_balance = 12
    visual_preset = 13
    laser_power = 14
    accuracy = 15
    motion_range = 16
    filter_option = 17
    confidence_threshold = 18
    emitter_enabled = 19
    frames_queue_size = 20
    total_frame_drops = 21
    auto_exposure_mode = 22
    power_line_frequency = 23
    asic_temperature = 24
    error_polling_enabled = 25
    projector_temperature = 26
    output_trigger_enabled = 27
    count = 28

    # Override name property for string representation
    @property
    def name(self):
        return self._name_

# Mock for option_range
class option_range:
    def __init__(self, min_val=0, max_val=100, default=50, step=1):
        self.min = min_val
        self.max = max_val
        self.default = default
        self.step = step

# Mock for rs.context class
class context:
    def __init__(self):
        self.devices = []

    def add_device(self, device):
        self.devices.append(device)

# Mock for rs.device class
class device:
    def __init__(self, serial_number="1234", name="Test Device"):
        self.serial = serial_number
        self.name = name
        self.sensors = []
        self._info = {
            camera_info.serial_number: serial_number,
            camera_info.name: name,
            camera_info.firmware_version: "1.0.0",
            camera_info.physical_port: "USB1",
            camera_info.usb_type_descriptor: "3.0",
            camera_info.product_id: "0123"
        }

    def get_info(self, info_type):
        if info_type in self._info:
            return self._info[info_type]
        raise RuntimeError(f"Info {info_type} not available")

    def add_sensor(self, sensor):
        self.sensors.append(sensor)

# Mock for sensor base class
class sensor:
    def __init__(self, name="Generic Sensor"):
        self.name = name
        self._info = {
            camera_info.name: name
        }
        self._options = {
            option.backlight_compensation: 0,
            option.brightness: 50,
            option.contrast: 50,
            option.exposure: 33,
            option.gain: 64
        }
        self._option_ranges = {
            option.backlight_compensation: option_range(0, 1, 0, 1),
            option.brightness: option_range(0, 100, 50, 1),
            option.contrast: option_range(0, 100, 50, 1),
            option.exposure: option_range(1, 66, 33, 1),
            option.gain: option_range(0, 128, 64, 1)
        }
        self._option_read_only = set()
        self._profiles = []

    def get_info(self, info_type):
        value = next(iter(self._info.values()))
        for key in self._info:
            if key.name == info_type.name:
                value = self._info[key]
                break
        return value

    def get_supported_options(self):
        return list(self._options.keys())

    def get_option(self, option_type):
        if option_type in self._options:
            return self._options[option_type]
        raise RuntimeError(f"Option {option_type} not supported")

    def set_option(self, option_type, value):
        if option_type in self._option_read_only:
            raise RuntimeError(f"Option {option_type} is read-only")
        if option_type in self._options:
            opt_range = self._option_ranges[option_type]
            if value < opt_range.min or value > opt_range.max:
                raise RuntimeError(f"Value {value} out of range [{opt_range.min}, {opt_range.max}]")
            self._options[option_type] = value
        else:
            raise RuntimeError(f"Option {option_type} not supported")

    def get_option_range(self, option_type):
        if option_type in self._option_ranges:
            return self._option_ranges[option_type]
        raise RuntimeError(f"Option range {option_type} not available")

    def get_option_description(self, option_type):
        return f"Description for {option_type}"

    def is_option_read_only(self, option_type):
        return option_type in self._option_read_only

    def get_stream_profiles(self):
        return self._profiles

    def add_profile(self, profile):
        self._profiles.append(profile)

    def is_depth_sensor(self):
        return False

    def get_recommended_filters(self):
        return []

    def is_color_sensor(self):
        return False

    def is_motion_sensor(self):
        return False

    def is_fisheye_sensor(self):
        return False

# Mock for depth sensor
class depth_sensor(sensor):
    def __init__(self, name="Depth Sensor"):
        super().__init__(name)
        # Add depth-specific options
        self._options[option.laser_power] = 10
        self._options[option.accuracy] = 2
        self._option_ranges[option.laser_power] = option_range(0, 100, 10, 1)
        self._option_ranges[option.accuracy] = option_range(1, 3, 2, 1)

    def is_depth_sensor(self):
        return True

# Mock for color sensor
class color_sensor(sensor):
    def __init__(self, name="Color Sensor"):
        super().__init__(name)
        # Add color-specific options
        self._options[option.saturation] = 64
        self._options[option.white_balance] = 4600
        self._option_ranges[option.saturation] = option_range(0, 100, 64, 1)
        self._option_ranges[option.white_balance] = option_range(2800, 6500, 4600, 10)

    def is_color_sensor(self):
        return True

# Mock for motion sensor
class motion_sensor(sensor):
    def __init__(self, name="Motion Sensor"):
        super().__init__(name)
        # Add motion-specific options
        self._options[option.motion_range] = 16
        self._option_ranges[option.motion_range] = option_range(0, 32, 16, 1)

    def is_motion_sensor(self):
        return True

# Mock for fisheye sensor
class fisheye_sensor(sensor):
    def __init__(self, name="Fisheye Sensor"):
        super().__init__(name)

    def is_fisheye_sensor(self):
        return True

# Mock for stream profile
class stream_profile:
    def __init__(self, stream_type=stream.depth, format=format.z16, index=0):
        self._stream_type = stream_type
        self._format = format
        self._index = index

    def stream_type(self):
        return self._stream_type

    def format(self):
        return self._format

    def index(self):
        return self._index

    def is_video_stream_profile(self):
        return isinstance(self, video_stream_profile)

    def as_video_stream_profile(self):
        if self.is_video_stream_profile():
            return self
        raise RuntimeError("Not a video stream profile")

# Mock for video stream profile
class video_stream_profile(stream_profile):
    def __init__(self, stream_type=stream.depth, format=format.z16, width=640, height=480, fps=30, index=0):
        super().__init__(stream_type, format, index)
        self._width = width
        self._height = height
        self._fps = fps

    def width(self):
        return self._width

    def height(self):
        return self._height

    def fps(self):
        return self._fps

# Mock for motion stream profile
class motion_stream_profile(stream_profile):
    def __init__(self, stream_type=stream.gyro, format=format.motion_xyz32f, fps=200, index=0):
        super().__init__(stream_type, format, index)
        self._fps = fps

    def fps(self):
        return self._fps

# Mock for frame
class frame:
    def __init__(self, width=640, height=480, timestamp=12345, frame_number=1):
        self._width = width
        self._height = height
        self._timestamp = timestamp
        self._frame_number = frame_number
        self._data = np.random.randint(0, 255, (height, width, 3), dtype=np.uint8)

    def get_width(self):
        return self._width

    def get_height(self):
        return self._height

    def get_timestamp(self):
        return self._timestamp

    def get_frame_number(self):
        return self._frame_number

    def get_data(self):
        return self._data

    def is_video_frame(self):
        return True

    def is_depth_frame(self):
        return isinstance(self, depth_frame)

    def is_color_frame(self):
        return isinstance(self, color_frame)

    def is_motion_frame(self):
        return isinstance(self, motion_frame)

    def as_depth_frame(self):
        if self.is_depth_frame():
            return self
        raise RuntimeError("Not a depth frame")

    def as_color_frame(self):
        if self.is_color_frame():
            return self
        raise RuntimeError("Not a color frame")

    def as_motion_frame(self):
        if self.is_motion_frame():
            return self
        raise RuntimeError("Not a motion frame")

# Mock for depth frame
class depth_frame(frame):
    def __init__(self, width=640, height=480, timestamp=12345, frame_number=1):
        super().__init__(width, height, timestamp, frame_number)
        # Generate random depth data (16-bit)
        self._depth_data = np.random.randint(0, 65535, (height, width), dtype=np.uint16)

    def get_distance(self, x, y):
        return self._depth_data[y, x] / 1000.0  # Convert to meters

# Mock for color frame
class color_frame(frame):
    def __init__(self, width=640, height=480, timestamp=12345, frame_number=1):
        super().__init__(width, height, timestamp, frame_number)
        # Generate random color data (RGB)
        self._data = np.random.randint(0, 255, (height, width, 3), dtype=np.uint8)

# Mock for motion frame
class motion_frame(frame):
    def __init__(self, timestamp=12345, frame_number=1):
        super().__init__(0, 0, timestamp, frame_number)

    def get_motion_data(self):
        class motion_data:
            def __init__(self):
                self.x = np.random.random()
                self.y = np.random.random()
                self.z = np.random.random()
        return motion_data()

# Mock for frameset
class frameset:
    def __init__(self):
        self.frames = {}

    def add_frame(self, stream_type, frame):
        self.frames[stream_type] = frame

    def get_depth_frame(self):
        if stream.depth in self.frames:
            return self.frames[stream.depth]
        return None

    def get_color_frame(self):
        if stream.color in self.frames:
            return self.frames[stream.color]
        return None

    def get_infrared_frame(self, index=1):
        stream_key = stream.infrared if index == 1 else stream.infrared2
        if stream_key in self.frames:
            return self.frames[stream_key]
        return None

    def get_motion_frame(self):
        if stream.motion in self.frames:
            return self.frames[stream.motion]
        return None

    def first_or_default(self, stream_type):
        if stream_type in self.frames:
            return self.frames[stream_type]
        return None

    def __iter__(self):
        return iter(self.frames.values())

# Mock for pipeline
class pipeline:
    def __init__(self, context=None):
        self.ctx = context if context else MagicMock()
        self.profile = None
        self.active = False
        self.device_id = None
        self.config = None
        self.frameset = frameset()

    def start(self, config=None):
        self.active = True
        self.config = config
        if config:
            self.device_id = config.device_id

        # Create mock pipeline profile
        self.profile = pipeline_profile(self.device_id)

        # Create mock frames based on the configuration
        if config and config.streams:
            for stream_cfg in config.streams:
                stream_type = stream_cfg["stream"]
                frame_obj = None

                if stream_type == stream.depth:
                    frame_obj = depth_frame(stream_cfg.get("width", 640), stream_cfg.get("height", 480))
                elif stream_type == stream.color:
                    frame_obj = color_frame(stream_cfg.get("width", 640), stream_cfg.get("height", 480))
                elif stream_type in [stream.gyro, stream.accel]:
                    frame_obj = motion_frame()
                else:
                    frame_obj = frame(stream_cfg.get("width", 640), stream_cfg.get("height", 480))

                self.frameset.add_frame(stream_type, frame_obj)

        return self.profile

    def stop(self):
        self.active = False
        self.profile = None

    def wait_for_frames(self, timeout_ms=5000):
        if not self.active:
            raise RuntimeError("Pipeline not started")
        # Return a copy of the frameset to simulate a new set of frames
        fs = frameset()
        for stream_type, frame in self.frameset.frames.items():
            fs.add_frame(stream_type, frame)
        return fs

# Mock for pipeline profile
class pipeline_profile:
    def __init__(self, device_id=None):
        self.device_id = device_id

    def get_device(self):
        return device(self.device_id)

    def get_streams(self):
        return []

# Mock for config
class config:
    def __init__(self):
        self.device_id = None
        self.streams = []

    def enable_device(self, device_id):
        self.device_id = device_id

    def enable_stream(self, stream_type, width=0, height=0, format=format.any, framerate=0):
        self.streams.append({
            "stream": stream_type,
            "width": width,
            "height": height,
            "format": format,
            "framerate": framerate
        })

    def disable_stream(self, stream_type):
        self.streams = [s for s in self.streams if s["stream"] != stream_type]

    def resolve(self, pipeline):
        # Return a pipeline profile
        return pipeline_profile(self.device_id)

# Mock for align
class align:
    def __init__(self, stream_type):
        self.stream_type = stream_type

    def process(self, frameset):
        # Just return the original frameset, since we're mocking
        return frameset

# Mock for pointcloud
class pointcloud:
    def __init__(self):
        pass

    def calculate(self, depth_frame):
        # Return a points object
        return points()

    def map_to(self, frame, mapped_frame):
        # Return a points object
        return points()

# Mock for points
class points:
    def __init__(self):
        pass

    def get_vertices(self):
        # Return random 3D points
        return np.random.random((100, 3)).astype(np.float32)

    def get_texture_coordinates(self):
        # Return random texture coordinates
        return np.random.random((100, 2)).astype(np.float32)

# Mock for colorizer
class colorizer:
    def __init__(self):
        pass

    def colorize(self, depth_frame):
        # Return a color frame from a depth frame
        width = depth_frame.get_width()
        height = depth_frame.get_height()
        # Create a colorized version of the depth frame
        colorized = color_frame(width, height, depth_frame.get_timestamp(), depth_frame.get_frame_number())
        # Generate a blue-to-red heatmap look
        colorized._data = np.zeros((height, width, 3), dtype=np.uint8)
        depth_data = depth_frame._depth_data.astype(float) / 65535.0  # Normalize to 0-1
        colorized._data[:,:,0] = (1.0 - depth_data) * 255  # Red (far)
        colorized._data[:,:,2] = depth_data * 255  # Blue (near)
        return colorized

# Mock for hole_filling_filter
class hole_filling_filter:
    def __init__(self):
        pass

    def process(self, frame):
        # Return the same frame, pretending we've filled holes
        return frame

# Mock for decimation_filter
class decimation_filter:
    def __init__(self):
        pass

    def process(self, frame):
        # Return the same frame, pretending we've decimated it
        return frame

# Mock for temporal_filter
class temporal_filter:
    def __init__(self):
        pass

    def process(self, frame):
        # Return the same frame, pretending we've applied temporal filtering
        return frame

# Mock for spatial_filter
class spatial_filter:
    def __init__(self):
        pass

    def process(self, frame):
        # Return the same frame, pretending we've applied spatial filtering
        return frame

# Mock for disparity_transform
class disparity_transform:
    def __init__(self, transform_to_disparity=True):
        self.transform_to_disparity = transform_to_disparity

    def process(self, frame):
        # Return the same frame, pretending we've transformed it
        return frame

# Mock exception types
class error(Exception):
    pass

class camera_disconnected_error(error):
    pass

class invalid_value_error(error):
    pass

class wrong_api_call_sequence_error(error):
    pass

class not_implemented_error(error):
    pass

# Helper functions to set up mock devices
def create_mock_device(serial_number, name, with_depth=True, with_color=True, with_motion=False, with_fisheye=False):
    """
    Create a mock device with the specified sensors
    """
    mock_device = device(serial_number, name)

    if with_depth:
        depth_sensor_obj = depth_sensor("Depth Sensor")
        # Add depth stream profiles
        for res in [(640, 480), (1280, 720)]:
            for fps in [30, 60]:
                depth_sensor_obj.add_profile(video_stream_profile(
                    stream.depth, format.z16, res[0], res[1], fps
                ))
        mock_device.add_sensor(depth_sensor_obj)

    if with_color:
        color_sensor_obj = color_sensor("RGB Camera")
        # Add color stream profiles
        for res in [(640, 480), (1280, 720), (1920, 1080)]:
            for fps in [30, 60]:
                for fmt in [format.rgb8, format.bgr8]:
                    color_sensor_obj.add_profile(video_stream_profile(
                        stream.color, fmt, res[0], res[1], fps
                    ))
        mock_device.add_sensor(color_sensor_obj)

    if with_motion:
        motion_sensor_obj = motion_sensor("Motion Module")
        # Add motion stream profiles
        for fps in [200, 400]:
            motion_sensor_obj.add_profile(motion_stream_profile(stream.gyro, format.motion_xyz32f, fps))
            motion_sensor_obj.add_profile(motion_stream_profile(stream.accel, format.motion_xyz32f, fps))
        mock_device.add_sensor(motion_sensor_obj)

    if with_fisheye:
        fisheye_sensor_obj = fisheye_sensor("Fisheye Camera")
        # Add fisheye stream profiles
        for res in [(640, 480), (1280, 720)]:
            for fps in [30, 60]:
                fisheye_sensor_obj.add_profile(video_stream_profile(
                    stream.fisheye, format.y8, res[0], res[1], fps
                ))
        mock_device.add_sensor(fisheye_sensor_obj)

    return mock_device

def setup_mock_context():
    """
    Set up a mock context with multiple devices
    """
    ctx = context()

    # Add some mock devices
    device1 = create_mock_device("1234", "RealSense D435", with_depth=True, with_color=True)
    device2 = create_mock_device("5678", "RealSense D415", with_depth=True, with_color=True)
    device3 = create_mock_device("9012", "RealSense T265", with_fisheye=True, with_motion=True)

    ctx.add_device(device1)
    ctx.add_device(device2)
    ctx.add_device(device3)

    return ctx