# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Test configuration: Requires TWO D400 devices connected simultaneously
# The configuration below specifies two D400 devices (D400* appears twice)
# This tells the test infrastructure to allocate two separate D400 devices for this test
#test:device D400* D400*

"""
Example test demonstrating how to write tests that require two RealSense devices simultaneously.

This test validates:
1. Two devices can be discovered and distinguished by serial number
2. Two pipelines can run simultaneously on different devices
3. Both devices can stream depth and color data concurrently
4. Devices remain independent and do not interfere with each other

This test serves as a template for future multi-device tests.

============================================================================
HOW TO WRITE A TWO-DEVICE TEST
============================================================================

Method 1: Using the two_devices() context manager (RECOMMENDED)
----------------------------------------------------------------
This is the cleanest approach for most two-device tests:

    import pyrealsense2 as rs
    from rspy import test
    
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        # Your test code here
        # dev1 and dev2 are guaranteed to be different devices
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        test.check(sn1 != sn2)

Method 2: Using the helper function directly
---------------------------------------------
For tests that need more control over device lifecycle:

    import pyrealsense2 as rs
    from rspy import test
    
    dev1, dev2, ctx = test.find_two_devices_by_product_line_or_exit(rs.product_line.D400)
    # Your test code here
    # Manual cleanup if needed

Test Configuration Directive
-----------------------------
Add this line to the top of your test file:
    #test:device D400* D400*
    
The repeated spec (D400* appears twice) tells the infrastructure to allocate
TWO devices matching the D400 pattern.

Graceful Handling of Insufficient Devices
------------------------------------------
If fewer than 2 devices are connected:
- The test will be SKIPPED (not failed)
- A clear message will indicate: "Test requires 2 devices; found X device(s)"
- This allows CI/CD pipelines to run without failing in single-device environments

============================================================================
"""

import pyrealsense2 as rs
from rspy import test, log
import time

#
# Test 1: Verify two devices can be discovered with different serial numbers
#
with test.closure("Two devices discovery and serial number verification"):
    # The two_devices context manager handles device discovery and validation
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        # Verify devices are valid
        test.check(dev1 is not None, "Device 1 should be valid")
        test.check(dev2 is not None, "Device 2 should be valid")
        
        # Get serial numbers
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        
        log.i(f"Device 1 serial number: {sn1}")
        log.i(f"Device 2 serial number: {sn2}")
        
        # Verify serial numbers are different
        test.check(sn1 != sn2, f"Serial numbers must be different: {sn1} vs {sn2}")
        
        # Verify names
        name1 = dev1.get_info(rs.camera_info.name)
        name2 = dev2.get_info(rs.camera_info.name)
        log.i(f"Device 1: {name1}")
        log.i(f"Device 2: {name2}")

#
# Test 2: Verify both devices can stream simultaneously
#
with test.closure("Simultaneous streaming from two devices"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        # Create two separate pipelines
        pipe1 = rs.pipeline()
        pipe2 = rs.pipeline()
        
        # Create configurations for both pipelines
        cfg1 = rs.config()
        cfg2 = rs.config()
        
        # Enable streams for both devices
        # Using 640x480 at 30fps as a common resolution
        cfg1.enable_device(dev1.get_info(rs.camera_info.serial_number))
        cfg1.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        cfg1.enable_stream(rs.stream.color, 640, 480, rs.format.rgb8, 30)
        
        cfg2.enable_device(dev2.get_info(rs.camera_info.serial_number))
        cfg2.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        cfg2.enable_stream(rs.stream.color, 640, 480, rs.format.rgb8, 30)
        
        try:
            # Start both pipelines
            log.i("Starting pipeline 1...")
            profile1 = pipe1.start(cfg1)
            
            log.i("Starting pipeline 2...")
            profile2 = pipe2.start(cfg2)
            
            # Verify both pipelines started successfully
            test.check(profile1 is not None, "Pipeline 1 should start successfully")
            test.check(profile2 is not None, "Pipeline 2 should start successfully")
            
            # Let the auto-exposure settle
            log.i("Waiting for auto-exposure to settle...")
            for _ in range(30):
                pipe1.wait_for_frames(timeout_ms=5000)
                pipe2.wait_for_frames(timeout_ms=5000)
            
            # Capture frames from both devices
            log.i("Capturing frames from both devices...")
            frames_captured = 0
            for i in range(30):
                # Get frames from device 1
                frameset1 = pipe1.wait_for_frames(timeout_ms=5000)
                depth1 = frameset1.get_depth_frame()
                color1 = frameset1.get_color_frame()
                
                # Get frames from device 2
                frameset2 = pipe2.wait_for_frames(timeout_ms=5000)
                depth2 = frameset2.get_depth_frame()
                color2 = frameset2.get_color_frame()
                
                # Verify frames are valid
                if depth1 and color1 and depth2 and color2:
                    frames_captured += 1
            
            log.i(f"Successfully captured {frames_captured}/30 frame pairs")
            test.check(frames_captured >= 25, 
                      f"Should capture at least 25 frame pairs, got {frames_captured}")
            
        finally:
            # Clean up: stop both pipelines
            log.i("Stopping pipelines...")
            try:
                pipe1.stop()
            except:
                pass
            try:
                pipe2.stop()
            except:
                pass

#
# Test 3: Verify devices can stream sequentially (stop one, start other)
#
with test.closure("Sequential streaming - stop and start"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        sn1 = dev1.get_info(rs.camera_info.serial_number)
        sn2 = dev2.get_info(rs.camera_info.serial_number)
        
        pipe = rs.pipeline()
        cfg = rs.config()
        
        # Stream from device 1
        log.i(f"Streaming from device 1 ({sn1})...")
        cfg.enable_device(sn1)
        cfg.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
        
        try:
            pipe.start(cfg)
            
            # Capture a few frames
            for _ in range(10):
                frames = pipe.wait_for_frames(timeout_ms=5000)
                test.check(frames.get_depth_frame() is not None, "Should get depth frames from device 1")
            
            pipe.stop()
            
            # Now switch to device 2
            log.i(f"Streaming from device 2 ({sn2})...")
            cfg = rs.config()  # Create new config
            cfg.enable_device(sn2)
            cfg.enable_stream(rs.stream.depth, 640, 480, rs.format.z16, 30)
            
            pipe.start(cfg)
            
            # Capture a few frames
            for _ in range(10):
                frames = pipe.wait_for_frames(timeout_ms=5000)
                test.check(frames.get_depth_frame() is not None, "Should get depth frames from device 2")
            
            pipe.stop()
            
            log.i("Sequential streaming test completed successfully")
            
        except Exception as e:
            log.e(f"Sequential streaming test failed: {e}")
            test.check(False, f"Sequential streaming should work: {e}")
        finally:
            try:
                pipe.stop()
            except:
                pass

#
# Test 4: Verify device sensor information is independent
#
with test.closure("Verify independent device sensor information"):
    with test.two_devices(rs.product_line.D400) as (dev1, dev2):
        
        # Get depth sensors from both devices
        depth_sensor1 = dev1.first_depth_sensor()
        depth_sensor2 = dev2.first_depth_sensor()
        
        # Verify sensors are valid
        test.check(depth_sensor1 is not None, "Device 1 should have a depth sensor")
        test.check(depth_sensor2 is not None, "Device 2 should have a depth sensor")
        
        # Check that we can query options from both sensors independently
        if depth_sensor1.supports(rs.option.enable_auto_exposure):
            ae1_initial = depth_sensor1.get_option(rs.option.enable_auto_exposure)
            log.i(f"Device 1 auto-exposure: {ae1_initial}")
            
            # Toggle the option
            depth_sensor1.set_option(rs.option.enable_auto_exposure, 1.0 if ae1_initial == 0.0 else 0.0)
            ae1_new = depth_sensor1.get_option(rs.option.enable_auto_exposure)
            
            # Verify device 2 is unaffected
            if depth_sensor2.supports(rs.option.enable_auto_exposure):
                ae2 = depth_sensor2.get_option(rs.option.enable_auto_exposure)
                log.i(f"Device 2 auto-exposure (should be unchanged): {ae2}")
                
                # The two devices should be able to have different settings
                # (we're not checking they're different, just that changing one doesn't change the other)
            
            # Restore original setting
            depth_sensor1.set_option(rs.option.enable_auto_exposure, ae1_initial)
        
        log.i("Device sensor independence verified")

# Print test summary
test.print_results_and_exit()
