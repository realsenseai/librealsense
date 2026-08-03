# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
E2E: a test that uses the tmp_path fixture must survive a rerun with no teardown error.

The tmp_path fixture teardown reads request.node.stash[tmppath_result_key], populated only by
pytest_runtest_makereport. A retry engine that bypasses makereport (as pytest-retry did) leaves
the key missing at the retried test's real teardown and the finalizer raises
`KeyError: <StashKey>` -- recorded as a teardown ERROR even though the test passed on retry
(seen as Jenkins libci junit "failed on teardown with KeyError: <StashKey>").
pytest-rerunfailures reruns the whole protocol, so makereport fires on every attempt and the
key stays balanced. This guard keeps the whole class from coming back with any engine.
"""

from helpers import run_e2e, parse_outcomes


class TestRetryTmpPath:

    def test_tmp_path_fixture_survives_retry(self):
        """Attempt 1 fails, attempt 2 passes. The retried test uses tmp_path, so its teardown
        finalizer must not raise KeyError on a missing stash key -- the test genuinely PASSES
        with no teardown error."""
        rc, out, *_ = run_e2e("pytest-retry-tmp-path.py", "--reruns", "1")
        assert rc == 0, out
        outcomes = parse_outcomes(out)
        assert outcomes.get("passed") == 1, out
        assert outcomes.get("rerun") == 1, f"expected exactly 1 rerun: {out}"
        assert outcomes.get("error", 0) == 0, f"tmp_path teardown KeyError leaked:\n{out}"
        assert "KeyError" not in out, out
