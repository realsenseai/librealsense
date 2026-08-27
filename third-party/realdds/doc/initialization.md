
See also: [device](device.md), [notifications](notifications.md)


# Initialization

In order to stream, a client must know the stream names, available formats, etc.

The only way for a client to get these is by subscribing to the `notification` topic. When the server detects a subscriber on this topic, it will broadcast a set of initialization messages in the following order:

- `device-header`
- `device-options` - optional
- For each stream:
    - `stream-header`
    - `stream-options`

These initialization messages should only have effect on devices that are not already initialized. They are expected in the above order.

Once all streams have been received, the device is initialized and ready to use. See [Streaming](streaming.md).


## "Atomic" Initialization


Initialization may happen interspersed with regular notifications: for example, a server that's already serving data and notifications to an existing client may see another client and broadcast initialization messages.

Only one set of initialization messages can be sent at a time. I.e., if a new client is detected while initialization messages for a previous client are still outgoing, a new set of messages must go out but only once the previous set is finished.

While a set of initialization messages are outgoing, all other notifications must take a back seat and wait until the set is written out.


## Messages


### `device-header`

This is the very first message, for example:

```JSON
{
    "id": "device-header",
    "n-streams": 5,
    "extrinsics": [
        ["Depth","Gyro",[1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0,-0.005520000122487545,0.005100000184029341,0.011739999987185001]]
        ["Depth","Infrared 2",[1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0,-0.04986396059393883,0.0,0.0]]
    ],
    "presets": ["Default", "Max Range", "Max Quality"]
}
```

- `n-streams` is the number of streams to expect
     The device will wait for this many stream headers to arrive to finish initialization
- `extrinsics` describe world coordinate transformations between any two streams in the device, required for proper translation of pixel coordinates between sensors, such as when a point-cloud is needed
- `presets` is an optional array of preset names
    The presets may then be applied using `change-preset`

#### Extrinsics

In order to translate from one stream viewpoint to another, a graph needs to be available with nodes for each stream and a directional edge between them (one from A to B; another from B to A).

So `extrinsics` is an array of such edges: `[<edge1>,<edge2>,...]`,
Each `edge` is also an array: `[<from>,<to>,[<r00>,<r01>,...,<r22>,<tx>,<ty>,<tz>]]`.
Where `<from>` and `<to>` are stream names, and the array inside contains the 9 rotation values (column-major) followed by 3 translation vector values.

The complete graph can be transmitted like this but this is overkill: given at least one edge from each stream to another, the rest can be computed. This is what librealsense does:
- Depth to IR = identity
- Depth to IR2 = {...}
- RGB to Depth = {...}

With those 3, one can compute IR2 to RGB, for example.

### `device-options`

This is optional: not all devices have options. Device options will not be shown in the Viewer. See [device](device.md).

```JSON
{
    "id": "device-options",
    "options": [
        ["Domain",0,0,232,1,0,"The DDS domain (0-232) in which this device will be discovered"],
        ["IP Address","1.2.3.4",null,"Which IP address to assign to the device; if empty, DHCP will be used", ["optional","IPv4"]]
    ]
}
```

* `"options"` is an array of options:

#### Options

Options are defined with a JSON array: `[name, value, range..., default-value, description, [properties...]]`:
* The `name` is what will be displayed to the user
* The current `value`
* An optional `range` of valid values
    * Numeric options (`float`, `int`), defined by a `minimum`, `maximum`, and `stepping`
        * I.e., is-valid = one-of( `minimum`, `minimum+1*stepping`, `minimum+2*stepping`, ..., `maximum` )
    * Booleans can remove the range, e.g. `["Enabled", true, true, "Description"]`
        * Booleans can be expressed as a range with `minimum=0`, `maximum=1`, `stepping=1`
    * Free string options would likewise have no range, e.g. `["Name", "Bob", "", "The customer's name"]`
        * `"IPv4"` is a string option that conforms to `W.X.Y.Z` (IP address) format
    * Enum options are strings with an array of choices, e.g. `["Preset", "Maximum Quality", ["Maximum Range", "Maximum Quality", "Maximum Speed"], "Maximum Speed", "Standard preset combination of options"]`
    * Rectangles are defined with values that arrays themselves: `[x1, y1, x2, y2]`
        * All four should be integers
        * No range should be used
        * E.g., `["name", [1,2,3,4], null, "description", ["optional"]]`
* A `default-value` which also adheres to the range
    * If this and the range are missing, the option is read-only
* A user-friendly description that describes the option, to be shown in any tooltip
* Additional `properties` describing behavior or nature, as an array of (case-sensitive) strings
    * `"optional"` to note that it's possible for it to not have a value; lack of a value is denoted as `null` in the JSON
        * If optional, a type must be deducible or present in the properties
        * E.g., `["name", null, "description", ["optional", "string"]]` is an optional read-only string value that's currently unset
        * Enums cannot be optional
    * `"string"`, `"int"`, `"boolean"`, `"float"`, `"IPv4"`, `"enum"`, `"rect"` can (and sometime must) indicate the value type
        * If missing, the type will be deduced, if possible, from the values
    * `"read-only"` options are not settable
        * `set-option` will fail for these, though their value may change on the server side


### `stream-header`

Information about a specific stream:
- `name` is the stream name, e.g. `Color`
    - This will be shown in the Viewer
    - This is also the name of the topic containing the stream data
        - E.g., `rt/<topic-root>_Color`
        - If it contains a space, the space will be replaced by `_` - e.g., `rt/<topic-root>_Infrared_1`
- `profiles` array
    - Each profile is itself an array of `[frequency, format, width, height]`
    - Format is a string representation, similar to the image encoding in ROS
- `default-profile-index` is the index into the `profiles` for the default profile
- `sensor-name` is the name of the sensor
    - Sometimes, a single sensor may produce multiple streams, e.g. `Stereo Module` produces a `Depth` stream and also `Infrared`
    - Streaming one stream requires starting the sensor, and so may have effect on the other streams, depending on the server implementation
    - This allows streams to be grouped by the client and may affect its logic
- `type` is one of `ir`, `depth`, `color`, `confidence`, `motion` - similar to the librealsense `rs2_stream` enum
- `metadata-enabled` is `true` if a `metadata` topic for the device will be written to

The order in which `stream-header` messages are sent is meaningful: a client assigns stream indices from it.
Where a sensor has several streams of the same `type` and the names carry no explicit index, the first one
declared gets index 0 and the rest follow - e.g. `Color`, then `Color/compressed` produce Color 0 and Color 1.


```JSON
{
    "default-profile-index": 29,
    "id": "stream-header",
    "metadata-enabled": true,
    "name": "Color",
    "profiles": [
        [30,"rgb8",1280,800],
        [30,"BYR2",1280,800],
        [30,"Y16",1280,800],
        ...
    ],
    "sensor-name": "RGB Camera",
    "type": "color"
}
```

### `stream-options`

- `stream-name` is the name of the stream, same as in `stream-header`
- `intrinsics` is:
    - For video streams, see below
    - For motion streams, a mapping from either `accel` or `gyro` to an array of float values conforming to:
      ```C++
      struct rs2_motion_device_intrinsic
      {
          // Scale X       cross axis  cross axis  Bias X
          // cross axis    Scale Y     cross axis  Bias Y
          // cross axis    cross axis  Scale Z     Bias Z
          float data[3][4];

          // Variance of noise for X, Y, and Z axis
          float noise_variances[3];  

          // Variance of bias for X, Y, and Z axis
          float bias_variances[3];   
      }
      ```
- `options` is an array of option objects, same as `device-options` above; stream options are shown in the Viewer
- `embedded-filters` is an array of embedded filters, with name and options for each filter; all these are shown in the Viewer
E.g.:
  
```JSON
{
    "id": "stream-options",
    "stream-name": "Motion",
    "intrinsics": {
        "accel": [1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0],
        "gyro": [1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0]
    },
    "options": [],
    "embedded-filters": []
}
```

#### Video Stream Intrinsics

```JSON
{
    "id": "stream-options",
    "stream-name": "Depth",
    "intrinsics": {
        "width": 1280,
        "height": 720,
        "principal-point": [640.2379150390625,357.3431396484375],
        "focal-length": [631.3428955078125,631.3428955078125]
    },
    "options": [],
	"embedded-filters": [
        {
            "name": "Decimation Filter",
            "options": [
                ["Toggle",0,0,1,1,0,"Activate filter: 0:disable filter, 1:enable filter"],
                ["Magnitude",2,1,8,1,2,"How many pixels will be grouped into 1",["read-only"]]
            ]
        },
        {
            "name": "Temporal Filter",
            "options": [
                ["Toggle",0,0,1,1,0,"Activate filter: 0:disable filter, 1:enable filter"],
                ["Alpha",0.4,0,1,0.01,0.4,"The Alpha factor in an exponential moving average with Alpha=1 - no filter. Alpha = 0 - infinite filter",["float"]],
                ["Delta",20,1,100,1,20,"Step-size boundary. Establishes the threshold used to preserve surfaces (edges)",["float"]],
                ["Persistency",3,0,8,1,3,"Hole Filling policy"]
            ]
        }
    ]
}
```

Like extrinsics are used to communicate translation between different stream viewpoints, the intrinsics serve to transform 2D pixel values to 3D world coordinates. I.e., an RGB pixel has to be converted to a 3D point in space, then mapped to a 3D point from the viewpoint of the Depth stream, then transformed back into a 2D Depth pixel.

The intrinsics are communicated in an object, as shown above:
- A `width` and `height` are for the native stream resolution, as 16-bit integer values
- A `principal-point` defined as `[<x>,<y>]` floating point values
- A `focal-length`, also as `[<x>,<y>]` floats

A distortion model may be applied:
- The `model` would specify which model is to be used, with the default of `brown`
- The `coefficients` is an array of floating point values, the number and meaning which depend on the `model`
    - For `brown`, 5 points [k1, k2, p1, p2, k3] are needed

The coefficients are assumed 0 if not there, applying no un/distortion.

An additional `force-symmetry` boolean can be applied, and defaults to `false`.

The intrinsics are communicated for the native resolution the device chooses. Librealsense, or any client, will need to scale these to a target resolution.

