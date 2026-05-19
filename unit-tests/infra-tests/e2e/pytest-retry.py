# Test module-scoped retry: the module has two tests, one fails on the first pass.
# --retries 1 should rerun the entire module (both tests) after recycling.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fail_attempt = 0

def test_always_passes(module_device_setup):
    """This test always passes — included to verify the entire module reruns."""
    pass

def test_fails_then_passes(module_device_setup):
    """Fail on first pass (step 0), pass on retry (step 1)."""
    global _fail_attempt
    _fail_attempt += 1
    if _fail_attempt == 1:
        assert False, "intentional first-pass failure"
