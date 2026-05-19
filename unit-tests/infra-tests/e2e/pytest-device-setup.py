import pytest

@pytest.mark.device("D455")
def test_d455(module_device_setup):
    assert module_device_setup == '111'

@pytest.mark.device("D999")
def test_d999_no_match(module_device_setup):
    pass

@pytest.mark.device("D455")
@pytest.mark.device_exclude("D455")
def test_d455_excluded(module_device_setup):
    pass

def test_no_markers(module_device_setup):
    assert module_device_setup is None
