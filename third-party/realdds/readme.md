
# Introduction

"RealSense DDS" (RealDDS) is an abstraction layer on top of plain old DDS, adding RealSense-specific constructs to manage devices.

This is the basis for the implementation of DDS devices by librealsense.
I.e., RealDDS is not reliant on librealsense; librealsense is reliant on RealDDS.
As such, RealDDS is itself a self-contained API and can be used for testing, debug, or an actual application. There is a Python module built for it (`pyrealdds`), for exactly such usage.

### DDS

[Data Distribution Service](https://en.wikipedia.org/wiki/Data_Distribution_Service) (DDS) is a [standard](https://www.omg.org/spec/DDS) managed by the Object Management Group (OMG) defining a publish-subscribe pattern (RTPS) for connecting "nodes" over a network.

DDS communication is conducted over a *domain* where various *readers* and *writers* register for *topics* they want to publish or subscribe to.
A *Topic* defines the underlying structure and meaning of the data being shared, while each message transferred is called a *sample*.

There are many vendors that offer an implementation that meet the DDS standard. Currently we use [FastDDS](https://github.com/eProsima/Fast-DDS) as the underlying middleware.
For more information about DDS entities and the FastDDS API you can check out the [documentation](https://fast-dds.docs.eprosima.com/en/latest/).

# The Players

RealDDS encompasses both server and client roles: objects exist to serve data (device-server, stream-server, etc.) and receive it (device, stream, etc.).

Multiple *participants* can be part of this system: multiple clients can access a single server. I.e., the server broadcasts data rather than hand it to one particular client. Likewise, multiple servers can exist together (if the network bandwidth permits).

### Discovery

To bring clients and servers together, a [discovery](doc/discovery.md) mechanism at the device level (called the *device-info*) is provided in the form of a topic with a fixed name. A server would broadcast on this topic, while a client would watch it.

### Server

Any entity that provides a data stream on a specific topic is taking on the role of a *stream-server*.

Any entity that puts one or more stream-servers together for publication, with attached mechanisms for control & notification, is a *device-server*.

### Client

Consumes data produced by a server: a *stream* is the client to a stream-server, and a *device* to a device-server.

#### ROS2

[ROS2](https://docs.ros.org/) is one of the clients for which effort has been expended in order to be compliant. This is direct-to-ROS compliance that does not require use of a custom node (based on librealsense) for translation (as the [ROS Wrapper for Intel® RealSense™ cameras](https://github.com/realsenseai/realsense-ros) is): ROS is a DDS participant like any other Client.

Specifically, at this time, the requirement is that ROS nodes should be able to:
1. See any streams that RealDDS exposes
2. Understand the (default) formats used by those streams

As RealDDS is improved, more and more functionality will work directly with ROS.

# Terminology

Most objects in RealDDS are prefixed with `dds_` and exist in the `realdds::` namespace. When a `device-server` is mentioned, the corresponding object is likely `dds_device_server` unless otherwise noted.

# Usage

To enable DDS devices in *librealsense*, make sure `BUILD_WITH_DDS` is turned on in CMake. Make sure to check out the [`rs-dds-adapter` tool](../../tools/dds/dds-adapter) to turn any RealSense camera into a DDS device.

The client for DDS devices is librealsense and anything built with it, notably the Viewer: when built with `BUILD_WITH_DDS`, the Viewer will discover any devices available and should be able to stream from them, control them, and generally treat them like any other device.

Alternatively, one can use RealDDS to bypass librealsense. This will avoid some overhead but will also hide many of the capabilities that librealsense enables.

Or you can directly interact with the DDS subsystem, using any implementation. This bypasses librealsense and RealDDS completely. While the protocol used by RealDDS (the topic structure, message formats, etc.) will likely not change much, it is not guaranteed to stay static in the face of new features or bugs.

**Note**: In some systems where firewall is enabled, you may see the message "Do you want to allow networks to access this app", make sure to allow this in your network to ensure proper communication with the DDS device. This message shows up since we open and bind a socket when using DDS, so it should be allowed in the network security settings.

# Next

Start with [Discovery](doc/discovery.md), then continue on to [Device](doc/device.md).
