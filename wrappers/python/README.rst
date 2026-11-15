Python Wrapper for RealSense SDK 2.0
====================================

The Python wrapper for RealSense SDK 2.0 provides the C++ to Python binding required to access the SDK.

Quick start
-----------

::

  import pyrealsense2 as rs
  pipe = rs.pipeline()
  profile = pipe.start()
  try:
    for i in range(0, 100):
      frames = pipe.wait_for_frames()
      for f in frames:
        print(f.profile)
  finally:
      pipe.stop()

NVIDIA Jetson (aarch64)
-----------------------

The ``aarch64`` wheels are built with CUDA on a specific JetPack release. The wheel's
Python version selects the JetPack it was built for:

============  =============  =====  ====================
Runs on       Built on       CUDA   Wheel Python version
============  =============  =====  ====================
JetPack 5.x   JetPack 5.0.2  11.4   3.9
JetPack 6.x   JetPack 6.0    12.2   3.10
JetPack 7.x   JetPack 7.0    13.0   3.12
============  =============  =====  ====================

A wheel built on the first release of a JetPack line runs on every later release of that
line, but not on an older line.

Install with the matching Python (e.g. ``python3.10 -m pip install pyrealsense2`` on
JetPack 6). Installing with a different Python pulls a wheel built against another
JetPack's CUDA; ``pip`` cannot detect this and GPU-accelerated paths (e.g. RGB8/BGR8
color conversion, align, pointcloud) will fail. Python 3.8 (the JetPack 5 system Python) is
end-of-life and not supported; the minimum is Python 3.9. For other Python versions, or to
run without CUDA, build from source:
https://github.com/realsenseai/librealsense/blob/master/wrappers/python/readme.md#building-from-source
