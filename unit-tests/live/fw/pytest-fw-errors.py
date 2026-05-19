# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging
import threading

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
from rspy.stopwatch import Stopwatch

log = logging.getLogger(__name__)

# This test monitors firmware error notifications during streaming to ensure hardware stability

pytestmark = [
    pytest.mark.device_each("D400*"),
    pytest.mark.device_each("D500*"),
    pytest.mark.context("nightly"),
]

STREAMING_DURATION_SECONDS = 10  # Duration to stream and monitor for FW errors
ERROR_TOLERANCE = 0  # No firmware errors should be tolerated

def get_known_errors_for_firmware(current_fw_version):
    """Get list of known errors based on firmware version"""
    known_errors = []

    # Motion Module failure is a known issue starting before firmware 5.17.0.12
    if (rsutils.version(current_fw_version) < rsutils.version(5, 17, 0, 12)):
        known_errors.append("Motion Module failure")  # See also RSDSO-20645
        # Add other version-specific known errors here as needed

    return known_errors

class FirmwareErrorMonitor:
    def __init__(self, known_errors=None):
        self.firmware_errors = []
        self.hardware_errors = []
        self.known_errors = []
        self.known_error_list = known_errors or []
        self.lock = threading.Lock()

    def notification_callback(self, notification):
        """Callback to handle firmware notifications"""
        category = notification.get_category()
        severity = notification.get_severity()
        description = notification.get_description()
        timestamp = notification.get_timestamp()

        with self.lock:
            log.debug("Notification received - Category: %s, Severity: %s, Description: %s", category, severity, description)

            # Check if this is a known error that should be ignored
            is_known_error = any(known_error in description for known_error in self.known_error_list)

            if is_known_error:
                error_info = {
                    'category': category,
                    'severity': severity,
                    'description': description,
                    'timestamp': timestamp
                }
                self.known_errors.append(error_info)
                log.warning("Known error detected (ignored): %s", description)
                return  # Don't count as failure

            # Check for hardware errors (firmware errors typically fall under this category)
            if category == rs.notification_category.hardware_error:
                error_info = {
                    'category': category,
                    'severity': severity,
                    'description': description,
                    'timestamp': timestamp
                }
                self.hardware_errors.append(error_info)
                log.warning("Hardware error detected: %s", description)

            # Check for unknown errors which might include firmware issues
            elif category == rs.notification_category.unknown_error:
                error_info = {
                    'category': category,
                    'severity': severity,
                    'description': description,
                    'timestamp': timestamp
                }
                self.firmware_errors.append(error_info)
                log.warning("Unknown error detected: %s", description)

    def get_error_count(self):
        """Get total number of firmware-related errors (excluding known errors)"""
        with self.lock:
            return len(self.firmware_errors) + len(self.hardware_errors)

    def get_error_summary(self):
        """Get summary of all detected errors"""
        with self.lock:
            return {
                'firmware_errors': self.firmware_errors.copy(),
                'hardware_errors': self.hardware_errors.copy(),
                'known_errors': self.known_errors.copy(),
                'total_errors': len(self.firmware_errors) + len(self.hardware_errors)
            }


def test_firmware_error_monitoring(test_device):
    device, ctx = test_device

    # Get device info for logging
    device_name = device.get_info(rs.camera_info.name)
    device_serial = device.get_info(rs.camera_info.serial_number)
    firmware_version = device.get_info(rs.camera_info.firmware_version)

    log.info("Testing device: %s (S/N: %s, FW: %s)", device_name, device_serial, firmware_version)

    # Get known errors for this firmware version
    known_errors = get_known_errors_for_firmware(firmware_version)
    if known_errors:
        log.info("Known errors for firmware %s: %s", firmware_version, ', '.join(known_errors))
    else:
        log.info("No known errors defined for firmware %s", firmware_version)

    # Set up error monitor with firmware-specific known errors
    error_monitor = FirmwareErrorMonitor(known_errors)

    # Query all available sensors and register notification callbacks
    sensors = device.query_sensors()
    registered_sensors = []

    for sensor in sensors:
        try:
            sensor_name = sensor.get_info(rs.camera_info.name)
            sensor.set_notifications_callback(error_monitor.notification_callback)
            registered_sensors.append(sensor_name)
            log.debug("Registered notification callback for %s", sensor_name)
        except Exception as e:
            sensor_name = "Unknown sensor"
            try:
                sensor_name = sensor.get_info(rs.camera_info.name)
            except Exception:
                pass
            log.warning("Could not register notification callback for %s: %s", sensor_name, e)

    log.info("Monitoring firmware errors on %d sensors: %s", len(registered_sensors), ', '.join(registered_sensors))

    # Start streaming
    pipe = rs.pipeline(ctx)

    try:
        pipe.start()
        log.info("Started streaming, monitoring for firmware errors for %d seconds...", STREAMING_DURATION_SECONDS)

        stopwatch = Stopwatch()
        frame_count = 0
        last_log_time = 0

        # Stream for specified duration while monitoring for errors
        while stopwatch.get_elapsed() < STREAMING_DURATION_SECONDS:
            try:
                frames = pipe.wait_for_frames()

                # Verify we're getting frames
                if frames:
                    frame_count += 1
                    elapsed = stopwatch.get_elapsed()

                    # Log every second using stopwatch timing
                    if elapsed - last_log_time >= 1.0:
                        error_count = error_monitor.get_error_count()
                        log.debug("Streaming progress: %.1fs, %d frames, %d errors detected", elapsed, frame_count, error_count)
                        last_log_time = elapsed

            except RuntimeError as e:
                log.warning("Frame timeout or error during streaming: %s", e)
                # Continue monitoring even if individual frames fail

        elapsed_time = stopwatch.get_elapsed()
        log.info("Streaming completed - Duration: %.1fs, Total frames: %d", elapsed_time, frame_count)

    finally:
        pipe.stop()
        log.debug("Pipeline stopped")

    # Analyze results
    error_summary = error_monitor.get_error_summary()
    total_errors = error_summary['total_errors']

    log.info("Firmware error monitoring results:")
    log.info("  - Total errors detected: %d", total_errors)
    log.info("  - Hardware errors: %d", len(error_summary['hardware_errors']))
    log.info("  - Unknown/Firmware errors: %d", len(error_summary['firmware_errors']))
    log.info("  - Known errors (ignored): %d", len(error_summary['known_errors']))

    # Log detailed error information if any errors were found
    if total_errors > 0:
        log.warning("Detailed error information:")

        for i, error in enumerate(error_summary['hardware_errors']):
            log.warning("  Hardware Error %d: %s (Severity: %s)", i + 1, error['description'], error['severity'])

        for i, error in enumerate(error_summary['firmware_errors']):
            log.warning("  Firmware Error %d: %s (Severity: %s)", i + 1, error['description'], error['severity'])

    # Log known errors for informational purposes
    if len(error_summary['known_errors']) > 0:
        log.info("Known errors detected (ignored for test result):")
        for i, error in enumerate(error_summary['known_errors']):
            log.info("  Known Error %d: %s (Severity: %s)", i + 1, error['description'], error['severity'])

    # Test assertion - no firmware errors should be detected
    assert total_errors == ERROR_TOLERANCE

    log.info("Firmware error monitoring test completed successfully")
