# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""E2E: subprocess_isolation crash containment, retry, and repeat semantics."""

from helpers import run_e2e, assert_outcomes


class TestSubprocessCrashContainment:

    def test_sigsegv_contained_to_group(self):
        # Both tests share (fspath, no-device-id) -> one group -> one child.
        # The first test crashes the child; subprocess isolation must report
        # BOTH tests as failed (one crashed, one synthesized "did not run")
        # and the parent must finish the run cleanly.
        #
        # Without subprocess isolation, the SIGSEGV would have killed the
        # whole pytest session and we'd see at most one FAILED line. The
        # `failed=2` outcome IS the evidence isolation worked. (Conftest sets
        # tbstyle=no so longrepr text doesn't appear in stdout -- we don't
        # assert on it here.)
        rc, out, *_ = run_e2e("pytest-subprocess-crash.py", with_subprocess_isolation=True)
        assert_outcomes(out, failed=2)
        assert "test_crash_via_sigsegv" in out
        assert "test_crash_sibling" in out


class TestSubprocessRetry:

    def test_retry_reruns_full_module(self):
        # Fixture: test_a fails on the first attempt, passes on the second
        # (using a marker file). test_b always passes.
        # With --retries 1, the parent must:
        #   - re-spawn the WHOLE module (both tests), not just the failed one
        #   - so test_a's first attempt FAILS, then the retry re-runs both
        #     test_a (now PASS, marker exists) and test_b (PASS)
        # Final outcome: 2 passed (last-attempt reports win).
        rc, out, tracking = run_e2e(
            "pytest-retry-once.py",
            "--retries", "1",
            with_subprocess_isolation=True,
        )
        assert_outcomes(out, passed=2)
        # The "retrying full module" announce line must appear in the parent
        # output -- proves my plugin's parent-side retry actually fired.
        assert "retrying full module" in out, (
            f"expected 'retrying full module' in parent stdout; got:\n{out}"
        )
        # Two enable_only calls (initial + retry) prove the hub recycled
        # between attempts.
        ec = tracking.get("enable_only_calls", [])
        assert len(ec) >= 2, f"expected >= 2 enable_only calls; got {ec}"


class TestSubprocessRepeat:

    def test_repeat_reruns_full_module(self):
        # Fixture: a module with two trivially-passing tests. With --repeat 2,
        # pytest-repeat must parametrize each test with 2 iterations AND
        # group iterations by module (per our --repeat-scope=module default,
        # set in conftest.pytest_configure).
        # Collection order should be: test_a[1-2], test_b[1-2], test_a[2-2], test_b[2-2]
        # That gives my plugin two groups (one per iteration), each containing
        # both test functions.
        rc, out, tracking = run_e2e(
            "pytest-repeat-module.py",
            "--repeat", "2",
            with_subprocess_isolation=True,
        )
        # 4 total: 2 tests * 2 iterations
        assert_outcomes(out, passed=4)
        # Two enable_only calls (one per module iteration), not four (one per
        # function-iteration). This is the proof that --repeat-scope=module
        # is active.
        ec = tracking.get("enable_only_calls", [])
        assert len(ec) == 2, (
            f"expected exactly 2 enable_only calls (one per module iteration); "
            f"got {len(ec)}: {ec}"
        )
