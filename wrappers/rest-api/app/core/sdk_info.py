# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.


def sdk_version() -> str:
    """Version of the pyrealsense2 binary actually loaded, or 'unknown'.

    Read from the module (RS2_API_*, bound as __full_version__) rather than pip metadata:
    main.py may load a locally-built extension whose version differs from any installed
    wheel. The source-built wrapper re-exports __full_version__ on the package, while the
    PyPI wheel exposes it only on the extension submodule, so the inner module wins.
    """
    try:
        import pyrealsense2 as rs
        return getattr(getattr(rs, "pyrealsense2", rs), "__full_version__", "unknown")
    except Exception:
        return "unknown"
