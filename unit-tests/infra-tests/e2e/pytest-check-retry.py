# Exercises the pytest-check + pytest-retry interaction. Run under --retries.
#
# pytest-check defers soft-check failures to makereport, but pytest-retry reruns
# via pytest_runtest_call + TestReport.from_item_and_call (no makereport), so a
# failed soft check used to (a) look "passed" to the retry decision and (b) leak
# onto the teardown report. conftest's pytest_runtest_call hook surfaces the
# failure in the call phase instead. These tests lock that in:
#   - persistent failure  -> stays FAILED (attributed to call, no teardown error)
#   - flaky failure        -> genuinely PASSES on retry
from pytest_check import check

# Module globals accumulate across attempts in a single subprocess: pytest-retry
# tears down/re-creates fixtures but does not re-import the module.
_flaky_attempt = 0


def test_persistent_soft_check():
    check.equal(1, 2, "always fails")


def test_flaky_soft_check():
    global _flaky_attempt
    _flaky_attempt += 1
    check.equal(_flaky_attempt, 2, "fails on attempt 1, passes on attempt 2")
