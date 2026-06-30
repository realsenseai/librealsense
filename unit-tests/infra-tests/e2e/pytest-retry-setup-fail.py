# Regression for Jenkins win #113344: a setup-phase failure must still trigger
# a retry. Native pytest-retry skips setup failures by default; conftest.py
# patches its should_handle_retry to also retry setup failures.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fixture_attempt = 0


@pytest.fixture
def setup_fails_first_attempt():
    """Raise on first invocation, succeed on retry."""
    global _fixture_attempt
    _fixture_attempt += 1
    if _fixture_attempt == 1:
        raise RuntimeError("intentional first-attempt setup failure")
    return _fixture_attempt


def test_setup_fails_then_passes(setup_fails_first_attempt):
    """Attempt 1: fixture raises → reported as ERROR (without our patch, this would
    be the final outcome). Attempt 2 (retry): fixture returns → test passes."""
    assert setup_fails_first_attempt >= 2
