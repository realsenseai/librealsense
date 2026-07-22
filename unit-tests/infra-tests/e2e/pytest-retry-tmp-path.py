# Exercises the tmp_path fixture + pytest-retry interaction. Run under --retries=1.
#
# The tmp_path fixture teardown reads request.node.stash[tmppath_result_key] (populated only by
# pytest_runtest_makereport) then deletes it. pytest-retry reruns a test via
# TestReport.from_item_and_call (bypassing makereport) and its preliminary between-attempts
# teardown runs the tmp_path finalizer, deleting the key. A retried test that uses tmp_path then
# reaches the real protocol teardown with the key gone, and the finalizer raises
# `KeyError: <StashKey>` -- a teardown ERROR on a test that passed on retry. The conftest
# pytest_runtest_teardown restores the key before finalizers fire; this test locks that in.
_attempt = 0


def test_tmp_path_survives_retry(tmp_path):
    global _attempt
    _attempt += 1
    (tmp_path / "artifact.txt").write_text("x")
    assert _attempt >= 2, "intentional first-attempt failure"
