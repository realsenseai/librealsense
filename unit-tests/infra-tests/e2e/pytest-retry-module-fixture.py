# Verifies the core mechanic this PR depends on: pytest-retry tears down
# and re-creates module-scoped fixtures between attempts (pytest-retry's
# preliminary teardown trick, retry_plugin.py:207).
#
# Without this guarantee, module preconditions (e.g. setting D585S safety
# service mode in test_device_wrapped) would not be re-applied after the
# device is recycled on retry.
import pytest

pytestmark = [pytest.mark.device("D455")]

_module_setup_count = 0


@pytest.fixture(scope="module")
def counted_module_fixture():
    global _module_setup_count
    _module_setup_count += 1
    yield _module_setup_count


_test_attempt = 0


def test_module_fixture_is_recreated_on_retry(counted_module_fixture):
    """Attempt 1: module-fixture setup ran once → counted_module_fixture == 1; test fails.
    Attempt 2 (retry): pytest-retry's preliminary teardown trick tears down the
    module fixture; setup re-runs → counted_module_fixture == 2; test asserts
    the counter incremented (i.e. fixture was actually re-instantiated)."""
    global _test_attempt
    _test_attempt += 1
    if _test_attempt == 1:
        assert False, "intentional first-attempt failure"
    assert counted_module_fixture == 2, (
        f"module fixture should have been recreated between attempts; "
        f"counted_module_fixture = {counted_module_fixture}"
    )
