# Frame Management

librealsense2 provides flexible model for frame management and synchronization. The document will overview frame memory management, passing frames between threads and synchronization. 

## API Overview

The core C++ abstraction when dealing is the `rs2::frame` class and the `rs2::sensor::start` method. All other management and synchronization primitives can be derived from those two APIs. 
```cpp
/**
 * Start passing frames into user provided callback
 * \param[in] callback   Stream callback, can be any callable object accepting rs2::frame
 */
template<class T>
void start(T callback) const;
```
Once you call start, the library will start dispatching new frames from selected sensor into the callback you provided. 
The callback will be invoked from the same thread handling the low-level IO ensuring minimal latency. Any object implementing `void operator()(rs2::frame)` can be used as a callback. In particular, you can pass an anonymous function (lambda with capture) as the frame callback:
```cpp
sensor.start([](rs::frame f){
    std::cout << "This line be printed every frame!" << std::endl; 
}); 
```
As a side-note, `rs2::sensor::stop` will block until all pending callbacks return. This way within callback scope you can be sure the device object is available. 

## Frame Memory Management

`rs2::frame` is a smart reference to the underlying frame - as long as you hold ownership of the `rs2::frame` the underlying memory is exclusively yours and will not be modified or freed. 
* If no processing was necessary on the frame, `rs2::frame::get_data` will provide a direct pointer to the buffer provided by the underlying driver stack. No extra memory copies are performed in this case. 
* If some processing was required (for example, whenever you configure `RS2_FORMAT_RGB8` it is likely librealsense will do the conversion from `YUY` format internally) librealsense will store the processing output in an internal buffer, and `rs2::frame::get_data` will point to it. 
* You can extend the lifetime of the `rs2::frame` object by moving it out of the callback into some global, thread-safe, data structure. (See below) Moving `rs2::frame` does not involve a mem-copy of its content. 
* Except some initial stabilization period, librealsense ensures no heap allocations are being made when using frame callbacks. (This also applies to `rs2::frame_queue` but not to `rs2::syncer` primitive)
* If you are not releasing `rs2::frame` objects in less then the `1000 / fps` milliseconds, you will likely encounter frame drops. These events will be visible in the log, if you decrease the severity to DEBUG level. 

## Frame Copies

The following diagram specifies the frame flow in the system and indicates where and when the frame is being copied/reconstructed.



![](./img/frame_lifetime.png)

* First copy is a mandatory step in the SDK and it's purpose is passing the frame content ownership from the digital media controller (WMF/V4L2) into librealsense.

* Second reconstruction of the frame is optional and a subject of frame manipulation needed by the user, examples for it are: pixel format representation conversions and more.. (See some of the implemented filters [here](https://github.com/realsenseai/librealsense/blob/master/doc/post-processing-filters.md) )

  This filters / post processing blocks can be concatenated and each one will get the last processed frame as input and output a new frame.

## Frames and Threads

Callbacks are invoked from an internal thread to minimize latency. If you have a lot of processing to do, or simply want to handle the frame in your main event loop, librealsense provides `rs2::frame_queue` primitive to move frames from one thread to another in a thread-safe fashion:
```cpp
rs2::frame_queue q;

sensor.start([](rs2::frame f){
    q.enqueue(std::move(f)); // enqueue any new frames into q
});

while(true)
{
    rs2::frame f = q.wait_for_frame(); // wait until new frame is available and dequeue it
    // handle frames in the main event loop
}
```
Since `rs2::frame_queue` implements `operator()` you can also pass the queue directly to `start`:
```cpp
rs2::frame_queue q;
sensor.start(q);
```

## Frame-Drops vs. Latency

There are two common types of applications of the streaming API:
* Those who need the most relevant data as soon as possible (low latency) 
* Those who want all the data, but don't mind waiting for it (low frame-drops)

librealsense provides some degree of control over this trade-off using `RS2_OPTION_FRAMES_QUEUE_SIZE` option. If you increase this number, your application will consume more memory and some frames might potentially wait in line more time, but frame drops will be less likely to happen. On the flip side, if you decrease this number, you will get frames faster, but if new frame will arrive while you are busy it will get dropped. 

## Frame Syncer

Often the input to an image processing application is not simply a frame, but rather a coherent set of frames, preferably taken at the same time. librealsense provides `rs2::syncer` primitive to help with this problem:
```cpp
#define CAPACITY = 10
// Open color and depth sensors
depth_sensor.open();
color_sensor.open();

// Create new syncer and set it’s queue capacity. Default capacity is 1
rs2::syncer sync(CAPACITY);

// Start the sensors with syncer object
depth_sensor.start(sync);
color_sensor.start(sync);

while(true)
{
    auto frameset = sync.wait_for_frames(); // wait for a coherent set of frames
    for (auto&& frame : frameset)
    {
        // handle frame
    }
}
```
* In general, there is no guarantee on the quality of the temporal synchronization. 
* If hardware timestamps are available, librealsense will take advantage of them.
* If the device supports hardware sync, librealsense will try to take advantage of it if it's enabled, but will not implicitly enable it. 
* You can also use a single `rs2::syncer` to synchronize between devices, assuming it makes sense. 




