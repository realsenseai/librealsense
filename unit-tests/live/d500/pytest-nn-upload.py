# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# NN model upload (nn_loader): reported only on the D585 over USB, the checks done before any transfer, and the device
# rejecting a bad package.
# Uploads of a real model package (then detecting with it) need a valid model package and are not covered yet.

import pytest
import pyrealsense2 as rs
import pyrsutils as rsutils
import time
from rspy.pytest.device_helpers import require_min_fw_version
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555", "D535", "D585" ),
    pytest.mark.device_exclude( "D585S" ),       # the safety SKU has no perception sensor
    pytest.mark.device_type_exclude( "GMSL" ),   # uploads go over the USB HW monitor only
    pytest.mark.device_type_exclude( "DDS" ),
]

NN_UPLOAD_MIN_FW = rsutils.version( 7, 60, 0, 0 )   # placeholder, as in d500-nn-loader.cpp
NN_UPLOAD_PIDS = { "0C04", "0C05", "0C06", "0C07", "0C08" }   # D585 only, as nn_upload_pids in d500-private.h
MAX_FILE_SIZE = ( 1 << 16 ) * 1000 - 16
GARBAGE = bytes( range( 256 ) ) * 16                 # not a package: no package magic


def is_locked( dev ):
    return dev.get_info( rs.camera_info.camera_locked ) == "YES"


def is_d585( dev ):
    return dev.get_info( rs.camera_info.product_id ).upper() in NN_UPLOAD_PIDS


@pytest.fixture
def loader( test_device ):
    dev, _ = test_device
    if not is_d585( dev ):
        pytest.skip( "NN upload is D585 only" )
    require_min_fw_version( dev, NN_UPLOAD_MIN_FW, "NN upload" )
    assert dev.is_nn_loader()
    return dev.as_nn_loader()


@pytest.fixture
def unlocked_loader( loader ):
    if is_locked( loader ):
        pytest.skip( "Camera is locked" )
    return loader


@pytest.fixture
def perception( test_device ):
    dev, _ = test_device
    try:
        sensor = dev.first_perception_sensor()   # throws if the device has no perception sensor
    except RuntimeError:
        pytest.skip( "Device has no perception sensor" )
    profile = next( ( p for p in sensor.get_stream_profiles() if p.stream_type() == rs.stream.object_detection ), None )
    if profile is None:
        pytest.skip( "Perception sensor has no object-detection profile" )
    yield sensor, profile
    for close in ( sensor.stop, sensor.close ):   # never hand the next test an open sensor
        try:
            close()
        except RuntimeError:
            pass                                 # not started/open, the normal path


def frames_for( sensor, profile, seconds, action=None ):
    """Streams detections for the given time, running the action in the middle; returns the frame count."""
    frames = []
    sensor.open( profile )
    sensor.start( frames.append )
    try:
        time.sleep( seconds / 2 )
        if action:
            action()
        time.sleep( seconds / 2 )
    finally:
        sensor.stop()
        sensor.close()
    return len( frames )


def test_reported_only_on_d585( test_device ):
    dev, _ = test_device
    fw = rsutils.version( dev.get_info( rs.camera_info.firmware_version ) )
    assert dev.is_nn_loader() == ( is_d585( dev ) and fw >= NN_UPLOAD_MIN_FW )


def test_is_upload_supported( loader ):
    supported, reason = loader.is_upload_supported()
    if is_locked( loader ):
        assert not supported and "locked" in reason
    else:
        assert supported and reason == ""


def test_unsupported_options( loader ):
    for kwargs, message in ( ( { 'target': rs.nn_target.ddr }, "only the flash target" ),
                             ( { 'slot': 1 }, "only slot 0" ),
                             ( { 'activate': False }, "without activating" ) ):
        with pytest.raises( RuntimeError, match=message ):
            loader.nn_upload( GARBAGE, **kwargs )


def test_size_limits( loader ):
    start = time.monotonic()
    for data, message in ( ( b"", "empty" ), ( bytes( MAX_FILE_SIZE + 1 ), "at most" ) ):
        with pytest.raises( RuntimeError, match=message ):
            loader.nn_upload( data )
    assert time.monotonic() - start < 1   # refused before any transfer


def test_refused_while_streaming( unlocked_loader, perception ):
    sensor, profile = perception
    def upload():
        with pytest.raises( RuntimeError, match="stop the Perception sensor" ):
            unlocked_loader.nn_upload( GARBAGE )
    assert frames_for( sensor, profile, 2, upload ) > 0


def test_refused_when_locked( loader ):
    if not is_locked( loader ):
        pytest.skip( "Camera is not locked" )
    with pytest.raises( RuntimeError, match="locked" ):
        loader.nn_upload( GARBAGE )


def test_bad_package_rejected( unlocked_loader ):
    progress = []
    with pytest.raises( RuntimeError, match="NN upload failed" ):
        unlocked_loader.nn_upload( GARBAGE, callback=progress.append )
    log.debug( f"progress: {progress}" )
    assert all( 0 <= p < 1 for p in progress )   # never reports completion
    assert progress == sorted( progress )

    # The device is ready for the next upload. Whether the stored model survives depends on where the device rejects the
    # package (before or during the commit), so detections are not checked here.
    assert unlocked_loader.is_upload_supported() == ( True, "" )
