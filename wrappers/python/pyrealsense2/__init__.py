# py libs (pyd/so) should be copied to pyrealsense2 folder
from .pyrealsense2 import *
# `from ... import *` skips dunder names, so re-export them explicitly
# to make `pyrealsense2.__version__` work instead of `pyrealsense2.pyrealsense2.__version__`.
from .pyrealsense2 import __version__, __full_version__