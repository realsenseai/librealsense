# Regression for Jenkins win #113344 — setup-phase ERROR in pass 0 must still
# trigger the retry pass.  The bug was that pytest_runtest_makereport gated
# failure-tracking on report.when == "call", so fixture setup failures (which
# Jenkins reports as ERROR, not FAILED) were silently treated as "pass had no
# failures" and the skip-if-clean retry optimisation skipped the retry.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fixture_attempt = 0


@pytest.fixture
def setup_fails_first_pass():
    """Raise on first invocation, succeed on retry.  Function-scoped so it re-runs
    per test instance, including the retry pass."""
    global _fixture_attempt
    _fixture_attempt += 1
    if _fixture_attempt == 1:
        raise RuntimeError("intentional first-pass setup failure")
    return _fixture_attempt


def test_setup_fails_then_passes(setup_fails_first_pass):
    """Pass 0: fixture raises → test reported as ERROR.
    Pass 1 (retry):  fixture returns → test PASSED.
    With the fix, pass 0's ERROR is recorded as a module-level failure, so the
    retry is not skipped by the skip-if-clean optimisation."""
    assert setup_fails_first_pass >= 2
