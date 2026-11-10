# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
A device that throws on creation must not hide the other devices from the registry.
"""

from unittest.mock import MagicMock

import rspy.devices as dev


def test_create_devices_skips_failing_device():
    bad = RuntimeError("option 'Filter Type' not in this filter")
    dl = MagicMock()
    dl.__len__.return_value = 3
    dl.__getitem__.side_effect = ['D455', bad, 'D435']
    assert dev._create_devices( dl ) == ['D455', 'D435']
