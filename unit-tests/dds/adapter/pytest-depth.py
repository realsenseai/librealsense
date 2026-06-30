# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import pytest
import re
import logging
from rspy import repo, config_file
import subprocess, platform, signal, os
import time
from pytest_check import check

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.dds,
    pytest.mark.device( "D400*" ),
]

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

def test_depth():
    global adapter_process

    # Kill leftover rs-dds-adapter processes
    kill_all_dds_adapters()
    time.sleep( 0.5 )  # let the OS release resources

    # Run rs-dds-adapter. Do NOT capture stdout via a PIPE: the adapter writes
    # many log lines as it runs and we don't actively drain them. With PIPE the
    # buffer fills up and the adapter blocks on write, freezing mid-operation
    # before it can publish its DDS device — wait_for_devices below then times
    # out. stdout=None inherits the parent's stdout (which pytest captures), so
    # the adapter never blocks.
    adapter_path = repo.find_built_exe( 'tools/dds/dds-adapter', 'rs-dds-adapter' )
    if check.is_true( adapter_path ):
        cmd = [adapter_path, '--domain-id', str(config_file.get_domain_from_config_file_or_default())]
        if log.isEnabledFor( logging.DEBUG ):
            cmd.append( '--debug' )
        adapter_process = subprocess.Popen( cmd,
            stdout=None,
            stderr=subprocess.STDOUT,
            universal_newlines=True )

    # Verify adapter process started
    assert adapter_process is not None

    from rspy import librs as rs
    if log.isEnabledFor( logging.DEBUG ):
        rs.log_to_console( rs.log_severity.debug )

    try:

        # Initialize librealsense context. No explicit participant name →
        # defaults to executable_name and reuses the harness's discovery
        # participant in the configured domain (librs enforces one-name-per-
        # (process, domain); an explicit different name here would collide).
        context = rs.context( { 'dds': { 'enabled': True, 'domain': config_file.get_domain_from_config_file_or_default() }} )

        # Wait for a device
        # Note: takes time for a device to enumerate, and more to get it discovered
        dev = rs.wait_for_devices( context, rs.only_sw_devices, n=1., timeout=8 )

        # Get sensor
        sensor = dev.first_depth_sensor()
        assert sensor

        # Find profile
        for p in sensor.profiles:
            log.debug( p )
        profile = next( p for p in sensor.profiles
                        if p.fps() == 30
                        and p.stream_type() == rs.stream.depth )
        assert profile

        n_frames = 0
        start_time = None
        def frame_callback( frame ):
            nonlocal n_frames, start_time
            if n_frames == 0:
                start_time = time.perf_counter()
            n_frames += 1

        # Stream
        sensor.open( [profile] )
        sensor.start( frame_callback )

        # Let it stream
        time.sleep( 3 )
        end_time = time.perf_counter()
        if check.is_true( n_frames > 0 ):
            log.info( f'start_time: {start_time}' )
            log.info( f'end_time: {end_time}' )
            log.info( f'n_frames: {n_frames}' )
            check.between( n_frames / (end_time-start_time), 25, 31 )

        # Open the same profile while streaming!
        with pytest.raises( RuntimeError, match=re.escape( 'open(...) failed. Software device is streaming!' ) ):
            sensor.open( [profile] )

        # Stop streaming
        sensor.stop()
        sensor.close()

        del profile
        del sensor
        del dev
        del context

    finally:
        # Always ensure the adapter process is terminated, even if test fails
        # Stop rs-dds-adapter
        try:
            # Try graceful termination first (SIGTERM lets adapter release DDS resources on Linux)
            adapter_process.send_signal( signal.SIGTERM )
            adapter_process.wait( timeout=2 )
            log.debug( 'rs-dds-adapter terminated gracefully' )
        except (subprocess.TimeoutExpired, Exception) as e:
            log.error( f'Error terminating rs-dds-adapter: {e}' )
        finally:
            # Safety net: force kill any remaining adapter processes by name
            kill_all_dds_adapters()
