# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import logging

import pytest

import hdr_helper

log = logging.getLogger(__name__)

pytestmark = [
    pytest.mark.device_each("D455"),
    pytest.mark.device_each("D457"),
]

MANUAL_HDR_CONFIG = {
    "hdr-preset": {
        "id": "0",
        "iterations": "0",
        "items": [
            {"iterations": "1", "controls": {"depth-gain": "16", "depth-exposure": "1"}},
            {"iterations": "2", "controls": {"depth-gain": "61", "depth-exposure": "10"}},
            {"iterations": "1", "controls": {"depth-gain": "116", "depth-exposure": "100"}},
            {"iterations": "3", "controls": {"depth-gain": "161", "depth-exposure": "1000"}},
            {"iterations": "1", "controls": {"depth-gain": "22", "depth-exposure": "10000"}},
            {"iterations": "2", "controls": {"depth-gain": "222", "depth-exposure": "4444"}},
        ]
    }
}

AUTO_HDR_CONFIG = {
    "hdr-preset": {
        "id": "0",
        "iterations": "0",
        "items": [
            {"iterations": "1", "controls": {"depth-ae": "1"}},
            {"iterations": "2", "controls": {"depth-ae-exp": "2000", "depth-ae-gain": "30"}},
            {"iterations": "2", "controls": {"depth-ae-exp": "-2000", "depth-ae-gain": "20"}},
            {"iterations": "3", "controls": {"depth-ae-exp": "2500", "depth-ae-gain": "10"}},
            {"iterations": "3", "controls": {"depth-ae-exp": "-2500", "depth-ae-gain": "40"}},
        ]
    }
}


def test_hdr_preset_manual(test_device):
    hdr_helper.setup_for_device(test_device)
    hdr_helper.load_and_perform_test(MANUAL_HDR_CONFIG, "Auto HDR - Sanity - Manual mode")


def test_hdr_preset_auto(test_device):
    hdr_helper.setup_for_device(test_device)
    hdr_helper.load_and_perform_test(AUTO_HDR_CONFIG, "Auto HDR - Sanity - Auto mode")
