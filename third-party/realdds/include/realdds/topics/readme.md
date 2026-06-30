
# Topic List

The various topics are arranged hierarchically:

* `realsense/`
    * `device-info` — [device discovery](../../../doc/discovery.md) ([flexible](flexible/))
    * `<model>_<serial-number>/` — topic root per device in [device-info](../../doc/discovery.md) (all in [flexible](flexible/) format)
        * `notification` — [server notifications, responses, etc.](../../../doc/notifications.md)
        * `control` — [client requests to server](../../../doc/control.md)
        * `metadata` — [optional stream information](../../../doc/metadata.md)
* `rt/realsense/` — ROS2-compatible [streams](../../../doc/streaming.md)
    * `<model>_<serial-number>_<stream-name>` — [Image](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Image.msg)/[Imu](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Imu.msg) stream supported by the device (e.g., Depth, Infrared, Color, Gyro, etc.)

# Interface Definition Files

The code for each message type in this directory is generated from IDL files if the message format is defined by RealDDS.

For [ROS](#ros2), the IDLs are available in the `/opt/ros/<distro>/share/` installation folder.

# Generation

A utility from FastDDS called [FastDDSGen](https://fast-dds.docs.eprosima.com/en/latest/fastddsgen/introduction/introduction.html#fastddsgen-intro) is required to convert the [IDL](https://fast-dds.docs.eprosima.com/en/latest/fastddsgen/dataTypes/dataTypes.html) files to the headers that're needed.
Unfortunately, this utility requires a java installation and sometimes not even the latest.

Instead, we found it easiest to download a Docker image for FastDDS (last used is version 2.10.6) and generate the files within.

1. Download the Docker image from [here](https://www.eprosima.com/component/ars/releases/eprosima-fast-dds?Itemid=0) (link was valid at time of writing; if no longer, register with eProsima and update it)

    ```zsh
    docker load -i /mnt/c/work/ubuntu-fastdds-suite\ v2.10.6.tar
    ```

2. In a `zsh` shell:

    ```zsh
    # from third-party/realdds/
    #
    for topic in flexible
    do
    cd include/realdds/topics/${topic}
    cid=`docker run -itd --privileged ubuntu-fastdds:v2.10.6`
    docker exec $cid mkdir /idl /idl/out
    docker cp *.idl $cid:idl/
    docker exec -w /idl/out $cid fastddsgen -cs -typeobject /idl/`ls -1 *.idl`
    docker cp $cid:/idl/out .
    docker kill $cid
    cd out
    for cxx in *.cxx; do mv -- "$cxx" "../../../../../src/topics/${cxx%.cxx}.cpp"; done
    mv -- *TypeObject.h "../../../../../src/topics/"
    mv * ..
    cd ..
    rmdir out
    cd ../../../..
    done
    ```

    This automatically renames .cxx to .cpp, places them in the right directory, etc.

3. Once files are generated, they still need some manipulation:
    * Certain `#include` statements (mainly in the .cpp files moved into src/) need to be updated (e.g., `#include "flexible.h"`  changed to `#include <realdds/topics/flexible/flexible.h>`)
    * Copyright notices in all the files needs updating to LibRealSense's

# ROS2

All the ROS2 topics are under `ros2/`. The IDLs are not part of this repo and the source code was pre-generated then modified appropriately.

The per-distro (including rolling) source message (`.msg`) files can be found in:
>https://github.com/ros2/common_interfaces

The `.msg` files can be converted to `.idl` with [rosidl](https://docs.ros.org/en/rolling/Concepts/About-Internal-Interfaces.html#the-rosidl-repository) and then `fastddsgen` can be used to generate source code, similar to what was detailed above. This was all done offline and is not part of the build.
Note - when generating ROS2 compatible topics using `fastddsgen` the flag `-typeros2` should be used to follow ROS2 topic naming convention.

Because there is an interest in keeping our topics ROS-compatible (i.e., so that ROS applications can pick up on and read them), this requires specific topic naming conventions and formats. We try to use ROS-compatible formats, encodings, and names where possible. Right now this is done only partially: ROS2 can read certain topics, and control the camera, but advanced features like calibrations are not available through native ROS yet.
