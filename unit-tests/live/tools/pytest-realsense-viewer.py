# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 Intel Corporation. All Rights Reserved.

import pytest
import pyrealsense2 as rs
from rspy.pytest.device_helpers import parse_fw_version
from viewer_helper import run_viewer_tests

# the tests in the realsense-viewer-tests executable are defined along the tester file itself at
# tools/realsense-viewer/tests/. Using the --auto flag allows us to run all tests one by one.
# If we want to run a specific test / test group, we can use the -r flag

pytestmark = [
    pytest.mark.device_each("D400*", "D500*"),
    pytest.mark.context("nightly"),
    pytest.mark.context("gui"),
    # Override the 200s default from conftest.py: mem_leak_depth_start_stop is a ~2.5 min
    # soak (10 * (10s + 5s) real) and the rest of the suite adds ~90s on real CI hardware
    # (LibCI 17981 D555 Win 245s, D585 Proto Linux 234s). 400s gives ~25% headroom over that.
    pytest.mark.timeout(400),
    # Opt out of retries: this launches the realsense-viewer GUI and is long-running /
    # not reliably retryable, so a rerun just doubles CI time without adding signal.
    pytest.mark.flaky(reruns=0),
]


# Fail on D500; to be restored once fixed
D500_SKIPPED_TESTS = ['select_resolution_and_stream', 'stream_all_sensors',
                      'stream_each_sensor_individually', 'mem_leak_depth_start_stop']
_D500_EXTRA = ['restore_factory', 'advanced_write_back', 'set_exposure', 'global_search', 'post_processing_value_persists']
# Per device: (name, connection type or None for any) -> more tests to skip
D500_DEVICE_SKIPPED_TESTS = {
    ('D555', None): ['global_search'],
    ('D585S', None): _D500_EXTRA,
    ('D585 Prototype', 'USB'): ['hardware_reset'] + _D500_EXTRA,
}
# Older D585 FW makes the viewer pop up an ICR schema error that blocks the restore-factory test (see #16064)
D585_RESTORE_FACTORY_MIN_FW = '7.62'


def test_realsense_viewer_gui(module_device_setup, test_device):
    dev, _ = test_device
    if dev.get_info( rs.camera_info.product_line ) == 'D500':
        name, conn = dev.get_info( rs.camera_info.name ), dev.get_info( rs.camera_info.connection_type )
        skipped = list( D500_SKIPPED_TESTS )
        for ( n, c ), tests in D500_DEVICE_SKIPPED_TESTS.items():
            if n in name and c in ( None, conn ):
                skipped += tests
        if 'D585' in name and 'D585S' not in name:
            fw = dev.get_info( rs.camera_info.firmware_version )
            if parse_fw_version( fw ) < parse_fw_version( D585_RESTORE_FACTORY_MIN_FW ):
                skipped.append( 'restore_factory' )
        run_viewer_tests( ','.join( '-' + t for t in skipped ) )  # '-' excludes a test
    else:
        run_viewer_tests()
