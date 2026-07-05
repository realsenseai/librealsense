# A module-scoped fixture (autouse-dependency chain) that fails its first two setup
# attempts. pytest-retry reruns setup+call unconditionally; on the failed re-setup the
# call phase would raise KeyError('<fixture>'), masking the real error. The conftest guard
# surfaces the real error instead, so the retry sees the true (retryable) exception.
import pytest
_base = 0
_dep = 0

@pytest.fixture(scope="module", autouse=True)
def base_module_fixture():
    global _base; _base += 1
    yield _base

@pytest.fixture(scope="module")
def dependent_module_fixture(base_module_fixture):
    global _dep; _dep += 1
    if _dep <= 2:
        raise RuntimeError("intentional module-fixture setup failure attempt %d" % _dep)
    yield _dep

def test_recovers_without_keyerror(dependent_module_fixture):
    assert dependent_module_fixture >= 3
