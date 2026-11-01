// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

namespace rs2
{
    // Where the distance shown on each object-detection box comes from
    enum class detection_distance_source
    {
        none,         // show no distance
        application,  // estimated by the application from the depth stream
        camera        // calculated by the camera
    };

    // The value the camera's Detection Distance control should hold for a source: only `camera` needs it on
    inline float detection_distance_control_value( detection_distance_source source )
    {
        return source == detection_distance_source::camera ? 1.f : 0.f;
    }

    // Keep the source in step with the camera's control, which another app, ROS or a FW reset can change: the camera
    // calculates only while the control is on, and Application is the one choice that leaves it off
    inline detection_distance_source follow_detection_distance_control( detection_distance_source source,
                                                                        float control_value )
    {
        if( control_value > 0.f )
            return detection_distance_source::camera;
        return source == detection_distance_source::application ? source : detection_distance_source::none;
    }

    // Whether the application has to estimate the distance itself
    inline bool needs_application_detection_distance( detection_distance_source source )
    {
        return source == detection_distance_source::application;
    }

    // The distance to show for a detection; 0 shows none
    inline float pick_detection_distance( detection_distance_source source, float camera_distance,
                                          float application_distance )
    {
        switch( source )
        {
        case detection_distance_source::none:
            return 0.f;
        case detection_distance_source::application:
            return application_distance;
        case detection_distance_source::camera:
            return camera_distance;
        }
        return 0.f;  // not reached; no default, so a new source is flagged by -Wswitch
    }
}
