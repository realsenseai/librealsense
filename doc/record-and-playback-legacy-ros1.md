> **Legacy Documentation:** This document describes the ROS1 `.bag` recording format, which has been superseded by the ROS2 `.db3` format. For current documentation, see [Record and Playback](./record-and-playback.md).

Realsense Record and Playback (Legacy ROS1 Format)
===================


Overview
-------------
In addition to streaming video and other data from devices and sensors, the realsense SDK provides the ability to record a live session of streaming to a file. The recorded files can later be loaded by the SDK and to create a device with "read-only" abilities of the recorded device ( we will explain what "read-only" abilities mean later on).
The SDK is recording a single device to a single [rosbag file](http://wiki.ros.org/rosbag), using mostly standard ROS messages. This allows files recorded by the SDK to be replayed using any ROS tools \ application.

> For example recording files, please see [Sample Data](./sample-data.md)


Quick Start
-------------

#### `rs2::recorder`

> :exclamation: If you are not familiar with the basic streaming [examples](../examples), please follow them before moving on

To enable recording of any device, simply create a **rs2::recorder** from it and provide a path to the desired output file:
```cpp
//Create a context and get the first device
rs2::context ctx;
auto devices = ctx.query_devices();
if (devices.size() > 0)
{
    //Create a rs2::recorder from the first device, and desired file name
    //'.bag' is the common extension for rosbag files
    rs2::recorder device("my_file_name.bag", devices[0]); 	
    //recorder "is a" device, so just use it like any other device now
}
```
A `recorder` has the same functionality as a "real" device, with additional control for recording, such as pausing and resuming record.


#### `rs2::playback`

> :exclamation: If you are not familiar with the basic streaming [examples](../examples), please follow them before moving on

Recorded files can be loaded and used to create a playback device by simply loading a file to the context:
```cpp
//Create a context
rs2::context ctx;
//Load the recorded file to the context
rs2::playback device = ctx.load_device("my_file_name.bag");
//playback "is a" device, so just use it like any other device now
```
The above code creates a playback device, which can be used as any device, but has the obvious limitation of only playing the recorded streams.
Playback devices can be used to query information on the device and it sensors, and can be extended to which ever extension the "real" device could.
A `playback` provides additional functionalities such as seek, pause, resume and playback speed.

### Using `rs2::config` with `rs2::pipeline`

The `rs2::pipeline` can be configured to record or play a streaming session by providing it with a `rs2::config` of your choice:

Recording to file:
```cpp
rs2::config cfg;
cfg.enable_record_to_file("path_to_output_file.bag");
rs2::pipeline pipe;
pipe.start(cfg); //File will be opened in write mode at this point
for (int i = 0; i < 30; i++)
{
    auto frames = pipe.wait_for_frames();
    //use frames here
}
pipe.stop(); //File will be closed at this point
```

Playing from file:
```cpp
rs2::config cfg;
cfg.enable_device_from_file("path_to_input_file.bag");
rs2::pipeline pipe;
pipe.start(cfg); //File will be opened in read mode at this point
for (int i = 0; i < 30; i++)
{
    rs2::frameset frames;
    if (pipe.poll_for_frames(&frames))
    {
        //use frames here
    }
    //Note that we use poll_for_frames instead of wait_for_frames
    //This is because the file only contains a finite amount of frames
    // and if we use wait_for_frames then after reading the entire file
    // we will fail to wait for the next frame (and get an exception)
}
pipe.stop(); //File will be closed at this point
```


Playback and Record in RealSense Viewer
-------------

Among its many features, the [RealSense Viewer](../tools/realsense-viewer) allows recording a device, and loading a file to playback.

To record a streaming session, simply click the "bars" icon next to the device name, choose "Record to File...", and select the destination for the file.

![Recording a device](./img/record_screenshot.png)

After choosing a file destination, a red dot will appear next to the device's name, indicating that it is recording.
Starting a stream will save its frames to the file, and once all streams are stopped, recording will complete automatically.

To replay a file, click "Add Source", choose "Load Recorded Sequence" and select the file you want to play.
Once you select the file, the Viewer will automatically add it to the list of sources, and a popup should appear stating that the file was loaded:

![Loading Recorded Sequence](./img/playback_screenshot.png)
>Notice that devices that were loaded from file have a "movie" icon next to their name.  

After loading the file, you can start streaming its streams, view its controls (with the values at time of record), pause the playback, choose speed, and use the seek bar to navigate through frames.


Under the Hood
------

#### Basics Terminology

A **Device** is a container of Sensors with some correlation between them (e.g - all sensors are on a single board, sensors are mounted on a robot and share calibration information, etc.). A **Sensor** is a data streaming object, that provides one or more Streams.
**Stream** is a sequence of data items of a single data type, which are ordered according to their time of creation or arrival. The Sensor provides the Streams frames to the user.

We call the device's sensors and streams, the **topology** of the device.

Devices and Sensors can have **Extensions** that provide additional functionalities. A **Snapshot** of an Extension is a snapshot of the data that is available by the extension at some point of time, it is a sort of "read-only" version of the extension. For example, say we have a `DigitalClockExtension`, that can set and show the time. If we take a snapshot of that extension at noon, then whenever we ask the snapshot to show the time it will show "12:00", and trying to set its time will fail.

Finally, we will refer to an actual implementation of devices and sensors as "live" or "real" devices and sensors.

#### Rosbag

##### Overview on ROSBAG file structure

The Rosbag file consists of data records, each data that is written to the file is represented by a topic that represents the data content, message that is the data definition, and capture time.
Any topic can be coupled with any message, in our file each topic will be represented by a single message. However, we will have different topics with the same message such as property.  
A bag file consists of a list of records, each record can be one of the following types:

- Bag header. Stores information about the entire bag, such as the offset to the first index data record, and the number of chunks and connections.
- Chunk. Stores (possibly compressed) connection and message records.
- Connection. Stores the header of a ROS connection, including topic name and full text of the message definition (message structure).
- Message data. Stores the serialized message data (which can be zero-length) with the ID of the connection.
- Index data. Stores an index of messages in a single connection of the preceding chunk.
- Chunk info. Stores information about messages in a chunk.

Each message that we write is stored in a chunk, each chunk consists of a number of messages based on the chunk size. In addition to the message data, the chunk stores also the connections of the messages, the connection describes the topic name and message definition to be able to serialize the message data.
The flow to find a message on topic “A”:

- Check in all chunk info’s what kind of connections can be found in the chunk.
- Access the chunk with the wanted connection, chunk info contains an offset to the paired chunk.
- Access the message from the relevant index data.

##### Dependencies
The SDK depends on ROS packages that are used for recording and playing the bag file.
The dependencies are:
- rosbag_storage
- roscpp_serialization
- cpp_common
- rostime
- boost_system
- lz4

> **Note:** The above dependencies are embedded into the source files, there is no need for additional installations.

##### Topics

The rosbag file requires messages to be associates with Topics
> Topics are [named](http://wiki.ros.org/Names) buses over which [nodes](http://wiki.ros.org/Nodes) exchange [messages](http://wiki.ros.org/Messages). [[ROS Wiki\Topics]](http://wiki.ros.org/Topics)

The following table depicts the Topics that are supported by RealSense file format:

<table>
  <tr>
    <th>Topic</th>
    <th>Published Name</th>
    <th>Message Type</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>File Version</td>
    <td>/file_version</td>
    <td><a href="http://docs.ros.org/api/std_msgs/html/msg/UInt32.html">std_msgs/UInt32</a></td>
    <td>Version of file format.<br>A single message for entire file</td>
  </tr>
  <tr>
    <td>Device Information</td>
    <td>/device_&lt;device_id&gt;/info</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Device information.<br>Many messages to a single topic</td>
  </tr>
  <tr>
    <td>Sensor Information</td>
    <td>/source_&lt;source_id&gt;/info</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Sensor information.<br>Many messages to a single topic</td>
  </tr>
  <tr>
    <td>Stream Information</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/info</td>
    <td><a href="#stream-info">realsense_msg/StreamInfo</a></td>
    <td>generic-stream information<br>A single messages to a single topic</td>
  </tr>
  <tr>
    <td>Video Stream Info</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/info/camera_info</td>
    <td><a href="http://docs.ros.org/api/sensor_msgs/html/msg/CameraInfo.html">sensor_msgs/camera_info</a></td>
    <td>Image information.<br>A single messages to a single topic</td>
  </tr>
  <tr>
    <td>IMU Intrinsics</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/info/imu_intrinsic</td>
    <td><a href="#motion-intrinsic">realsense_msgs/ImuIntrinsic</a></td>
    <td>Intrinsic of a motion stream.<br>A single messages to a single topic</td>
  </tr>
  <tr>
    <td>Options</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/option/&lt;option name&gt;/value<br>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/option/&lt;option name&gt;/description</td>
    <td><a href="http://docs.ros.org/api/std_msgs/html/msg/Float32.html">std_msgs/Float32</a><br><a href="http://docs.ros.org/api/std_msgs/html/msg/String.html">std_msgs/String</a></td>
    <td>Options of a sensor.<br>values represent the value of the option and has many messages to a single topic<br>description is a human readable description of the option, with a single messages to a single topic</td>
  </tr>
  <tr>
    <td>Image Data</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/image/data</td>
    <td><a href="http://docs.ros.org/api/sensor_msgs/html/msg/Image.html">sensor_msgs/Image</a></td>
    <td>The data of a single image. A single messages to a single topic</td>
  </tr>
  <tr>
    <td>Image Information</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/image/metadata</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Additional information of a single image. Many message to a single topic</td>
  </tr>
  <tr>
    <td>IMU Data</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/imu/data</td>
    <td><a href="http://docs.ros.org/api/sensor_msgs/html/msg/Imu.html">sensor_msgs/Imu</a></td>
    <td>The data of a single imu frame.<br>A single messages to a single topic</td>
  </tr>
  <tr>
    <td>IMU Information</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/imu/metadata</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Additional information of a single imu frame. Many message to a single topic</td>
  </tr>
  <tr>
    <td>Pose Data</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/pose/{transform, accel, twist}/data</td>
    <td>
        <table>
               <tbody>
                    <tr>
                        <td>pose/transform
                        <td><a href="http://docs.ros.org/jade/api/geometry_msgs/html/msg/Transform.html">geometry_msgs/Transform Message</a></tr>
                    <tr>
                        <td>pose/accel
                        <td><a href="http://docs.ros.org/jade/api/geometry_msgs/html/msg/Accel.html">geometry_msgs/Accel Message</a></tr>
                    <tr>
                        <td>pose/twist
                        <td><a href="http://docs.ros.org/jade/api/geometry_msgs/html/msg/Twist.html">geometry_msgs/Twist Message</a></tr>
        </table>
    </td>
    <td>The Pose data is split into 3 messages (each under different topic <sup><a href="#1-pose-frames">[1]</a></sup>):</br>
        <table>
               <tbody>
                    <tr>
                        <td>pose/transform
                        <td>Translation and rotation of the object</tr>
                    <tr>
                        <td>pose/accel
                        <td>Angular and linear acceleration of the object</tr>
                    <tr>
                        <td>pose/twist
                        <td>Angular and linear velocity of the object</tr>
        </table>
    </td>
  </tr>
  <tr>
    <td>Pose Information</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/pose/metadata</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Additional information of a single pose frame. Many message to a single topic</td>
  </tr>
  <tr>
    <td>Occupancy Map Data</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/occupancy_map/data</td>
    <td><a href="http://docs.ros.org/api/nav_msgs/html/msg/OccupancyGrid.html">nav_msgs/OccupancyGrid</a></td>
    <td></td>
  </tr>
  <tr>
    <td>Occupancy Map Information</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/occupancy_map/metadata</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Additional </td>
  </tr>
  <tr>
    <td>Time Stream</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/time</td>
    <td><a href="http://docs.ros.org/jade/api/sensor_msgs/html/msg/TimeReference.html">sensor_msgs/TimeReference</a></td>
    <td></td>
  </tr>
  <tr>
    <td>Log</td>
    <td>/log</td>
    <td><a href="http://docs.ros.org/jade/api/rosgraph_msgs/html/msg/Log.html">rosgraph_msgs/Log</a></td>
    <td>Log messages</td>
  </tr>
  <tr>
    <td>Stream Exntrinsics</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/&lt;stream_type&gt;_&lt;stream_id&gt;/tf/&lt;group_index&gt;</td>
    <td><a href="http://docs.ros.org/jade/api/geometry_msgs/html/msg/Transform.html">geometry_msgs/Transform Message</a></td>
    <td>Extrinsic transformation between some point of reference (indexed by group_index) to the stream in the topic</td>
  </tr>
  <tr>
    <td>Additional Info</td>
    <td>/additional_info</td>
    <td><a href="http://docs.ros.org/api/diagnostic_msgs/html/msg/KeyValue.html">diagnostic_msgs/KeyValue</a></td>
    <td>Additinal information of any kind. Can be useful for application that require additional metadata on the recorded file (such as program name, version etc...)</td>
  </tr>
  <tr>
    <td>Sensor Notification</td>
    <td>/device_&lt;device_id&gt;/sensor_&lt;sensor_id&gt;/notification/&lt;rs2_notification_category&gt;</td>
    <td><a href="#notification">realsense_msgs/Notification</td>
    <td>Additinal information of any kind. Can be useful for application that require additional metadata on the recorded file (such as program name, version etc...)</td>
  </tr>
</table>

##### [1] Pose frames

As you might have noticed, pose data is split into three messages, each under a different topic.
The reasons for this choice is so that we expose only standard ros messages instead of creating a proprietary message.
This choice allows any common tool that uses rosbag to read the content of a pose frame (and most of the rest of the messages we save).

When reading a file using the SDK's playback, the reader will only iterate over `pose/transform` messages and for each of those messages, will match the corresponding `pose/accel` and `pose/twist` message to form a single pose frame.
This matching of messages adds an overhead of searching the file for a single message, each frame.
But, this overhead should have minor performance impact even for huge files since it is merely a tree search (See [ros/ros_comm: PR #1223 - Performance improvement for lower/upper bound](https://github.com/ros/ros_comm/pull/1223))


##### Messages
ROS uses a simplified messages description language for describing the data values (aka messages) that ROS nodes publish. This description makes it easy for ROS tools to automatically generate source code for the message type in several target languages. Message descriptions are stored in .msg files.
There are two parts to a .msg file: fields and constants. Fields are the data that is sent inside of the message. Constants define useful values that can be used to interpret those fields (e.g. enum-like constants for an integer value).

--------------

#### RealSense Proprietary  Messages

In addition to the standard ROS messsages, the SDK writes additional proprietary messages (available in the [3rd party folder](../third-party/realsense-file/rosbag/msgs/realsense_msgs)) for new data types that are recorded.
The following are the new messages created by the SDK:

 - [realsense_msgs::StreamInfo](#stream-info)
 - [realsense_msgs::ImuIntrinsic](#motion-intrinsic)

--------------
##### **Stream Info**

This message defines meta information for a stream
The stream type is expressed in the topic name

<table>
  <tr>
    <th>Name</th>
    <th>Description</th>
    <th>Format</th>
  </tr>
    <tr>
      <td>is_recommended</td>
      <td>Indicates if this stream is recommended by RealSense SDK</td>
      <td>bool</td>
    </tr>
  <tr>
    <td>fps</td>
    <td>Frame per second value</td>
    <td>uint32</td>
  </tr>
  <tr>
    <td>encoding</td>
    <td>Stream's data format.
    Supported encodings are listed below</td>
    <td>string</td>
  </tr>
</table>

###### Supported Encoding

For video streams, the supported encoding types can be found at <a href="http://docs.ros.org/jade/api/sensor_msgs/html/namespacesensor__msgs_1_1image__encodings.html">ros documentation</a>. Additional supported encodings are listed under [rs_sensor.h](../include/librealsense2/h/rs_sensor.h) as the `rs2_format` enumeration. Note that some of the encodings appear in both locations.

--------------

##### Motion Intrinsic

This message defines a Motion device's intrinsic: scale, bias, and variances

<table>
   <tbody>
      <tr>
         <th>Name</th>
         <th>Description</th>
         <th>Format</th>
      </tr>
      <tr>
         <td>data</td>
         <td>
            <p>Interpret data array values. <br>3x4 Row-major matrix:</p>
            <table>
               <tbody>
                  <tr>
                     <td>Scale X</td>
                     <td>Cross Axis</td>
                     <td>Cross Axis</td>
                     <td>Bias X</td>
                  </tr>
                  <tr>
                     <td>Cross Axis</td>
                     <td>Scale Y</td>
                     <td>Cross Axis</td>
                     <td>Bias Y</td>
                  </tr>
                  <tr>
                     <td>Cross Axis</td>
                     <td>Cross Axis</td>
                     <td>Scale Z</td>
                     <td>Cross Axis</td>
                  </tr>
               </tbody>
            </table>
         </td>
         <td>float32[12]</td>
      </tr>
      <tr>
         <td>noise_variances</td>
         <td>Variance of noise for X, Y, and Z axis</td>
         <td>float32[3]</td>
      </tr>
      <tr>
         <td>bias_variances</td>
         <td>Variance of bias for X, Y, and Z axis</td>
         <td>float32[3]</td>
      </tr>
   </tbody>
</table>
----------

##### Notification

This message defines a notification

<table>
   <tbody>
      <tr>
         <th>Name</th>
         <th>Description</th>
         <th>Format</th>
      </tr>
      <tr>
         <td>timestamp</td>
                 <td>The time of occurrence</td>
         <td><a href="http://docs.ros.org/api/std_msgs/html/msg/Time.html">std_msgs/Time</a></td>
      </tr>
      <tr>
         <td>category</td>
         <td>Category of the notification, matching an rs2_notification_category</td>
         <td><a href="http://docs.ros.org/api/std_msgs/html/msg/String.html">std_msgs/String</a></td>
      </tr>
            <tr>
         <td>severity</td>
         <td>Severity of the notification, matching an rs2_log_severity</td>
         <td><a href="http://docs.ros.org/api/std_msgs/html/msg/String.html">std_msgs/String</a></td>
      </tr>
            <tr>
                 <td>description</td>
                 <td>Human readable description of the notification</td>
                 <td><a href="http://docs.ros.org/api/std_msgs/html/msg/String.html">std_msgs/String</a></td>
            </tr>
            <tr>
                 <td>serialized_data</td>
                 <td>JSON string with additional data</td>
                 <td><a href="http://docs.ros.org/api/std_msgs/html/msg/String.html">std_msgs/String</a></td>
            </tr>
   </tbody>
</table>

### Versioning

Each bag file recorded using this SDK should contains a version message.
Version number is a single integer which indicates the version of topics and messages that were used to record the file.
The above messages and topics reflect the current version.
Changes from previous versions will appear at the end of this section.

> Current file version: ***3***

Changes from previous version:

- Removed:
    - ***Property*** topic
- Added:
    - ***Options*** topic



----------


Recording
----------
Recording is performed at the Device level, meaning that the device, its sensors, their streams' data (frames) and all extensions are saved to file.
To allow for a seamless experience when switching between a live device and a record or playback device we save the device's topology and all of the extensions' snapshots to the file, in addition to the streaming frames.

A record device is like a wrapper around a real device, that delegates actions to the device and sensors while recording new data in between actions.
When a record device is created, a record sensor is created per real sensor of the real device.
A record sensor will record newly arriving frames for each of its streams, and changes to extensions' data (snapshots).

Recording related files are:
 - [record/record_device.cpp](../src/media/record/record_device.cpp)
 - [record/record_device.h](../src/media/record/record_device.h)
 - [record/record_sensor.cpp](../src/media/record/record_sensor.cpp)
 - [record/record_sensor.h](../src/media/record/record_sensor.h)
 - [ros/ros_writer.h](../src/media/ros/ros_writer.h)

A `librealsense::record_device` is constructed with a "live" device and a `device_serializer::writer`. At the moment the only `device_serializer::writer` we use is a `ros_writer` which writes device information to a rosbag file.

When constructing a `ros_writer` the requested file is created if it does not exist, and then opened for writing. In addition, a single message containing the realsense file format version is written to the file.
The `ros_writer` implements the `device_serializer::writer` interface which has only 4 functions:
 - write_device_description
     - Used to record the initial state of the device. This includes writing all of the device's and sensor's extensions.
 - write_frame
     - Used to record a single frame to file
 - write_snapshot (2 overloads)
     - Used to record a snapshot of an extension to file.

#### Recording device description
A device description (or device snapshot) is the set of data that is available for any `librealsense::device`. It contains all of the device's extensions snapshots, and a collection of each  of the device's sensors' extensions snapshots. In addition it holds a mapping of extrinsic information for all streams of the device.

A single `librealsense::device_snapshot` instance is enough to understand the a device's topology: Which extensions it supports, how many sensors it has, which extensions each sensor supports, which streams each sensor supports and which streams share calibration information (extrinsic).

#### Recording Frames
Each frame in the SDK implements the  `librealsense::frame_interface` interface. This means that frames are polymorphic and represent all types of data that streams provide.
Frames are recorded to file with all of their additional information (such as metadata, timestamp, etc...), and the time that they arrived from the backend to the sensor.


#### Recording Snapshots
Upon creation, the record device goes over all of the extensions of the real device and its sensors, and saves snapshots of those extensions to the file (This is the data that is passed to `write_device_description(..)`) . These snapshots will allow the playback device to recreate the topology of the recorded device, and will serve as the initial data of their extensions.
To allow record sensors to save changes to extensions' data over the life time of the program, when a user asks a record sensor for an extension, a record-able extension is provided.
A record-able version of an extension holds an action to perform whenever the extension's data changes. This action is provided by the record device (or sensor), and requires extensions to pass a reference of themselves to the device, which will usually create a snapshot from them and record them to file with the time at which they occurred.

----------


## Playback

Playback device is an implementation of device interface which reads from a file to simulate a real device.
Playback device holds playback sensors which simulate real sensors.         
When creating the playback device, it will read the initial device snapshot from the file in order to map itself and its sensors in matters of functionality and data provided.               
When creating each sensor, the device will create a sensor from the sensor's initial snapshot. 
Each sensor will hold a single thread for each of the sensor's streams which is used to raise frames to the user.
The playback device holds a single reading thread that reads the next frame in a loop and dispatches the frame to the relevant sensor.
The reading of the file, as well as each sensor's handling of frames, are done in separate threads. All this is managed via a common `dispatcher` concurrency mechanism: an `invoke()` call enqueues an `action` and is dequeued and run from a worker thread.

### Sequence Diagram
![playback](./img/playback/playback-flow.png)

*Note: this flow uses the sensor API; `librealsense2` supports playback with the pipeline API as well, which looks similar inside the playback device.*
*Created using  [DrawIO](https://app.diagrams.net/)