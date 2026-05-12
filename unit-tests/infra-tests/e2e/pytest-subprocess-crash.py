# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""E2E fixture: first test segfaults, sibling shares the same (file, no-device-id)
group. test_e2e_subprocess_isolation runs this and asserts both nodeids fail
(crash + did-not-run) - that's the evidence isolation worked.
"""

import faulthandler


def test_crash_via_sigsegv():
    faulthandler._sigsegv()


def test_crash_sibling():
    assert True
