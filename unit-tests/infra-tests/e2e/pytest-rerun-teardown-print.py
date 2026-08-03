# A module fixture whose teardown writes RAW stdout (like rspy's "-D-" hub prints). The
# between-attempts recycle runs fixture teardowns while pytest's capture is suspended, so
# without the conftest stdout guard this marker would leak into the CI console mid-run.
import pytest

pytestmark = [pytest.mark.device("D455")]

_attempt = 0


@pytest.fixture(scope="module")
def noisy_module_fixture():
    yield
    print("RAW-TEARDOWN-LEAK-MARKER")


def test_fails_then_passes(noisy_module_fixture, module_device_setup):
    global _attempt
    _attempt += 1
    assert _attempt >= 2, "intentional first-attempt failure"
