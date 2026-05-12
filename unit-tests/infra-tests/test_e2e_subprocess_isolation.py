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

    def test_retry_module_scope_under_isolation(self):
        # Retry semantics are owned by conftest (--retries -> --count=N+1 +
        # _module_retry_mode + skip-if-clean-pass hook). Subprocess isolation
        # groups by (file, device) and strips pytest-repeat's "-step-total"
        # suffix from the group key, so ALL repeat passes for one (file, device)
        # share a single child subprocess. That preserves the in-process
        # _module_pass_had_failure dict across passes -- pass 1 sees pass 0's
        # failures and the skip-if-clean hook fires correctly.
        #
        # Fixture (upstream pytest-retry.py): test_always_passes always passes,
        # test_fails_then_passes fails on pass 0 and passes on pass 1.
        # --retries 1 -> count=2, module-scoped.
        # Expected: pass 0 runs both (1P 1F), pass 1 runs both (both PASS
        # because dict shows pass 0 had a failure, so the skip hook lets them
        # through). Total: 3 passed, 1 failed.
        rc, out, tracking = run_e2e(
            "pytest-retry.py",
            "--retries", "1",
            with_subprocess_isolation=True,
        )
        assert_outcomes(out, passed=3, failed=1)
        # Exactly one enable_only call: the parent recycles the hub once when
        # dispatching the (file, device) group. Both repeat passes run inside
        # that single child subprocess (no per-pass parent recycle, by design).
        ec = tracking.get("enable_only_calls", [])
        assert len(ec) == 1, (
            f"expected exactly 1 enable_only call (single subprocess group for "
            f"all repeat passes); got {len(ec)}: {ec}"
        )


class TestSubprocessRepeat:

    def test_repeat_runs_all_passes_in_one_subprocess(self):
        # Fixture: a module with two passing tests. With --repeat 2,
        # pytest-repeat parametrizes each test with step 1 and 2 (--repeat
        # implies --repeat-scope=module via conftest).
        # Collection order: test_a[1-2], test_b[1-2], test_a[2-2], test_b[2-2]
        # Subprocess isolation strips the "-1-2" / "-2-2" suffix from the group
        # key so all four items live in ONE group -> ONE child subprocess.
        rc, out, tracking = run_e2e(
            "pytest-repeat-module.py",
            "--repeat", "2",
            with_subprocess_isolation=True,
        )
        # 4 total: 2 tests * 2 iterations
        assert_outcomes(out, passed=4)
        # Exactly one enable_only call: the parent recycles the hub once for
        # the (file, device) group. Both repeat passes run inside that single
        # child subprocess.
        ec = tracking.get("enable_only_calls", [])
        assert len(ec) == 1, (
            f"expected exactly 1 enable_only call (single subprocess group for "
            f"both repeat passes); got {len(ec)}: {ec}"
        )
