import pytest

# D555 is not in the e2e lab inventory: without --exclude-device this is a MISSING failure;
# with --exclude-device D555 (or D500*) it must become a skip


@pytest.mark.device("D555")
def test_single_absent(test_device):
    pass
