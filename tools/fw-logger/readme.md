# rs-fw-logger Tool

## Goal
`rs-fw-logger` tool is a console application for collecting internal camera logs.
If you are suspecting that you have an issue that is related to the camera’s firmware, please use this tool and provide the output together with the FW version number when you are creating an issue.
In order to run this, ensure that your camera is streaming. This can be done using the [realsense-viewer](https://github.com/realsenseai/librealsense/tree/development/tools/realsense-viewer) or [rs-capture Sample](https://github.com/realsenseai/librealsense/tree/development/examples/capture)

## Command Line Parameters
|Flag   |Description   |Default| Range|
|---|---|---|---|
|`-l <xml-path>`|xml file ful path, used to parse the logs||
|`-p <polling-interval-in-ms>`|logs polling interval (in milliseconds)| 100 | 25-300|
|`-f`|collect flash logs instead of firmware logs||
|`-o <file-path>`|output file path||

## Usage
After installing `librealsense` run `rs-fw-logger` to launch the tool. 


