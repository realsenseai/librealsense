# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#test:donotrun:!dds
#test:device D400*

from rspy import log, repo, test, config_file
import subprocess, platform, signal, os
import time


def kill_all_dds_adapters():
    """Kill any leftover rs-dds-adapter processes (e.g. from a previous crashed test run)"""
    try:
        if platform.system() == 'Windows':
            subprocess.run( ['taskkill', '/F', '/IM', 'rs-dds-adapter.exe'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL )
        else:
            subprocess.run( ['pkill', '-9', '-f', 'rs-dds-adapter'],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL )
    except Exception:
        pass


adapter_process = None

def _sigterm_handler( signum, frame ):
    """Kill the adapter on SIGTERM (e.g. Jenkins abort) before we exit"""
    kill_all_dds_adapters()
    signal.signal( signal.SIGTERM, signal.SIG_DFL )
    os.kill( os.getpid(), signal.SIGTERM )

signal.signal( signal.SIGTERM, _sigterm_handler )
if platform.system() == 'Windows':
    signal.signal( signal.SIGBREAK, _sigterm_handler )

with test.closure( 'Kill leftover rs-dds-adapter processes' ):
    kill_all_dds_adapters()
    time.sleep( 0.5 )  # let the OS release resources

with test.closure( 'Run rs-dds-adapter', on_fail=test.ABORT ):
    adapter_path = repo.find_built_exe( 'tools/dds/dds-adapter', 'rs-dds-adapter' )
    if test.check( adapter_path ):
        cmd = [adapter_path, '--domain-id', str(config_file.get_domain_from_config_file_or_default())]
        if log.is_debug_on():
            cmd.append( '--debug' )
        adapter_process = subprocess.Popen( cmd,
            stdout=None,
            stderr=subprocess.STDOUT,
            universal_newlines=True )  # don't fail on errors

with test.closure( 'Verify adapter process started', on_fail=test.ABORT ):
    test.check( adapter_process is not None )

from rspy import librs as rs
if log.is_debug_on():
    rs.log_to_console( rs.log_severity.debug )

try:

    with test.closure( 'Initialize librealsense context', on_fail=test.ABORT ):
        context = rs.context( { 'dds': { 'enabled': True, 'domain': config_file.get_domain_from_config_file_or_default(), 'participant': 'client' }} )

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

finally:
    # Always ensure the adapter process is terminated, even if test fails
    with test.closure( 'Stop rs-dds-adapter', on_fail=test.ABORT ):
        try:
            # Try graceful termination first (SIGTERM lets adapter release DDS resources on Linux)
            adapter_process.send_signal( signal.SIGTERM )
            adapter_process.wait( timeout=2 )
            log.d( 'rs-dds-adapter terminated gracefully' )
        except (subprocess.TimeoutExpired, Exception) as e:
            log.e( f'Error terminating rs-dds-adapter: {e}' )
        finally:
            # Safety net: force kill any remaining adapter processes by name
            kill_all_dds_adapters()

test.print_results_and_exit()

