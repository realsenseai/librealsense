# rs-nn-load-example Sample

## Overview
This example uploads an NN model package from the Perception Service to a camera, waits for the camera to reset with the new model, then prints its object detections for 10 seconds.

## Supported Cameras
D585 and D585F over USB, unlocked, with firmware that supports NN model upload. Other cameras, including D555, D535 and D585S, and D585 over GMSL, don't support it. Run `rs-nn-load -l` to check a camera.

## Usage
```
rs-nn-load-example <model package>
```

## Code Overview
Find a camera that accepts NN models and on which upload is available. If there is none, `reason` says why:
```cpp
rs2::nn_loader loader;
std::string reason = "No camera that accepts NN models was found";
for( auto && dev : ctx.query_devices() )
{
    rs2::nn_loader candidate( dev );
    if( candidate && candidate.is_upload_supported( &reason ) )
    {
        loader = candidate;
        break;
    }
}
```

Upload the package. The call blocks until the model is installed, then the SDK resets the camera:
```cpp
loader.nn_upload( package, []( float progress ) { ... } );
```

The camera disconnects, so the example waits for a device with the same serial number to come back through `set_devices_changed_callback`, then streams Color and Object Detection with a `pipeline`, as in the [object-detection](../object-detection) example.
