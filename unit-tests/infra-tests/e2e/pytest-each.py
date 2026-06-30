import pytest

@pytest.mark.device_each("D400*")
def test_d400(_test_device_serial):
    assert _test_device_serial in ('111', '222', '777')

@pytest.mark.device_each("D400*")
@pytest.mark.device_exclude("D401")
def test_d400_exclude(_test_device_serial):
    assert _test_device_serial != '777'

@pytest.mark.device_each("D999")
def test_d999_no_match():
    pass

@pytest.mark.device_each("D455")
@pytest.mark.device_each("D515")
def test_union(_test_device_serial):
    assert _test_device_serial in ('111', '555')

@pytest.mark.device_each("D455")
def test_ids(_test_device_serial):
    pass

# --- device() + device_each() coexistence ---

@pytest.mark.device("D455")
@pytest.mark.device_each("D515")
def test_device_and_each(module_device_setup):
    """device("D455") produces one mandatory instance; device_each("D515") adds one more."""
    assert module_device_setup in ('111', '555')

@pytest.mark.device("D999")
@pytest.mark.device_each("D455")
def test_device_missing_and_each(module_device_setup):
    """device("D999") not found → fail; device_each("D455") → pass."""
    # This test runs twice: once for D455 (passes) and once for __MISSING__:D999 (fails)
    assert module_device_setup == '111'
