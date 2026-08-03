# A module-scoped fixture (autouse-dependency chain) that fails its first two setup
# attempts, then recovers. Setup-phase failures must be retried natively and the real
# error recorded in the per-test log — never a masking KeyError('<fixture>') (which the
# old pytest-retry engine produced by rerunning the call phase after a failed re-setup).
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
