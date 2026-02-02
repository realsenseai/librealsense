
See also: [device](device.md)


# Streaming

A stream is a single topic. Its data flows from the server (publisher) to the client (subscriber).


#### Quality of Service

- Reliability: `BEST_EFFORT`
- Durability: `VOLATILE`


## Start / Stop

To receive data, a client need give no command.

The act of subscribing is enough to start streaming.

I.e.: the server must track the number of subscribers and start or stop streaming from the sensor accordingly (if there is a sensor).


#### Multiple Clients

Note that streaming is a read-only operation and therefore can be shared: once streaming, any subscriber can see what's being streamed. See [`open-streams`](#open-streams) for information about changing what is being streamed.


#### Multicast

Network bandwidth is at a premium and multiple clients accessing the same server entails sending datagrams to each separately (by default). To save on bandwidth and efficiently read from a server, a multicast IP address is used.

The address is broadcast as part of [device discovery](discovery.md).

With multicast-enabled clients, the server has to send datagrams to one address, saving network bandwidth and processing time. The clients need to know to listen on this address.


#### Multiple Streams

To start multiple streams, a client must simply subscribe to multiple topics.


#### Incompatible profiles

If, when streaming is attempted (a subscriber is detected), no compatible profile can be found to the currently-streaming profile, an error will result.

E.g.:
* Default `Depth` profile is 1280x800
* Streaming starts on `Depth`
* Streaming is then attempted on `Infrared`
* `Infrared` has no profile for 1280x800; only for 1280x720

It is therefore very important for default profiles to agree with one-another and with the intended client (ROS2, specifically).


## Format

The type of stream matters and dictates the stream format.

For video streams, the format is the [ROS2 Image format](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Image.msg); for motion, the [ROS2 Imu format](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Imu.msg).


### Video

For video streams, the format is the [ROS2 Image format](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Image.msg). This is a simplified IDL for reference:

```rosidl
struct Time {
    long              sec;
    unsigned long     nanosec;
};

struct Header {
    Time              stamp;
    string            frame_id;
};

struct Image {
    Header            header;
    unsigned long     height;
    unsigned long     width;
    string            encoding;
    octet             is_bigendian;   // false
    unsigned long     step;
    sequence< octet > data;
};
```

The `encoding` is the same as the currently set profile format, and shouldn't change between frames. Neither should the `width`, `height`, `step`, or `frame_id`.


### Motion

See the [ROS2 Imu format](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Imu.msg). This is a simplified IDL for reference:

```rosidl
struct Vector3 {
    double x;
    double y;
    double z;
};

struct Quaternion {
    double x;
    double y;
    double z;
    double w;
};

struct Imu {
    Header            header;
    Quaternion        orientation;
    double[9]         orientation_covariance;
    Vector3           angular_velocity;
    double[9]         angular_velocity_covariance;
    Vector3           linear_acceleration;
    double[9]         linear_acceleration_covariance;
};
```


## Metadata

There are no frame numbers in the ROS2 formats.

It is therefore impossible to communicate a frame number unless additional data is used. This is what [metadata](metadata.md) is for. Without metadata (which is optional), frames will not be numbered, or will be up to the client to number (as happens in librealsense).

The same is true for any frame information that the server generates.


## Controls


#### `open-streams`

This control tries to configure the profile(s) used for streaming.

Note that it does not start streaming. It is a request. It may fail.

Notably, if a sensor is already streaming, this command will likely fail though the behavior is up to the server.

```JSON
{
    "id": "open-streams",
    "stream-profiles": {
        "Color": [30,"rgb8",1280,720]
    }
}
```

The `stream-profiles` is a mapping from `stream-name` to a `profile`. The `profile` is an array of `[frequency, format, width, height]`, similar to those used in initialization.

If the `stream-name` or `profile` are not found, this should result in an error.

If a field called `reset` is set to `true` (defaults to `false` -- the above JSON therefore has it at `false`), any previous `open-streams` are forgotten and the server resets all sensors to default profiles before processing the `stream-profiles` (which can be empty).

Multiple streams can be configured by a single `open-streams`. If these streams share a `sensor-name` (see [initialization](initialization.md#stream-header)), they must be compatible or an error will result.

If the stream profile cannot be changed (sensor is already open, for example), an error will be the result. This usually means that, once the stream is streaming, its profile cannot be modified by anyone. The only way is to stop all subscriptions first.

If `commit` is set to `true` (again the default), the state of the streams is locked in after `open-streams` and until the next `reset` is received. If `false`, additional `open-streams` requests can be cumulative (with `reset` also false). A `commit` is implicit when streaming actually starts.


##### Implicit vs. Explicit profiles

Many times profiles are dependent on one-another.

A good example is the `Stereo Module` sensor, with two streams: `Depth` and `Infrared`. When you set the `Depth` profile, the `Infrared` profile must change implicitly if not explicitly specified by `open-streams`, else streaming won't work (because the sensor cannot work at two different FPS or resolutions, for example).

When streaming starts (on first subscription), the server must align the profiles for the sensor. They are then "committed" and cannot be changed.

Assume the default for both streams is 1280x720. An `open-streams` is received for only `Depth`, to 640x480. `Infrared` should be implicitly set to the same resolution (if possible). `Depth` is then subscribed to. Then, to stop streaming, the subscription is removed. A second client then subscribes to either stream. Which resolution should it stream? An explicit resolution control was made, and so the same resolution should stay locked in until `reset`.

Without an `open-streams` control, all profiles are implicit, meaning streams revert back to their defaults when streaming stops.
