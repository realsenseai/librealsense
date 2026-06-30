# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Tests for the shared log format constants in rspy/pytest/logging_setup.py.

Verifies that the timestamp prefix is well-formed (HH:MM:SS.mmm), uses the
single-letter level abbreviation, and that msecs is rounded (not truncated).
"""

import logging
import re

from rspy.pytest.logging_setup import _LOG_FORMAT, _LOG_DATEFMT


class TestLogFormat:
    """_LOG_FORMAT / _LOG_DATEFMT produce a HH:MM:SS.mmm -X- <msg> line."""

    def _formatted(self, level, msg, msecs=0):
        rec = logging.LogRecord('t', level, '', 0, msg, (), None)
        rec.msecs = msecs
        return logging.Formatter(_LOG_FORMAT, datefmt=_LOG_DATEFMT).format(rec)

    def test_shape_info(self):
        line = self._formatted(logging.INFO, 'hello')
        assert re.match(r'^\d{2}:\d{2}:\d{2}\.\d{3} -I- hello$', line), repr(line)

    def test_shape_debug(self):
        line = self._formatted(logging.DEBUG, 'x')
        assert re.match(r'^\d{2}:\d{2}:\d{2}\.\d{3} -D- x$', line), repr(line)

    def test_msecs_rounded_not_truncated(self):
        # 999.9 ms must format as 1000 (rounded), not 999 (truncated by %d).
        line = self._formatted(logging.INFO, 'x', msecs=999.9)
        assert line.split(' ', 1)[0].endswith('.1000'), repr(line)

    def test_msecs_zero_padded(self):
        line = self._formatted(logging.INFO, 'x', msecs=7)
        assert re.search(r'\.007 -I-', line), repr(line)
