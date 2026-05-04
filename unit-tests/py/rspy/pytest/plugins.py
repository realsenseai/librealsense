# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Required pytest plugin registry and availability check.

Without this check a missing plugin silently disables its CLI options
(e.g. --repeat maps to pytest-repeat's --count; if pytest-repeat is
absent, the mapping becomes a no-op and tests run once instead of N
times with no error). We fail fast in pytest_configure instead.

When adding a new pytest plugin to unit-tests/requirements.txt, also
add it to REQUIRED_PYTEST_PLUGINS below.
"""

import importlib.util
import pytest


REQUIRED_PYTEST_PLUGINS = {
    # module name          : pip package name (for the install hint)
    'pytest_timeout':      'pytest-timeout',
    'pytest_retry':        'pytest-retry',
    'pytest_repeat':       'pytest-repeat',
    'pytest_check':        'pytest-check',
    'pytest_dependency':   'pytest-dependency',
}


def check_required_plugins(plugins=None):
    plugins = plugins if plugins is not None else REQUIRED_PYTEST_PLUGINS
    missing = [pip_name for mod, pip_name in plugins.items()
               if importlib.util.find_spec(mod) is None]
    if missing:
        raise pytest.UsageError(
            f"Missing required pytest plugin(s): {', '.join(missing)}. "
            f"Install via: pip install -r unit-tests/requirements.txt"
        )
