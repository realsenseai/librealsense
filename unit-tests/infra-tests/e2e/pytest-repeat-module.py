# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""E2E fixture: a module with two passing tests. Used to verify --repeat N
repeats the WHOLE module N times (per our --repeat-scope=module default),
not pytest-repeat's default per-function scope.

@device marker so subprocess_isolation's _enable_target_devices_for_group
resolves a target and triggers devices.enable_only -- which the
e2e_conftest mock counts (one per module iteration).
"""

import pytest

pytestmark = [pytest.mark.device("D400*")]


def test_a():
    pass


def test_b():
    pass
