# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

from rspy import log
import os, sys, signal

signal_handler = lambda: log.d("Signal handler not set")
_cleanup_in_progress = False


def register_signal_handlers(on_signal=None):
    def handle_abort(signum, _):
        global signal_handler, _cleanup_in_progress
        if _cleanup_in_progress:
            # Second signal during cleanup — force-exit immediately so we don't hang
            log.w("got signal", signum, "during cleanup — force-exiting")
            os._exit(1)
        _cleanup_in_progress = True
        log.w("got signal", signum, "aborting... ")
        signal_handler()
        os._exit(1)

    global signal_handler
    signal_handler = on_signal or signal_handler

    signal.signal(signal.SIGTERM, handle_abort)  # for when aborting via Jenkins
    signal.signal(signal.SIGINT, handle_abort)  # for Ctrl+C
