import pytest

def test_no_device():
    pass

@pytest.mark.device_each("D455")
def test_with_device(_test_device_serial):
    pass
