# Driver for test_e2e_log_failures: a device test whose call phase raises. Used to
# verify the exception is recorded in the per-test log even under pytest-retry (which
# reopens the module log in 'w' mode and bypasses pytest_runtest_makereport).
import pytest

pytestmark = [pytest.mark.device("D455")]


def test_raises_runtime_error(module_device_setup):
    raise RuntimeError("xioctl(VIDIOC_G_EXT_CTRLS) failed, errno=22")
