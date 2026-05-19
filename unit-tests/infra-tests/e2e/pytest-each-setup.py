import pytest

@pytest.mark.device_each("D400*")
def test_d400(_test_device_serial, module_device_setup):
    assert module_device_setup == _test_device_serial

@pytest.mark.device_each("D999")
def test_d999_no_match(module_device_setup):
    pass

@pytest.mark.device_each("D455")
def test_d455_excluded(module_device_setup):
    pass
