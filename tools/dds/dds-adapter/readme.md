# rs-dds-adapter tool

## Goal

This tool (the server) is expected to run with a matching running client `librealsense` application (potentially on another machine) to demonstrate how the SDK can detect a RealSense device being connected / disconnected on the server side while Client and Server are connected via ethernet.

> NOTE: librealsense (both on client and server) needs to be built with `BUILD_WITH_DDS=ON` CMake flag

## Description

This tool will run and monitor the librealsense `devices_changed` callback.

If the tool detects a device dis/connection, it will notify the client using the DDS protocol.

## Command Line Parameters
| Flag | Description | Default|
|---|---|---|
|-h/--help|Show command line help menu||
|`--domain-id <0-232>`|Publish devices on the given domain ID|0|

## Expected Output
Assuming a running `librealsense` is found on the client side and network connection is stable, we expect to see prints like:

`Participant discovered`  --> The adapter found a valid client

`Device <device_name> - added` --> The device was added and a matching DDS data writer was created and publish the device information.

`Device <device_name> - removed`  --> The device was removed and a matching DDS data writer was destructed.
