# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

# Placeholder so that `pytest --tag dds` (≡ `-m dds`) selects at least one item
# and exits 0 instead of rc=5 ("no tests collected"). Until any real test under
# unit-tests/dds/ is migrated from test-*.py to pytest-*.py with @pytest.mark.dds,
# this is the only dds-tagged pytest test.
#
# REMOVE THIS FILE as soon as the first real dds test is migrated.
# See .github/skills/pytest-infra.md (migration step about path-derived markers).

import pytest


@pytest.mark.dds
def test_dds_placeholder():
    pass
