# All failures here are rescued by a retry; exit code must be 0 (green).
import pytest

pytestmark = [pytest.mark.device("D455")]

_flake = 0

def test_always_passes(module_device_setup):
    """Always passes — verifies de-duplication when the module re-runs."""
    pass

def test_flaky_passes_on_retry(module_device_setup):
    """Fails on step 0, passes on step 1 → rescued."""
    global _flake
    _flake += 1
    if _flake == 1:
        assert False, "intentional first-pass failure"
