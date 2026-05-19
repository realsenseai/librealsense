import pytest

pytestmark = [pytest.mark.device_each("D400*")]

def test_include(_test_device_serial):
    assert _test_device_serial == '111'

def test_exclude(_test_device_serial):
    assert _test_device_serial != '111'

def test_multi_include(_test_device_serial):
    assert _test_device_serial in ('111', '222')

def test_multi_exclude(_test_device_serial):
    assert _test_device_serial == '777'

def test_combined(_test_device_serial):
    assert _test_device_serial == '111'
