# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from time import time
from .pyrealsense_mock import context, create_mock_device

def setup_fake_devices():
    """
    Creates a set of fake RealSense devices for testing.
    Returns a dictionary containing all the mock objects.
    """
    # Create a mock context
    mock_context = context()

    # Create two fake devices with depth and color sensors
    fake_device1 = create_mock_device(
        "device1", "Test Device 1", with_depth=True, with_color=True
    )
    fake_device2 = create_mock_device(
        "device2", "Test Device 2", with_depth=True, with_color=True
    )

    # Add the devices to the context
    mock_context.add_device(fake_device1)
    mock_context.add_device(fake_device2)

    # Extract sensors for easy access in tests
    depth_sensors = []
    color_sensors = []

    # Collect sensors from device 1
    for i, sensor in enumerate(fake_device1.sensors):
        if sensor.is_depth_sensor():
            depth_sensors.append(sensor)
        elif sensor.is_color_sensor():
            color_sensors.append(sensor)

    # Collect sensors from device 2
    for i, sensor in enumerate(fake_device2.sensors):
        if sensor.is_depth_sensor():
            depth_sensors.append(sensor)
        elif sensor.is_color_sensor():
            color_sensors.append(sensor)

    # Return all mock objects in a dictionary
    return {
        "context": mock_context,
        "devices": [fake_device1, fake_device2],
        "depth_sensors": depth_sensors,
        "color_sensors": color_sensors,
    }