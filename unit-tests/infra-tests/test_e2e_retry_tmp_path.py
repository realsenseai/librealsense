# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a test that uses the tmp_path fixture must survive a pytest-retry rerun.

The tmp_path fixture teardown reads request.node.stash[tmppath_result_key], which is populated
only by pytest_runtest_makereport. pytest-retry reruns via TestReport.from_item_and_call (bypassing
makereport) and its preliminary between-attempts teardown runs the tmp_path finalizer, deleting the
key. A retried tmp_path test then reaches the real protocol teardown with the key gone and the
finalizer raises `KeyError: <StashKey>` -- recorded as a teardown ERROR even though the test passed
on retry. conftest's pytest_runtest_teardown restores the key before finalizers fire. This locks in
the fix: without it the run ends with a teardown error on a passed test.
"""

from helpers import run_e2e, parse_outcomes


class TestRetryTmpPath:

    def test_tmp_path_fixture_survives_retry(self):
        """Attempt 1 fails, attempt 2 passes. The retried test uses tmp_path, so its teardown
        finalizer must not raise KeyError on the dropped stash key -- the test genuinely PASSES
        with no teardown error."""
        rc, out, *_ = run_e2e("pytest-retry-tmp-path.py", "--retries", "1")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("retried") == 1, f"expected exactly 1 retry: {out}"
        assert outcomes.get("error", 0) == 0, f"tmp_path teardown KeyError leaked:\n{out}"
        assert "KeyError" not in out, out
