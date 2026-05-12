# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""Regression test for the ROS 2 launch.logging defense in conftest.py (RSDEV-9289).

When ROS 2 is sourced, launch_testing's pytest entry-point pulls in
launch.logging, which calls logging.setLoggerClass with a class that forces
propagate=False on every new logger. That breaks pytest log_cli — test
loggers never reach the root live-log handler.

This test reproduces the damage via a shim plugin loaded before conftest
(no ROS install needed) and asserts that conftest.py restored both the
Logger class and propagation on already-created loggers.
"""
import logging
import os
import subprocess
import sys
import textwrap


_SENTINEL = '_LRS_RUN_ROS_CANARY'
_UNIT_TESTS_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))


def test_conftest_repairs_poisoned_logger_class(tmp_path):
    # When invoked recursively by the outer test, behave as the canary:
    # assert conftest already repaired the damage the shim plugin inflicted.
    if os.environ.get(_SENTINEL):
        assert logging.getLoggerClass() is logging.Logger, (
            f"conftest did not reset Logger class; got {logging.getLoggerClass().__name__}"
        )
        assert logging.getLogger('_ros_pre_existing').propagate is True, (
            "conftest did not restore propagate on a pre-existing poisoned logger"
        )
        assert type(logging.getLogger('_ros_new_after_conftest')) is logging.Logger
        return

    # Outer run: write the shim plugin and re-invoke ourselves in a subprocess
    # with the shim loaded before conftest.
    (tmp_path / "_ros_logging_shim.py").write_text(textwrap.dedent("""\
        import logging

        class _Poisoned(logging.getLoggerClass()):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                self.propagate = False

        logging.setLoggerClass(_Poisoned)
        # Pre-create a logger to mimic launch.logging's import-time effect.
        logging.getLogger('_ros_pre_existing')
    """))

    env = os.environ.copy()
    env[_SENTINEL] = '1'
    env['PYTHONPATH'] = str(tmp_path) + os.pathsep + env.get('PYTHONPATH', '')

    p = subprocess.run(
        [sys.executable, '-m', 'pytest',
         '-p', '_ros_logging_shim',
         'infra-tests/test_e2e_ros_log_propagate.py::test_conftest_repairs_poisoned_logger_class',
         '-q', '--no-header'],
        cwd=_UNIT_TESTS_DIR,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        timeout=60,
    )
    assert p.returncode == 0, f"canary subprocess failed:\n{p.stdout}"
