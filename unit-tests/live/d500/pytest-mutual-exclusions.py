# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Mutual exclusion between the perception stream and embedded filters.
#
# The decimation and temporal embedded filters cannot run at the same time as the
# perception stream, so the SDK rejects the conflicting combination in either order with
# a user-friendly (wrong-api-call-sequence) error, surfaced as a RuntimeError through
# the Python API. Other ("non-blocking") embedded filters, e.g. improved close range,
# are not restricted and can run alongside perception.
#
# The test adapts to the device at runtime: it skips when the perception stream or a
# given embedded filter is not present, rather than gating on firmware versions.

import pytest
import pyrealsense2 as rs
from pytest_check import check
import logging
log = logging.getLogger(__name__)


pytestmark = [
    pytest.mark.device_each( "D555", "D585" ),
    pytest.mark.context( "nightly" ),
]


# Embedded filters that are mutually exclusive with the perception stream.
BLOCKING_FILTERS = [
    rs.embedded_filter_type.decimation,
    rs.embedded_filter_type.temporal,
]

# Embedded filters that may run alongside the perception stream.
NON_BLOCKING_FILTERS = [
    rs.embedded_filter_type.improved_close_range_depth,
]

ENABLED = rs.option.embedded_filter_enabled


@pytest.fixture
def depth_sensor(test_device):
    dev, _ = test_device
    return dev.first_depth_sensor()


@pytest.fixture
def perception_sensor(test_device):
    dev, _ = test_device
    try:
        return dev.first_perception_sensor()  # throws if the device has no perception sensor
    except RuntimeError:
        pytest.skip( "Device has no perception sensor" )


@pytest.fixture
def perception_profile(perception_sensor):
    profile = next( ( p for p in perception_sensor.get_stream_profiles()
                      if p.stream_type() == rs.stream.object_detection ), None )
    if profile is None:
        pytest.skip( "Perception sensor has no object-detection profile" )
    return profile


def get_embedded_filter_or_skip(depth_sensor, filter_type):
    # get_embedded_filter throws (rather than returning falsy) when the filter is not present.
    try:
        return depth_sensor.get_embedded_filter( filter_type )
    except Exception:
        pytest.skip( f"Embedded filter {filter_type} not present on this device" )


def require_perception_openable(perception_sensor, perception_profile):
    # The rejection tests are only meaningful if the perception stream opens standalone;
    # otherwise we cannot tell our guard apart from an unrelated open failure.
    try:
        perception_sensor.open( perception_profile )
        perception_sensor.close()
    except RuntimeError as e:
        pytest.skip( f"Perception stream not openable on this device: {e}" )


def require_perception_started(perception_sensor, perception_profile):
    # An perception sensor that exists must be startable - a failure here is a real error, not a skip.
    perception_sensor.open( perception_profile )
    try:
        perception_sensor.start( lambda f: None )
    except Exception:
        perception_sensor.close()  # start never took effect; undo the open before propagating
        raise


@pytest.mark.parametrize( "filter_type", BLOCKING_FILTERS )
def test_perception_rejected_when_blocking_filter_enabled(depth_sensor, perception_sensor, perception_profile, filter_type):
    require_perception_openable( perception_sensor, perception_profile )
    embedded_filter = get_embedded_filter_or_skip( depth_sensor, filter_type )

    embedded_filter.set_option( ENABLED, 1.0 )
    try:
        # The guard runs before the device is touched, so opening must fail here.
        with pytest.raises( RuntimeError ):
            perception_sensor.open( perception_profile )
    finally:
        embedded_filter.set_option( ENABLED, 0.0 )

    # With the filter disabled the same open succeeds again - proving the filter was the cause.
    perception_sensor.open( perception_profile )
    perception_sensor.close()


@pytest.mark.parametrize( "filter_type", BLOCKING_FILTERS )
def test_blocking_filter_rejected_when_perception_active(depth_sensor, perception_sensor, perception_profile, filter_type):
    embedded_filter = get_embedded_filter_or_skip( depth_sensor, filter_type )
    require_perception_started( perception_sensor, perception_profile )
    try:
        # Enabling the filter while perception streams is rejected, and the value stays off.
        with pytest.raises( RuntimeError ):
            embedded_filter.set_option( ENABLED, 1.0 )
        check.equal( embedded_filter.get_option( ENABLED ), 0.0 )
    finally:
        perception_sensor.stop()
        perception_sensor.close()

    # Once perception stops, enabling the filter is allowed again.
    embedded_filter.set_option( ENABLED, 1.0 )
    embedded_filter.set_option( ENABLED, 0.0 )


@pytest.mark.parametrize( "filter_type", BLOCKING_FILTERS )
def test_blocking_filter_rejected_when_perception_opened_not_streaming(depth_sensor, perception_sensor,
                                                                      perception_profile, filter_type):
    # Covers the open()->start() window: the perception sensor is opened but not yet streaming. Enabling a blocking
    # filter in that window must still be rejected, otherwise the subsequent start() would leave both active.
    embedded_filter = get_embedded_filter_or_skip( depth_sensor, filter_type )
    try:
        perception_sensor.open( perception_profile )  # opened, not started
    except RuntimeError as e:
        pytest.skip( f"Perception stream not openable on this device: {e}" )
    try:
        with pytest.raises( RuntimeError ):
            embedded_filter.set_option( ENABLED, 1.0 )
        check.equal( embedded_filter.get_option( ENABLED ), 0.0 )
    finally:
        perception_sensor.close()


@pytest.mark.parametrize( "filter_type", NON_BLOCKING_FILTERS )
def test_non_blocking_filter_and_perception_coexist(depth_sensor, perception_sensor, perception_profile, filter_type):
    embedded_filter = get_embedded_filter_or_skip( depth_sensor, filter_type )
    initial = embedded_filter.get_option( ENABLED )

    embedded_filter.set_option( ENABLED, 1.0 )
    try:
        # Perception can start while the non-blocking filter is enabled...
        require_perception_started( perception_sensor, perception_profile )
        try:
            # ...and the non-blocking filter can be toggled while perception streams.
            embedded_filter.set_option( ENABLED, 0.0 )
            embedded_filter.set_option( ENABLED, 1.0 )
        finally:
            perception_sensor.stop()
            perception_sensor.close()
    finally:
        embedded_filter.set_option( ENABLED, initial )
