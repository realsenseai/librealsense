# Verify native pytest-retry behaviour with --retries: a flaky test that
# fails attempt 1 and passes attempt 2 should be reported as a single PASS.
# Module-scoped fixtures are torn down + re-created between attempts, which
# is how device recycling and precondition re-apply happen automatically.
#
# Semantics note: native pytest-retry retries the FAILING TEST only, NOT
# the whole module. Module-level globals here (_fail_attempt,
# _always_passes_calls) accumulate across attempts within a single subprocess
# run because pytest-retry does not re-import the module — it only tears
# down + re-creates fixture instances. Each run_e2e() call is a fresh
# subprocess, so counters reset between e2e calls.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fail_attempt = 0
_always_passes_calls = 0


def test_always_passes(module_device_setup):
    """Runs exactly once. Native pytest-retry only retries the failing test,
    not the whole module, so this test is not re-run when test_fails_then_passes
    retries (contrast with the old pytest-repeat-based module-scoped retry)."""
    global _always_passes_calls
    _always_passes_calls += 1


def test_fails_then_passes(module_device_setup):
    """Fail on attempt 1, pass on attempt 2 (after pytest-retry tears down +
    re-creates module fixtures via its preliminary teardown trick).

    Also asserts _always_passes_calls == 1, locking in the "only failing test
    is retried" invariant: if pytest-retry ever re-runs the whole module on
    retry, this assertion would catch it."""
    global _fail_attempt
    _fail_attempt += 1
    if _fail_attempt == 1:
        assert False, "intentional first-attempt failure"
    assert _always_passes_calls == 1, (
        f"native pytest-retry should NOT re-run sibling tests on retry; "
        f"_always_passes_calls = {_always_passes_calls}"
    )
