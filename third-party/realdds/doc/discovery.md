
See also: [device](device.md)


# What is discovery

Discovery enables a client to get information about live devices, including how to access them.

It does not include connecting to those devices, talking to them, or anything else.


# Domains

DDS domains separate logical systems so they do not interfere with one another on the network. Domains are numbered 0-232. Discovery on domain 0 will not detect devices on domain 1.

All documentation assumes a single domain shared between all entities.


# Participant

A server is a participant in the system, and may publish multiple devices. As an entity in the DDS, it should be named in a way to be easily identifiable and looked up:

- If for a single device, it is suggested to use the same topic root syntax `<model>_<serial>` as below, e.g. `D457_457123`.


# realsense/device-info

Only one topic is used for discovery:

>`realsense/device-info`

This topic is not meant for consumption by ROS, and is not meant to be visible there.


#### Quality of Service

- Reliability: `RELIABLE`
- Durability: `VOLATILE`


# Protocol

The requirement is very simple: whenever a server detects a client, it should broadcast its device-info so the client can see it.

The DDS subsystem enables this by notifying a publisher (the server) that a subscriber (client) exists for the topic.


# Device-Info

The device-info is general information about the device, plus how to access it.

It should not change over the device lifetime.

One device-info per device. At this time, only one device-info per message.

The message format is a [flexible message](../include/realdds/topics/flexible/) using a simple JSON structure, for example:

```JSON
{
  "name": "Intel RealSense D405",
  "serial": "123622270732",
  "product-line": "D400",
  "topic-root": "realsense/D405_123622270732"
}
```

| Field | Purpose | rs2_camera_info |
| ---- | ---- | ---- |
| **name** | The name of the device | `RS2_CAMERA_INFO_NAME` |
| serial | The device serial number | `RS2_CAMERA_INFO_SERIAL_NUMBER` |
| product-line | The type of device (D400, L500, etc.) | `RS2_CAMERA_INFO_PRODUCT_LINE` |
| **topic-root** | The path to the root topic for the device | `RS2_CAMERA_INFO_PHYSICAL_PORT` |
| fw-version | The version of the software currently on the device | `RS2_CAMERA_INFO_FIRMWARE_VERSION` |

All are optional except `name` and `topic-root`. Any fields not shown above will be ignored.

The fields above are constants and not expected to change between device restarts! They should likewise remain the same in recovery mode.
The `topic-root` is the device identity and is how a client matches a re-discovered device to one it already knows, so it cannot change for a given device.

Besides the fields above, certain configurable settings may be needed before proper initialization of the device is possible (via the notifications topic). These may change between device initializations:

| Field        | Purpose                               |
|--------------|---------------------------------------|
| multicast-ip | IP address on which topics will be multicast

For example, when multicasting is enabled then the notification topic will be multicast on the `multicast-ip` address. So knowledge of this address is needed before the device can actually be initialized. Very few settings like this should be needed.


# Disconnection

Under normal operation, the DDS subsystem will notify participants of entity disconnections without much delay. If a participant crashes or goes offline unexpectedly, a timeout (currently 10 seconds) is triggered and only then participants are notified.

When it is expected that a server will go offline, the server can elect to send a `stopping` message so that clients can remove the device immediately rather than waiting for DDS to do its thing.

```JSON
{
  "topic-root": "realsense/D405_123622270732",
  "stopping": true
}
```

The `stopping` field has no set type at this time so any value will do. When it's there, any client should immediately assume the server is offline.

No fields other than the root are necessary with `stopping`.


# Recovery

If the device is in "recovery mode" with limited functionality, this needs to be communicated to the client:

```JSON
{
  "name": "Intel RealSense D405",
  "serial": "123622270732",
  "topic-root": "realsense/D405_123622270732",
  "recovery": true
}
```

The `control` and `notification` topics will exist under the topic-root, but will only accept update controls and replies, as discussed elsewhere. No streams should be available in a recovery device.

It is important that the `topic-root` stays the **same as a non-recovery device**: otherwise they are considered different devices.


# Topic Root

This points to the topic used as the device root. This is how to access the device; without it, we're just guessing.

See [Device Initialization](initialization.md).


# librealsense

Librealsense manages a single point from which all other objects are derived: the `context`. To get access to a device, a `context` is created and then `query_devices()` called.

The `context` has been augmented to be able to see DDS devices. This is on by default if `BUILD_WITH_DDS` is on.

When a context is created, a JSON representation may be passed to it, e.g.: `{"dds": { "domain": 123, "participant": "librs" }}`. This allows various customizations:

| Field                | Default | Description                  |
|----------------------|--------:|------------------------------|
| dds                  | `{}`      | Set to `false` to turn off DDS in this context
| dds/`enabled`          | `true`    | If `false`, DDS is disabled

The `dds` is there by default (i.e., not `false`). The value may contain the following settings dealing with discovery:

| Field            | Default         | Description                      |
|------------------|----------------:|----------------------------------|
| domain           | `0`               | The domain number to use (0-232)
| participant      | Executable name | The name given this context (how other participants will see it)
| participant-id   | Automatic       | The ID; not recommended to use, but may be needed in special circumstances

See a comprehensive list of settings under [device](device.md#Settings).
