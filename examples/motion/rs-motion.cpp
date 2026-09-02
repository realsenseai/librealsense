// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
#include <librealsense2/rs.hpp>
#include <mutex>
#include <iostream>
#include "example.hpp"          // Include short list of convenience functions for rendering
#include <cstring>

struct short3
{
    uint16_t x, y, z;
};

#include "d435.h"

void draw_axes()
{
    glLineWidth(2);
    glBegin(GL_LINES);
    // Draw x, y, z axes
    glColor3f(1, 0, 0); glVertex3f(0, 0, 0);  glVertex3f(-1, 0, 0);
    glColor3f(0, 1, 0); glVertex3f(0, 0, 0);  glVertex3f(0, -1, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0, 0);  glVertex3f(0, 0, 1);
    glEnd();

    glLineWidth(1);
}

void draw_floor()
{
    glBegin(GL_LINES);
    glColor4f(0.4f, 0.4f, 0.4f, 1.f);
    // Render "floor" grid
    for (int i = 0; i <= 8; i++)
    {
        glVertex3i(i - 4, 1, 0);
        glVertex3i(i - 4, 1, 8);
        glVertex3i(-4, 1, i);
        glVertex3i(4, 1, i);
    }
    glEnd();
}

void render_scene(glfw_state app_state)
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glColor3f(1.0, 1.0, 1.0);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 4.0 / 3.0, 1, 40);

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    gluLookAt(1, 0, 5, 1, 0, 0, 0, -1, 0);

    glTranslatef(0, 0, +0.5f + app_state.offset_y*0.05f);
    glRotated(app_state.pitch, -1, 0, 0);
    glRotated(app_state.yaw, 0, 1, 0);
    draw_floor();
}

class camera_renderer
{
    std::vector<float3> positions, normals;
    std::vector<short3> indexes;
public:
    // Initialize renderer with data needed to draw the camera
    camera_renderer()
    {
        uncompress_d435_obj(positions, normals, indexes);
    }

    // Takes the calculated angle as input and rotates the 3D camera model accordignly
    void render_camera(float3 theta)
    {

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        glPushMatrix();
        // Set the rotation, converting theta to degrees
        glRotatef(theta.x * 180 / PI_FL, 0, 0, -1);
        glRotatef(theta.y * 180 / PI_FL, 0, -1, 0);
        glRotatef((theta.z - PI_FL / 2) * 180 / PI_FL, -1, 0, 0);

        draw_axes();

        // Scale camera drawing
        glScalef(0.035f, 0.035f, 0.035f);

        glBegin(GL_TRIANGLES);
        // Draw the camera
        for (auto& i : indexes)
        {
            glVertex3fv(&positions[i.x].x);
            glVertex3fv(&positions[i.y].x);
            glVertex3fv(&positions[i.z].x);
            glColor4f(0.05f, 0.05f, 0.05f, 0.3f);
        }
        glEnd();

        glPopMatrix();

        glDisable(GL_BLEND);
        glFlush();
    }

};

class rotation_estimator
{
    // theta is the angle of camera rotation in x, y and z components
    float3 theta;
    std::mutex theta_mtx;
    /* alpha indicates the part that gyro and accelerometer take in computation of theta; higher alpha gives more weight to gyro, but too high
    values cause drift; lower alpha gives more weight to accelerometer, which is more sensitive to disturbances */
    float alpha = 0.98f;
    bool firstGyro = true;
    bool firstAccel = true;
    // Keeps the arrival time of previous gyro frame
    double last_ts_gyro = 0;
public:
    // Function to calculate the change in angle of motion based on data from gyro
    void process_gyro(rs2_vector gyro_data, double ts)
    {
        if (firstGyro) // On the first iteration, use only data from accelerometer to set the camera's initial position
        {
            firstGyro = false;
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

        // Change in angle equals gyro measures * time passed since last measurement
        gyro_angle = gyro_angle * static_cast<float>(dt_gyro);

        // Apply the calculated change of angle to the current angle (theta)
        std::lock_guard<std::mutex> lock(theta_mtx);
        theta.add(-gyro_angle.z, -gyro_angle.y, gyro_angle.x);
    }

    void process_accel(rs2_vector accel_data)
    {
        // Holds the angle as calculated from accelerometer data
        float3 accel_angle;

        // Calculate rotation angle from accelerometer data
        accel_angle.z = atan2(accel_data.y, accel_data.z);
        accel_angle.x = atan2(accel_data.x, sqrt(accel_data.y * accel_data.y + accel_data.z * accel_data.z));

        // If it is the first iteration, set initial pose of camera according to accelerometer data (note the different handling for Y axis)
        std::lock_guard<std::mutex> lock(theta_mtx);
        if (firstAccel)
        {
            firstAccel = false;
            theta = accel_angle;
            // Since we can't infer the angle around Y axis using accelerometer data, we'll use PI as a convetion for the initial pose
            theta.y = PI_FL;
        }
        else
        {
            /* 
            Apply Complementary Filter:
                - high-pass filter = theta * alpha:  allows short-duration signals to pass through while filtering out signals
                  that are steady over time, is used to cancel out drift.
                - low-pass filter = accel * (1- alpha): lets through long term changes, filtering out short term fluctuations 
            */
            theta.x = theta.x * alpha + accel_angle.x * (1 - alpha);
            theta.z = theta.z * alpha + accel_angle.z * (1 - alpha);
        }
    }
    
    // Returns the current rotation angle
    float3 get_theta()
    {
        std::lock_guard<std::mutex> lock(theta_mtx);
        return theta;
    }
};

enum class imu_mode
{
    none,
    split_accel_gyro,
    combined_motion
};

imu_mode detect_imu_mode( rs2::device_list const & devices )
{
    for( auto dev : devices )
    {
        bool found_gyro = false;
        bool found_accel = false;
        bool found_combined = false;

        for( auto sensor : dev.query_sensors() )
        {
            for( auto profile : sensor.get_stream_profiles() )
            {
                if( profile.stream_type() == RS2_STREAM_GYRO )
                    found_gyro = true;
                if( profile.stream_type() == RS2_STREAM_ACCEL )
                    found_accel = true;
                if( profile.stream_type() == RS2_STREAM_MOTION
                    && profile.format() == RS2_FORMAT_COMBINED_MOTION )
                    found_combined = true;
            }
        }
        if( found_combined )
            return imu_mode::combined_motion;
        if( found_gyro && found_accel )
            return imu_mode::split_accel_gyro;
    }
    return imu_mode::none;
}

template< class T >
rs2_vector to_rs2_vector( T const & value )
{
    return { static_cast< float >( value.x ),
             static_cast< float >( value.y ),
             static_cast< float >( value.z ) };
}

int main(int argc, char * argv[]) try
{
    rs2::context ctx;
    auto mode = detect_imu_mode( ctx.query_devices() );
    if( mode == imu_mode::none )
    {
        std::cerr << "Device supporting IMU not found\n";
        return EXIT_FAILURE;
    }

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe( ctx );
    // Create a configuration for configuring the pipeline with a non default profile
    rs2::config cfg;

    if( mode == imu_mode::split_accel_gyro )
    {
        // Add streams of gyro and accelerometer to configuration
        cfg.enable_stream(RS2_STREAM_ACCEL, RS2_FORMAT_MOTION_XYZ32F);
        cfg.enable_stream(RS2_STREAM_GYRO, RS2_FORMAT_MOTION_XYZ32F);
    }
    else
    {
        // Add combined gyro and accelerometer to configuration
        cfg.enable_stream(RS2_STREAM_MOTION, RS2_FORMAT_COMBINED_MOTION);
    }

    // Initialize window for rendering
    window app(1280, 720, "RealSense Motion Example");
    // Construct an object to manage view state
    glfw_state app_state(0.0, 0.0);
    // Register callbacks to allow manipulation of the view state
    register_glfw_callbacks(app, app_state);

    // Declare object for rendering camera motion
    camera_renderer camera;
    // Declare object that handles camera pose calculations
    rotation_estimator algo;

    // Start streaming with the given configuration;
    // Note that since we only allow IMU streams, only single frames are produced
    auto profile = pipe.start(cfg, [&](rs2::frame frame)
    {
        // Cast the frame that arrived to motion frame
        auto motion = frame.as<rs2::motion_frame>();
        // If casting succeeded and the arrived frame is from gyro stream
        if (motion && motion.get_profile().stream_type() == RS2_STREAM_GYRO && motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F)
        {
            // Get the timestamp of the current frame
            double ts = motion.get_timestamp();
            // Get gyro measures
            rs2_vector gyro_data = motion.get_motion_data();
            // Call function that computes the angle of motion based on the retrieved measures
            algo.process_gyro(gyro_data, ts);
        }
        // If casting succeeded and the arrived frame is from accelerometer stream
        if (motion && motion.get_profile().stream_type() == RS2_STREAM_ACCEL && motion.get_profile().format() == RS2_FORMAT_MOTION_XYZ32F)
        {
            // Get accelerometer measures
            rs2_vector accel_data = motion.get_motion_data();
            // Call function that computes the angle of motion based on the retrieved measures
            algo.process_accel(accel_data);
        }
        if (motion && motion.get_profile().stream_type() == RS2_STREAM_MOTION && motion.get_profile().format() == RS2_FORMAT_COMBINED_MOTION)
        {
            auto combined = motion.get_combined_motion_data();
            algo.process_gyro(to_rs2_vector(combined.angular_velocity), motion.get_timestamp());
            algo.process_accel(to_rs2_vector(combined.linear_acceleration));
        }
    });

    // Main loop
    while (app)
    {
        // Configure scene, draw floor, handle manipultation by the user etc.
        render_scene(app_state);
        // Draw the camera according to the computed theta
        camera.render_camera(algo.get_theta());
    }
    // Stop the pipeline
    pipe.stop();

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
