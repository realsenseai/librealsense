import pytest
pytestmark = [pytest.mark.device("D455")]
def test_first(module_device_setup):
    assert module_device_setup == '111'
def test_second(module_device_setup):
    assert module_device_setup == '111'
