# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a module-scoped fixture that fails setup under pytest-retry must surface the REAL
error, not a masking KeyError.

pytest-retry reruns a test by calling pytest_runtest_setup then pytest_runtest_call
unconditionally (retry_plugin ~L237-238); it ignores whether the retry-setup failed. When it
did, funcargs lack the fixture and pytest_pyfunc_call raises `KeyError: '<fixture>'`, hiding
the real setup error. conftest's pytest_runtest_setup/pytest_runtest_call guard swaps that
KeyError back for the recorded setup exception. Verified: without the guard this test sees
`KeyError` in the output; with it, the real RuntimeError is reported and the retry recovers.
"""

from helpers import run_e2e, parse_outcomes


class TestRetrySetupFail:

    def test_module_fixture_setup_fail_surfaces_real_error(self):
        """Fixture fails setup on attempts 1 & 2 (real RuntimeError) and recovers on attempt 3.
        The retry must never leak pytest-retry's masking KeyError."""
        rc, out, *_ = run_e2e("pytest-retry-module-setup-fail.py", "--retries", "2")
        assert parse_outcomes(out).get("passed") == 1, out
        assert "KeyError" not in out, f"pytest-retry KeyError leaked:\n{out}"
        assert "intentional module-fixture setup failure" in out, out
