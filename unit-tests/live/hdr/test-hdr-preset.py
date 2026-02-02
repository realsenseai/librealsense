# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#test:device:jetson D457
#test:device:!jetson D455

import pyrealsense2 as rs
from rspy import test, log
from hdr_helper import load_and_perform_test

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

load_and_perform_test(MANUAL_HDR_CONFIG, "Auto HDR - Sanity - Manual mode")

load_and_perform_test(AUTO_HDR_CONFIG, "Auto HDR - Sanity - Auto mode")

test.print_results_and_exit()
