# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:device D400*

from rspy import log, repo, test, config
import time


with test.closure( 'Run rs-dds-adapter', on_fail=test.ABORT ):
    adapter_path = repo.find_built_exe( 'tools/dds/dds-adapter', 'rs-dds-adapter' )
    if test.check( adapter_path ):
        import subprocess, signal
        cmd = [adapter_path, '--domain-id', str(config.get_domain_from_config_file())]
        if log.is_debug_on():
            cmd.append( '--debug' )
        adapter_process = subprocess.Popen( cmd,
            stdout=None,
            stderr=subprocess.STDOUT,
            universal_newlines=True )  # don't fail on errors

from rspy import librs as rs
if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )

with test.closure( 'Initialize librealsense context', on_fail=test.ABORT ):
    context = rs.context( { 'dds': { 'enabled': True, 'domain': config.get_domain_from_config_file(), 'participant': 'client' }} )

with test.closure( 'Wait for a device', on_fail=test.ABORT ):
    # Note: takes time for a device to enumerate, and more to get it discovered
    dev = rs.wait_for_devices( context, rs.only_sw_devices, n=1., timeout=8 )

with test.closure( 'Get sensor', on_fail=test.ABORT ):
    sensor = dev.first_depth_sensor()
    test.check( sensor )

with test.closure( 'Find profile', on_fail=test.ABORT ):
    for p in sensor.profiles:
        log.d( p )
    profile = next( p for p in sensor.profiles
                    if p.fps() == 30
                    and p.stream_type() == rs.stream.depth )
    test.check( profile )

n_frames = 0
start_time = None
def frame_callback( frame ):
    global n_frames, start_time
    if n_frames == 0:
        start_time = time.perf_counter()
    n_frames += 1

with test.closure( f'Stream {profile}', on_fail=test.ABORT ):
    sensor.open( [profile] )
    sensor.start( frame_callback )

with test.closure( 'Let it stream' ):
    time.sleep( 3 )
    end_time = time.perf_counter()
    if test.check( n_frames > 0 ):
        test.info( 'start_time', start_time )
        test.info( 'end_time', end_time )
        test.info( 'n_frames', n_frames )
        test.check_between( n_frames / (end_time-start_time), 25, 31 )

with test.closure( 'Open the same profile while streaming!' ):
    test.check_throws( lambda:
        sensor.open( [profile] ),
        RuntimeError, 'open(...) failed. Software device is streaming!' )

with test.closure( 'Stop streaming' ):
    sensor.stop()
    sensor.close()

del profile
del sensor
del dev
del context

with test.closure( 'Stop rs-dds-adapter' ):
    adapter_process.send_signal( signal.SIGTERM )
    adapter_process.wait( timeout=2 )

test.print_results()

