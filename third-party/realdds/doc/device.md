
See the [list of topics used](../include/realdds/topics/) for a general overview of the topic structure.


# The DDS Device

A DDS device is a collection of topics on which information flows.

Specifically:
* `<topic-root>/`
    * `notification` — [server notifications, responses, etc.](notifications.md)
    * `control` — [client requests to server](control.md)
    * `metadata` — [optional stream information](metadata.md)
* `rt/<topic-root>_` — ROS2-compatible [streams](streaming.md)
    * `<stream-name>` — [Image](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Image.msg)/[Imu](https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/msg/Imu.msg) streams


## The Topic Root

Note that topics are all using the same `<topic-root>` that is specified in the [`device-info`](discovery.md). I.e., if the topic-root is known, all other topics can be found.

As long as the `<topic-root>` is a valid topic path, it can be anything and is entirely determined by [Discovery](discovery.md).

Currently, the topic root as used by `rs-dds-adapter` is built as:
> realsense/ `<device-model>` _ `<serial-number>`


## Settings

On the client, device behavior with librealsense can be fine-tuned with JSON settings passed through the participant (and therefore from the librealsense context's `dds` settings), inside a `device` object:

```JSON
{
  "dds": {
    "device": {
      "control": { "reply-timeout-ms": 2000 },
      "metadata": { "reliability": "reliable" }
    }
  }
}
```

The above overrides the default reply timeout to 2 seconds and makes the metadata topic use reliable QoS rather than the default `best-effort`.

#### Device creation and discovery settings

```JSON
{
  "dds": {
    "device-initialization-timeout-ms": 5000,
    "query-devices-max": 5,
    "query-devices-min": 3
  }
}
```

When a device-info sample is received, announcing a device is available on the network, an appropriate `dds-device` object is created in the SDK.
`dds-device` needs some handshaking with the device to determine the device's capabilities. The `device-initialization-timeout-ms` setting determines how long the SDK will wait for this handshake to complete. This initialization usually takes less than 2 seconds, but for a multi-camera multi-host scenario the network might be congested and this might take more time.

When the user queries for devices we want to return as quickly as possible. However for a scenario in which a context is created and immediately `query_devices` is called, DDS devices don't have enough time to complete initialization.
The first `query_devices` call will block to give DDS devices a chance to be initialized. It will wait for `query-devices-min` seconds and if a remote participant is discovered during that time, the wait time will increase to let the device initialize, up to a maximum of `query-devices-max` seconds.

#### Standard topic QoS settings

`control`, `notification`, and `metadata` topics all can have their own objects to override default QoS and other settings. See the above for an example. They all share common settings:

* `reliability` can be a string denoting the `kind`, or an object:
    * `kind` is `best-effort` or `reliable`
* `durability` can be a string denoting the `kind`, or an object:
    * `kind` is `volatile`, `transient-local`, `transient`, or `persistent`
* `history` can be a number denoting the `depth`, or an object:
    * `kind` is `keep-last` or `keep-all`
    * `depth` is the number of samples to manage with `keep-last`
* `data-sharing` is a boolean: `true` to automatically enable if needed; `false` to turn off
* `endpoint` is an object:
    * `history-memory-policy` is `preallocated`, `preallocated-with-realloc`, `dynamic-reserve`, or `dynamic-reusable`

Note that these settings are **overrides**. The default values may be different depending on the topic for which they're intended (for example, `metadata` uses `best-effort` by default while `control` and `notification` use `reliable`).

#### Other Settings

| Field                    | Default | Type    | Description        |
|--------------------------|--------:|---------|--------------------|
| `control`/
| &nbsp;&nbsp;&nbsp;&nbsp;`reply-timeout-ms` |    2000 | size_t  | Reply timeout, in milliseconds

#### Device Options

To use device-level options, the control-reply needs to be used.
The server publishes device options as part of the initialization sequence, in the `device-options` message.
If available, the [`query-option` and `set-option` controls](control.md#query-option--set-option) can be used (no `stream-name`) to retrieve or update the values. **Setting these options will not take effect until the device is reset.**

The following device options may be available:

| Setting        | Default | Type             | `option-name`   | Description          |
|----------------|--------:|------------------|-----------------|---------------|
| Domain ID      |       0 | int, 0-232       | `domain-id`     | The DDS domain number to use to segment communications on the network
| IP address     |   empty | string "#.#.#.#" | `ip-address`    | The static IP that the server uses for itself (empty=DHCP on)
| Multicast IP   |       - | string "#.#.#.#" | `multicast-ip` | The IP address to use for multicasting (empty to disable)


## Continue with:

* [Notifications](notifications.md)
* [Initialization](initialization.md)
* [Control](control.md)
* [Streaming](streaming.md)
* [Metadata](metadata.md)
* [Stream Configurations](stream-configurations.md)
