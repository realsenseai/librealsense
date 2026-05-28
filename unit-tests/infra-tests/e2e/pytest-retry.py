# Test module-scoped retry aggregation: three logical tests exercise the three
# outcomes the conftest reconciler must collapse.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fail_then_pass_attempt = 0

def test_always_passes(module_device_setup):
    """Logical PASS — runs twice (module reruns) but reports once."""
    pass

def test_fails_then_passes(module_device_setup):
    """Logical PASS via retry — fails on step 0, passes on step 1 (rescued)."""
    global _fail_then_pass_attempt
    _fail_then_pass_attempt += 1
    if _fail_then_pass_attempt == 1:
        assert False, "intentional first-pass failure"

def test_always_fails(module_device_setup):
    """Logical FAIL — fails every attempt; report keeps one FAILED."""
    assert False, "intentional permanent failure"
