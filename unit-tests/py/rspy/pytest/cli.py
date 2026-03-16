# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Legacy CLI flag translation — intercept flags that clash with pytest built-ins."""

import sys


def _consume_flag_with_arg(flags, pytest_equiv):
    """Consume a flag+argument from sys.argv, translate to pytest equivalent."""
    for flag in flags:
        if flag not in sys.argv:
            continue
        idx = sys.argv.index(flag)
        if idx + 1 >= len(sys.argv):
            print(f'-F- {flag} requires an argument', file=sys.stderr)
            sys.exit(1)
        value = sys.argv[idx + 1]
        del sys.argv[idx:idx + 2]
        sys.argv.extend([pytest_equiv, value])
        return value
    return None


def consume_legacy_flags():
    """Translate legacy run-unit-tests.py flags to pytest equivalents.

    Call this before pytest parses sys.argv.
    """
    _consume_flag_with_arg(['-r', '--regex'], '-k')  # -r/--regex -> pytest's -k (keyword filter)
