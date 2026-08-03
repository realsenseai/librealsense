# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Guard: no test uses pytest-retry's `flaky(retries=N)` kwarg.

We retry via pytest-rerunfailures, whose flaky marker reads `reruns=` — a stray `retries=`
is silently ignored (the test loses its per-test retry). Scan the whole unit-tests tree so a
new file can't reintroduce the old syntax.
"""

import os
import re

_UNIT_TESTS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# flaky(...) marker carrying a `retries=` kwarg (any spacing)
_LEGACY = re.compile(r"mark\.flaky\([^)]*\bretries\s*=")


def test_no_flaky_retries_kwarg():
    offenders = []
    for root, _dirs, files in os.walk(_UNIT_TESTS):
        for name in files:
            if not name.endswith(".py"):
                continue
            path = os.path.join(root, name)
            if os.path.abspath(path) == os.path.abspath(__file__):
                continue
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            if _LEGACY.search(text):
                offenders.append(os.path.relpath(path, _UNIT_TESTS))
    assert not offenders, (
        "pytest-retry `flaky(retries=N)` found; use pytest-rerunfailures `flaky(reruns=N)`:\n"
        + "\n".join(offenders)
    )
