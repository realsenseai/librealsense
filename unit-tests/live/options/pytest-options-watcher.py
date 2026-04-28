# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

# test:device D400*
# test:device each(D555)
# test:donotrun:!nightly

import pyrealsense2 as rs
from rspy import test
from rspy import log
import time

dev, _ = test.find_first_device_or_exit()
depth_sensor = dev.first_depth_sensor()

changed_options = 0

def notification_callback( opt_list ):
    global changed_options
    log.d( f"notification_callback called with {len(opt_list)} options" )
    for opt in opt_list:
        log.d( f"    {opt.id} -> {opt.value}" )
        if not depth_sensor.is_option_read_only( opt.id ): # Ignore accidental temperature changes
            changed_options = changed_options + 1

depth_sensor.on_options_changed( notification_callback )

with test.closure( 'disabling auto exposure' ): # Need to disable or changing gain/exposure might automatically disable it
    depth_sensor.set_option( rs.option.enable_auto_exposure, 0 )
    test.check_equal( depth_sensor.get_option( rs.option.enable_auto_exposure ), 0.0 )
    time.sleep( 1.5 ) # default options-watcher update interval is 1 second

with test.closure( 'set one option' ):
    changed_options = 0
    current_gain = depth_sensor.get_option( rs.option.gain )
    depth_sensor.set_option( rs.option.gain , current_gain + 1 )
    test.check_equal( depth_sensor.get_option( rs.option.gain ), current_gain + 1 )
    time.sleep( 1.5 ) # default options-watcher update interval is 1 second
    test.check_equal( changed_options, 1 )
    changed_options = 0
    
with test.closure( 'set multiple options' ):
    current_gain = depth_sensor.get_option( rs.option.gain )
    depth_sensor.set_option( rs.option.gain , current_gain + 1 )
    test.check_equal( depth_sensor.get_option( rs.option.gain ), current_gain + 1 )
    current_exposure = depth_sensor.get_option( rs.option.exposure )
    depth_sensor.set_option( rs.option.exposure , current_exposure + 1 )
    test.check_equal( depth_sensor.get_option( rs.option.exposure ), current_exposure + 1 )
    time.sleep( 2.5 ) # default options-watcher update interval is 1 second, multiple options might be updated on different intervals
    test.check_equal( changed_options, 2 )
    changed_options = 0

with test.closure( 'no sporadic changes' ):
    changed_options = 0
    time.sleep( 3 )
    test.check_equal( changed_options, 0 )
    
with test.closure( 'cancel subscription' ):
    depth_sensor = dev.first_depth_sensor() # Get new sensor, old sensor subscription is canceled
    changed_options = 0
    current_gain = depth_sensor.get_option( rs.option.gain )
    depth_sensor.set_option( rs.option.gain , current_gain + 1 )
    test.check_equal( depth_sensor.get_option( rs.option.gain ), current_gain + 1 )
    time.sleep( 1.5 ) # default options-watcher update interval is 1 second
    test.check_equal( changed_options, 0 )
    
test.print_results_and_exit()
