
This document describes the functionality and limitations of frame buffering management. The SDK API provides a platform for defining how to manage the frames traffic that are sent by the librealsense library while streaming. 

There are two main methods for frame buffering management: 

 - **Asynchronous method** - The librealsense library will pass each frame that is ready to be handled to a user pre-defined callback.
 - **Synchronous method** - This method (described below), instead of handling the frames immediately in a callback, the received frames are queued in a frame queue and can be polled by the application.

[For more information on the librealsense SDK architecture](https://github.com/realsenseai/librealsense/blob/development/doc/api_arch.md). For frame ownership, threading and the frame syncer see [Frame Management](https://github.com/realsenseai/librealsense/blob/development/doc/frame_lifetime.md).

## Table of Contents
* [Latency vs. Performance](#latency-vs.-performance)
* [Frame Buffering examples in the SDK](#frame-buffering-examples-in-the-sdk)
  * [frame_queue](#frame_queue)
  * [syncer](#syncer)
  * [pipeline](#pipeline)


## Latency vs. Performance
The RealSense ASIC does not have an internal full-frame buffer, so most latency in the system will come from system level buffering done in drivers or software. When setting the frame queue capacity the trade-off between latency and performance (FPS) must be taken into consideration.

Latency is always an important consideration for live-capture systems. To minimize latency in the SDK when streaming is configured, it is best to set the queue size (the “capacity” parameter) to 1 if enabling depth-only, and 2 if enabling both depth and color streams.

## Frame Buffering examples in the SDK

### frame_queue
Frame queues are the simplest x-platform synchronization primitive provided by SDK to help developers who are not using asynchronous APIs.

    const int CAPACITY = 10;
    // Open sensor for exclusive access, by committing to a configuration
    sensor.open(stream_profile);
    
    //Creating frame queue and setting the queue capacity
    rs2::frame_queue fq(CAPACITY);
    
    //In order to begin getting data from the sensor, we need to register a class to handle frames, 
    // in our case we provide the frame_queue when starting the sensor.
    sensor.start(fq);
    
    // “wait for frame” command, it is a blocking call.
    // It will send data as soon as it becomes available, and will block until then (by default up to 5000 ms, then it throws).
    rs2::frame f = fq.wait_for_frame();
    // Do something with the received frame
    
    // To stop streaming, we simply need to call the sensor's stop method
    // After returning from the call to stop(), no frames will arrive from this sensor
    sensor.stop();
    
    // To complete the stop operation, and release access of the device, we need to call close() per sensor
    sensor.close();

### syncer
For each frame the syncer searches for a matching frames and unites them to a composite frame, the resulted coherent frame sets are saved in a queue until the user calls the blocking call “wait for frames”.  The user may use the syncer as a synchronized platform, it implements the API that allows it to be passed to the sensor as a class to handle the upcoming frames.

    const int CAPACITY = 10;
    // Open color and depth sensors for exclusive access, by committing to a configuration
    depth_sensor.open(depth_stream);
    color_sensor.open(color_stream);
    
    // Create new syncer and set it’s capacity, by default the capacity is 1
    rs2::syncer sync(CAPACITY);
    
    //In order to begin getting data from the sensor, we need to register a class to handle frames, 
    //in our case we provide the syncer at the sensor initialization.
    depth_sensor.start(sync);
    color_sensor.start(sync);
    
    // Wait until receive a coherent set of frames
    rs2::frameset fset = sync.wait_for_frames();
    rs2::frame depth = fset.first_or_default(RS2_STREAM_DEPTH);
    rs2::frame color = fset.first_or_default(RS2_STREAM_COLOR);
    //do something with the frames
    
    // To stop streaming, we simply need to call the sensor's stop method
    // After returning from the call to stop(), no frames will arrive from this sensor
    depth_sensor.stop();
    color_sensor.stop();
    
    // To complete the stop operation, and release access of the device, we need to call close()
    //per sensor
    depth_sensor.close();
    color_sensor.close();

### pipeline
The pipeline simplifies the user interaction with the device and computer vision processing modules. The class abstracts the camera configuration and streaming, and the vision modules triggering and threading. It lets the application focus on the computer vision output of the modules, or the device output data. The pipeline can manage computer vision modules, which are implemented as a processing blocks. The pipeline is the consumer of the processing block interface, while the application consumes the computer vision interface. The pipeline is a synchronized platform, it contains a frame queue that buffers the received frames.

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;
    // Start streaming with default recommended configuration
    pipe.start();
    // Wait for next set of frames from the camera
    rs2::frameset data = pipe.wait_for_frames();
    // Find the depth data
    rs2::frame depth = color_map(data.get_depth_frame());
    // Find the color data
    rs2::frame color = data.get_color_frame();

The pipeline does not expose an API for setting its internal frame queue capacity; it is fixed at 1 so that `wait_for_frames()` returns the most recent coherent frameset (lowest latency, older framesets are dropped; when playing back a recording with real-time mode off (`rs2::playback::set_real_time(false)`), the pipeline does not drop framesets from this queue but waits until the previous one is read). If your application needs deeper buffering, use one of the following instead of changing the SDK source:

 - Feed the pipeline output into your own `rs2::frame_queue` with the desired capacity (see [API How-To: Do Processing on a Background-Thread](https://github.com/realsenseai/librealsense/wiki/API-How-To#do-processing-on-a-background-thread)).
 - Use the sensor API directly with a `frame_queue` or `syncer` of the desired capacity, as shown above.
 - Use the asynchronous callback API and buffer frames yourself.

<!-- wikigen:footer -->
_Maintained from librealsense `development`; last checked at commit [`84e42c1`](https://github.com/realsenseai/librealsense/commit/84e42c1d2ce717c57dcf11f155bbe68bb41bbd1c)._
<!-- /wikigen:footer -->
