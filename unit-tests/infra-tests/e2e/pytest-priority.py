# Priority ordering test: filter_and_sort_items sorts tests by @priority value (lower runs first).
# Tests without @priority get the default value of 500.
#
# Expected execution order:
#   1. test_first   (priority=100)
#   2. test_middle  (priority=500)
#   3. test_verify  (priority=500, default — stable sort keeps it after test_middle)
#   4. test_last    (priority=900)
#
# test_verify runs at default priority 500, so it executes after test_first (100)
# and test_middle (500, defined earlier) but before test_last (900). It checks that
# the first two tests already ran in the correct order.

import pytest
execution_order = []

@pytest.mark.priority(900)
def test_last():
    execution_order.append('last')

@pytest.mark.priority(100)
def test_first():
    execution_order.append('first')

@pytest.mark.priority(500)
def test_middle():
    execution_order.append('middle')

def test_verify_order():
    assert execution_order == ['first', 'middle']
