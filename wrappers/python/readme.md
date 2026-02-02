# Python Wrapper

## Table of Contents
* [Installation](#installation)
* [Building from source](#building-from-source)
* [Examples](#examples)

## Installation


Our official wrapper is  [`pyrealsense2`](https://pypi.org/project/pyrealsense2/)
We also provide a beta package for early access to new features called [`pyrealsense2-beta`](https://pypi.org/project/pyrealsense2-beta/)

To install the package, run:
> `pip install pyrealsense2`

Or

> `pip install pyrealsense2-beta`

Users who wish to create a python package (wheel) can create it from this folder by running 

```pip install -U setuptools build hatchling```

```python -m build . ```

Windows users can install the RealSense SDK 2.0 from the release tab to get pre-compiled binaries of the wrapper, for both x86 and x64 architectures. (Python versions 3.9, 3.10, 3.11, 3.12 and 3.13 are supported).

> **Note:**
> EOL Python 3.8 distributables can be found for pyrealsense2 versions <= 2.55.2
> EOL Python 3.7 distributables can be found for pyrealsense2 versions <= 2.55.1


## Building From Source

#### Ubuntu LTS
1. Ensure apt-get is up to date
  * `sudo apt-get update && sudo apt-get upgrade`
  **Note:** Use `sudo apt-get dist-upgrade`, instead of `sudo apt-get upgrade`, in case you have an older Ubuntu 14.04 version
2. Install Python and its development files via apt-get
  * `sudo apt-get install python3 python3-dev`
3. Clone the librealsense repository and navigate into the directory:
   * `git clone https://github.com/realsenseai/librealsense.git`
   * `cd librealsense`
4. Configure and make:
  * `mkdir build`
  * `cd build`
  * `cmake ../ -DBUILD_PYTHON_BINDINGS:bool=true -DPYTHON_EXECUTABLE=$(which python3)`
  * `make -j4`
  * `sudo make install`
  > **Note**: For building a self-contained (statically compiled) pyrealsense2 library add the CMake flag:
  >
  >`-DBUILD_SHARED_LIBS=false`
  >
  > **Note**: To force compilation with a specific version on a system with a few Python versions installed, add the following flag to CMake command:
  >
  >`-DPYTHON_EXECUTABLE=[full path to the exact python executable]`
  
5. update your PYTHONPATH environment variable to add the path to the pyrealsense library
  * `export PYTHONPATH=$PYTHONPATH:/usr/local/lib`
  > **Note:** If this doesn't work, try using the following path instead:
	`export PYTHONPATH=$PYTHONPATH:/usr/local/lib/[python version]/pyrealsense2`
6. Alternatively, copy the build output (`librealsense2.so` and `pyrealsense2.so`) next to your script.
  **Note:** Python 3 module filenames may contain additional information, e.g. `pyrealsense2.cpython-35m-arm-linux-gnueabihf.so`)



#### Windows
1. Install Python 3 for windows. You can find the downloads on the official Python website [here](https://www.python.org/downloads/windows/)

2. When running `cmake-gui`, select the `BUILD_PYTHON_BINDINGS` option

   **Note:** For building a self-contained (statically compiled) pyrealsense2 library add the CMake flag:

      `-DBUILD_SHARED_LIBS=false`

3. If you have multiple python installations on your machine you can use: `-DPYTHON_EXECUTABLE=<path to python executable>`
  For example: `-DPYTHON_EXECUTABLE=C:/Python310/python.exe`
> The precompiled binaries shipped with [the installer](https://github.com/realsenseai/librealsense/releases) assume **Python 3.11**.
>The error `ImportError: DLL load failed: The specified module could not be found` might indicate versions mismatch or architecture (x86 vs x64) mismatch.

4. Open `realsense2.sln` that was created in the previous step, and build the `pyrealsense2` project
5. Open the output folder of the project (e.g `build\x64-Release\Release\`) and copy `pyrealsense2.pyd` into your python's `site-packages` (e.g `C:\Python310\Lib\site-packages`)
6. Alternatively, copy the build output (`realsense2.dll` and `pyrealsense2.pyd`) next to your script.

## Examples

> For a list of full code examples see the [examples](./examples) folder

#### Streaming using _rs.pipeline_
```python
# First import the library
import pyrealsense2 as rs

# Create a context object. This object owns the handles to all connected realsense devices
pipeline = rs.pipeline()
pipeline.start()

try:
    while True:
        # Create a pipeline object. This object configures the streaming camera and owns it's handle
        frames = pipeline.wait_for_frames()
        depth = frames.get_depth_frame()
        if not depth: continue

        # Print a simple text-based representation of the image, by breaking it into 10x20 pixel regions and approximating the coverage of pixels within one meter
        coverage = [0]*64
        for y in range(480):
            for x in range(640):
                dist = depth.get_distance(x, y)
                if 0 < dist and dist < 1:
                    coverage[x//10] += 1

            if y%20 == 19:
                line = ""
                for c in coverage:
                    line += " .:nhBXWW"[c//25]
                coverage = [0]*64
                print(line)

finally:
    pipeline.stop()
```

#### NumPy Integration
Librealsense frames support the buffer protocol. A numpy array can be constructed using this protocol with no data marshalling overhead:
```python
import numpy as np
depth = frames.get_depth_frame()
depth_data = depth.as_frame().get_data()
np_image = np.asanyarray(depth_data)
```
