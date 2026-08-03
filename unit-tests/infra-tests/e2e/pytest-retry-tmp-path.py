# Exercises the tmp_path fixture + rerun interaction. Run under --reruns 1.
#
# The tmp_path fixture teardown reads request.node.stash[tmppath_result_key], which is
# populated only by pytest_runtest_makereport. A retry engine that bypasses makereport
# (as pytest-retry did) leaves the key missing on a retried test's real teardown and the
# finalizer raises `KeyError: <StashKey>` -- a teardown ERROR on a test that passed on
# retry. pytest-rerunfailures reruns the whole protocol so makereport fires per attempt;
# this scenario keeps that invariant guarded no matter which engine runs the retries.
_attempt = 0


def test_tmp_path_survives_retry(tmp_path):
    global _attempt
    _attempt += 1
    (tmp_path / "artifact.txt").write_text("x")
    assert _attempt >= 2, "intentional first-attempt failure"
