# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

"""
Python import-path helpers for RealSense unit tests.

The CMake build produces fresh pyrealsense2/pyrealdds/pyrsutils binaries which
must take precedence over any copies that pip may have left in the user site
(~/.local/lib/pythonX.Y/site-packages). The naive approach — stripping the
whole user-site from sys.path — also hides paramiko/pykush etc. that tests
legitimately consume from there.
"""

import os
import sys
from site import getusersitepackages

from rspy import file


def block_user_site_for( pkg_names ):
    """
    Make the listed top-level packages un-importable from the user site, leaving
    everything else in user-site reachable. No-op if user-site doesn't currently
    contain any of the named packages.

    The freshly-built copies (in the CMake build directory, added later via
    sys.path.insert) will be picked up instead.

    :param pkg_names: iterable of top-level package names to block
    """
    user_site = getusersitepackages()
    blocked = { pkg for pkg in pkg_names
                if os.path.exists( os.path.join( user_site, pkg ) ) }
    if not blocked:
        return

    import importlib.abc
    import importlib.machinery

    class _BlockUserSiteFinder( importlib.abc.MetaPathFinder ):
        def find_spec( self, fullname, path, target=None ):
            if fullname.split('.')[0] not in blocked:
                return None
            non_user = [ p for p in sys.path if not file.is_inside( p, user_site ) ]
            return importlib.machinery.PathFinder.find_spec( fullname, non_user, target )

    sys.meta_path.insert( 0, _BlockUserSiteFinder() )
