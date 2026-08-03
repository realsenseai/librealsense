# Verify rerun behaviour with --reruns: a flaky test that fails attempt 1
# and passes attempt 2 should be reported as a single PASS. The conftest
# recycle hook tears down + re-creates local fixtures between attempts,
# which is how device recycling and precondition re-apply happen.
#
# Semantics note: only the FAILING TEST is rerun, NOT the whole module.
# Module-level globals here (_fail_attempt, _always_passes_calls) accumulate
# across attempts within a single subprocess run because the module is not
# re-imported — only fixture instances are re-created. Each run_e2e() call
# is a fresh subprocess, so counters reset between e2e calls.
import pytest

pytestmark = [pytest.mark.device("D455")]

_fail_attempt = 0
_always_passes_calls = 0


def test_always_passes(module_device_setup):
    """Runs exactly once. Only the failing test is rerun, not the whole
    module, so this test is not re-run when test_fails_then_passes retries."""
    global _always_passes_calls
    _always_passes_calls += 1


def test_fails_then_passes(module_device_setup):
    """Fail on attempt 1, pass on attempt 2 (after the recycle hook tears
    down + re-creates module fixtures between attempts).

    Also asserts _always_passes_calls == 1, locking in the "only failing test
    is retried" invariant: if the rerun ever re-runs the whole module, this
    assertion would catch it."""
    global _fail_attempt
    _fail_attempt += 1
    if _fail_attempt == 1:
        assert False, "intentional first-attempt failure"
    assert _always_passes_calls == 1, (
        f"reruns should NOT re-run sibling tests; "
        f"_always_passes_calls = {_always_passes_calls}"
    )
