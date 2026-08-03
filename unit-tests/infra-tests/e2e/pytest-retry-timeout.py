# Exercises the pytest-timeout + rerun interaction. Run under
# --reruns=1 --timeout=5.
#
# pytest-timeout arms its per-item timer in `pytest_runtest_protocol`, whose
# yield covers every attempt: pytest-rerunfailures runs all reruns inside that
# single protocol call, so without a reset each rerun shares the original
# --timeout budget with the failed first attempt.
#
# This test consumes ~3s per attempt on a 5s budget. Without the conftest
# reset, attempt 2 has ~2s of budget left after attempt 1 and gets killed
# mid-sleep with a `+++ Timeout +++` banner (os._exit(1) → whole subprocess
# dies). With the reset, each attempt gets a fresh 5s and the test passes on
# attempt 2. Margins chosen for slow CI: 2s of slack per attempt.
import time

_attempt = 0


def test_timeout_resets_between_retry_attempts():
    global _attempt
    _attempt += 1
    time.sleep(3.0)
    if _attempt == 1:
        assert False, "intentional first-attempt failure"
