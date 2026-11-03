# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""E2E: --custom-fw-* options are registered and their values reach the tests."""


def test_values(request):
    assert request.config.getoption('--custom-fw-d400') == 'd400.bin'
    assert request.config.getoption('--custom-fw-d555') == 'd555.bin'
    assert request.config.getoption('--custom-fw-d585') == 'd585.bin'
    assert request.config.getoption('--custom-fw-d585s') == 'd585s.img'


def test_defaults(request):
    assert request.config.getoption('--custom-fw-d400') is None
    assert request.config.getoption('--custom-fw-d555') is None
    assert request.config.getoption('--custom-fw-d585') is None
    assert request.config.getoption('--custom-fw-d585s') is None
