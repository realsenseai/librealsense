# rs-nn-load Tool

## Goal
`rs-nn-load` is a console application for uploading an NN model package from the Perception Service to a camera.

## Prerequisites
- A camera that accepts NN models (D585 over USB), with firmware that supports NN model upload.
- An unlocked camera: in this release, models can only be uploaded to unlocked cameras.
- The Perception sensor must not be streaming during the upload.

## Usage
Run `rs-nn-load -l` to list the connected cameras that accept NN models and whether upload is available on each:

```
Connected NN loader devices:
1) RealSense D585 s/n 541522320235, firmware version: 7.60.0.0, upload supported
```

Then upload a package with `rs-nn-load -f <package>`, adding `-s <serial_number>` when more than one such camera is connected:

```
Uploading DamoYolo.hkrp to RealSense D585 s/n 541522320235. Please don't disconnect the device!
NN model upload progress: 100[%]
NN model installed, waiting for the device to reconnect...
Device 541522320235 reconnected with the new NN model
```

When the install is done the camera resets and comes back with the new model. If the upload does not complete, wait a minute before reconnecting the camera, as it may still be writing the model, then upload again.

## Command Line Parameters

|Flag   |Description   |Default|
|---|---|---|
|`-l`|List the connected devices that accept NN models||
|`-f <path>`|Path of the NN model package||
|`-s <serial_number>`|The device to upload to, mandatory if more than one is connected||
|`--target <flash\|ddr>`|Where to store the model; only `flash` is supported|`flash`|
|`--slot <number>`|Model slot; only 0 is supported|0|
|`--no-activate`|Store the model without activating it; not supported yet||
