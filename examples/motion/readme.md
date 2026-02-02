# rs-motion Sample

> In order to run this example, a device supporting IMU (D435i) is required.

## Overview
This sample demonstrates how to use data from gyroscope and accelerometer to compute the rotation ([Euler Angles](https://en.wikipedia.org/wiki/Euler_angles)) of the camera, denoted by `theta`.

> The example is based on [code](https://github.com/GruffyPuffy/imutest) by @GruffyPuffy.

In this example, we use complemetary filter to aggregate data from gyroscope and accelerometer. For more information, you can look at this [tutorial](http://www.pieter-jan.com/node/11) by Pieter-Jan or this [presentation](https://github.com/jcarrus/MakeMITSelfBalancingRobot/blob/master/segspecs/filter.pdf) by Shane Colton, among other resources available online.

## Expected Output
![expected output](https://raw.githubusercontent.com/wiki/realsenseai/librealsense/res/imu.gif)

The application should open a window with a 3D model of the camera, approximating the physical orientation. In addition, you should be able to interact with the camera using your mouse, for rotating, zooming, and panning.

## Code Overview

First, we include the RealSense™ Cross-Platform API.  
All but advanced functionality is provided through a single header:
```cpp
#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
```

In this example we will also use the auxiliary library of `example.hpp`:
```cpp
#include "../example.hpp"    
```

We define 3 auxilary functions: `draw_axes` and `draw_floor`, which draw axes and floor grid respectively; and `render_scene` which prepares the general set-up of the scence, handles user manipulation etc.

Rendering the 3D model of the camera is encapsulated inside `camera_renderer` class. It holds vertexes of the camera model and applies rotation according to `theta`.

Calculation of the rotation angle, based on gyroscope and accelerometer data, is handled by the class `rotation_estimator`.
The class holds `theta` itself, which is updated on each IMU frame. We will process IMU frames asynchronously and therefore a mutex `theta_mtx` is needed ([learn more](https://github.com/realsenseai/librealsense/tree/master/examples/callback)).

`rotation_estimator` also holds the parameter `alpha`, which is used to combine gyroscope and accelerometer data in the calculation of `theta`.
The first iteration needs separate handling and therfore `first` is defined.
`last_ts_gyro` is needed to calculate the time elapsed between arrival of subsequent frames from gyroscope stream.
```cpp
// theta is the angle of camera rotation in x, y and z components
float3 theta;
std::mutex theta_mtx;
/* alpha indicates the part that gyro and accelerometer take in computation of theta; higher alpha gives more
weight to gyro, but too high values cause drift; lower alpha gives more weight to accelerometer, which is more
sensitive to disturbances */
float alpha = 0.98;
bool first = true;
// Keeps the arrival time of previous gyro frame
double last_ts_gyro = 0;
```


The function `process_gyro` computes the change in rotation angle, based on gyroscope measurements. It accepts `gyro_data`, an `rs2_vector` containing measurements retrieved from gyroscope, and `ts`, the timestamp of the current frame from gyroscope stream.

In order to compute the change in the direction of motion, we multiply gyroscope measurements by the length of the time period they correspond to (which equals the time passed since we processed the previous gyro frame). Then, we add or subtract from the current `theta` the retrieved angle difference. The sign is determined by the defined positive direction of the axis.

Note that gyroscope data is ignored until accelerometer data arrives, since the initial position is retrieved from the accelerometer (and afterwards, gyroscope allows us to calculate the direction of motion ).

```cpp
void process_gyro(rs2_vector gyro_data, double ts)
{
    if (first) // On the first iteration, use only data from accelerometer to set the camera's initial position
    {
        last_ts_gyro = ts;
        return;
    }
    // Holds the change in angle, as calculated from gyro
    float3 gyro_angle;

     // Initialize gyro_angle with data from gyro
    gyro_angle.x = gyro_data.x; // Pitch
    gyro_angle.y = gyro_data.y; // Yaw
    gyro_angle.z = gyro_data.z; // Roll

    // Compute the difference between arrival times of previous and current gyro frames
    double dt_gyro = (ts - last_ts_gyro) / 1000.0;
    last_ts_gyro = ts;

    // Change in angle equals gyro measurements * time passed since last measurement
    gyro_angle = gyro_angle * dt_gyro;

    // Apply the calculated change of angle to the current angle (theta)
    std::lock_guard<std::mutex> lock(theta_mtx);
	theta.add(-gyro_angle.z, -gyro_angle.y, gyro_angle.x);
}
```


The function `process_accel` computes the rotation angle from accelerometer data and updates the current `theta`. `accel_data` is an `rs2_vector` that holds the measurements retrieved from the accelerometer stream.

The angles in the z and x axis are computed from acclerometer data, using trigonometric calculations. Note that motion around Y axis cannot be estimated using accelerometer.
```cpp
void process_accel(rs2_vector accel_data)
{
    // Holds the angle as calculated from accelerometer data
    float3 accel_angle;
    // Calculate rotation angle from accelerometer data
    accel_angle.z = atan2(accel_data.y, accel_data.z);
    accel_angle.x = atan2(accel_data.x, sqrt(accel_data.y * accel_data.y + accel_data.z * accel_data.z));
```


If it is the first time we handle accelerometer data, we intiailize `theta` with the angles computed above. `theta` in Y direction is set to `PI` (180 degrees), facing the user.
```cpp
    // If it is the first iteration, set initial pose of camera according to accelerometer data
    // (note the different handling for Y axis)
    std::lock_guard<std::mutex> lock(theta_mtx);
    if (first)
    {
        first = false;
        theta = accel_angle;
        // Since we can't infer the angle around Y axis using accelerometer data, we'll use PI as a convetion
        // for the initial pose
        theta.y = PI;
    }
```


Otherwise, we use an approximate version of Complementary Filter to balance gyroscope and accelerometer results. Gyroscope gives generally accurate motion data, but it tends to drift, not returning to zero when the system goes back to its original position. Accelerometer, on the other hand, doesn't have this problem, but its signals are not as smooth and are easily affected by noises and disturbances. We use `alpha` to aggregate the data. New `theta` is the combination of the previous angle adjusted by difference computed from gyroscope data, and the angle calculated from accelerometer data. Note that this calculation does not apply to the Y component of `theta`, since it is measured by gyroscope only.
```cpp
        else
        {
            /* 
            Apply Complementary Filter:
                - "high-pass filter" = theta * alpha:  allows short-duration signals to pass through while
                  filtering out signals that are steady over time, is used to cancel out drift.
                - "low-pass filter" = accel * (1- alpha): lets through long term changes, filtering out short
                  term fluctuations 
            */
            theta.x = theta.x * alpha + accel_angle.x * (1 - alpha);
            theta.z = theta.z * alpha + accel_angle.z * (1 - alpha);
        }
    }
```


The main function handels frames arriving from IMU streams.

First, we check that a device that supports IMU is connected by calling the function `check_imu_is_supported`. This function queries all sensors from all devices and checks if their profiles support IMU streams. If a device that supports the streams `RS2_STREAM_GYRO` and `RS2_STREAM_ACCEL` is not found, we return an error code.

If the check passed successfully, we start the example. We declare the pipeline and configure it with `RS2_STREAM_ACCEL` and `RS2_STREAM_GYRO`.
```cpp
// Declare RealSense pipeline, encapsulating the actual device and sensors
rs2::pipeline pipe;
// Create a configuration for configuring the pipeline with a non default profile
rs2::config cfg;

// Add streams of gyro and accelerometer to configuration
cfg.enable_stream(RS2_STREAM_ACCEL, RS2_FORMAT_MOTION_XYZ32F);
cfg.enable_stream(RS2_STREAM_GYRO,  RS2_FORMAT_MOTION_XYZ32F);
```

Next, we declare objects of the helper classes we defined above.
```cpp
// Declare object for rendering camera motion
camera_renderer camera;
// Declare object that handles camera pose calculations
rotation_estimator algo;
```


To process frames from gyroscope and accelerometer streams asynchronously, we'll use pipeline's callback. This will ensure minimal latency. When a frame arrives, we cast it to `rs2::motion_frame`. If it's a gyro frame, we also find its timestamp using `motion.get_timestamp()`. Then, we can get the IMU data and call the corresponding function to calculate rotation angle.
```cpp
auto profile = pipe.start(cfg, [&](rs2::frame frame)
{
    // Cast the frame that arrived to motion frame
    auto motion = frame.as<rs2::motion_frame>();
    // If casting succeeded and the arrived frame is from gyro stream
    if (motion && motion.get_profile().stream_type() == RS2_STREAM_GYRO && 
    	motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F)
    {
        // Get the timestamp of the current frame
        double ts = motion.get_timestamp();
        // Get gyro measurements
        rs2_vector gyro_data = motion.get_motion_data();
        // Call function that computes the angle of motion based on the retrieved data
        algo.process_gyro(gyro_data, ts);
    }
    // If casting succeeded and the arrived frame is from accelerometer stream
    if (motion && motion.get_profile().stream_type() == RS2_STREAM_ACCEL && 
    	motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F)
    {
        // Get accelerometer measurements
        rs2_vector accel_data = motion.get_motion_data();
        // Call function that computes the angle of motion based on the retrieved data
        algo.process_accel(accel_data);
    }
});
```


The main loop renders the camera model, retrieving the current `theta` from the `algo` object in each iteration.
```cpp
while (app)
{
    // Configure scene, draw floor, handle manipultation by the user etc.
    render_scene(app_state);
    // Draw the camera according to the computed theta
    camera.render_camera(algo.get_theta());
}
```
