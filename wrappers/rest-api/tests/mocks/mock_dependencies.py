# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

from unittest.mock import MagicMock
import pytest

import app.api.dependencies as dependencies
from app.services.rs_manager import RealSenseManager

# Mock the dependencies
mock_oauth2_scheme = MagicMock()


# Pytest fixture to patch dependencies in your app
@pytest.fixture(autouse=True)
def patch_dependencies(monkeypatch):
    """
    Patch the app's dependencies with our mock versions.
    This fixture will automatically be used in all tests.
    """
    # Create mock instances
    rs_manager = RealSenseManager()

    monkeypatch.setattr(dependencies, "get_realsense_manager", lambda: rs_manager)
    monkeypatch.setattr(dependencies, "oauth2_scheme", mock_oauth2_scheme)

    # Also patch the global variables in the original module
    monkeypatch.setattr(dependencies, "_realsense_manager", rs_manager)

    # Return the mocks for use in tests
    return {
        "rs_manager": rs_manager,
        "oauth2_scheme": mock_oauth2_scheme,
    }