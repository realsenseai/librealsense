# rs-nn-load-example Sample

## Overview
This example uploads an NN model package from the Perception Service to a camera, waits for the camera to reset with the new model, then prints its object detections for 10 seconds.

It needs a camera that accepts NN models (D585 over USB), unlocked and with firmware that supports NN model upload.

## Usage
```
rs-nn-load-example <model package>
```

## Code Overview
Find a camera that accepts NN models, and check that upload is available on it:
```cpp
rs2::nn_loader loader;
for( auto && dev : ctx.query_devices() )
    if( ( loader = rs2::nn_loader( dev ) ) )
        break;
std::string reason;
if( ! loader.is_upload_supported( &reason ) )
    ...
```

Upload the package. The call blocks until the model is installed, then the SDK resets the camera:
```cpp
loader.nn_upload( package, []( float progress ) { ... } );
```

The camera disconnects, so the example waits for a device with the same serial number to come back through `set_devices_changed_callback`, then streams Color and Object Detection with a `pipeline`, as in the [object-detection](../object-detection) example.
