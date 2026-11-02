// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include <unit-tests/catch.h>
#include <common/detection-distance.h>

using rs2::detection_distance_source;
using rs2::detection_distance_control_value;
using rs2::needs_application_detection_distance;
using rs2::pick_detection_distance;


TEST_CASE( "detection-distance/off shows no distance", "[detection-distance]" )
{
    // The reported bug: with the control off the FW returns 0, and the viewer must not fill in its own estimate
    CHECK_FALSE( needs_application_detection_distance( detection_distance_source::none ) );
    CHECK( pick_detection_distance( detection_distance_source::none, 0.f, 1.7f ) == 0.f );
    CHECK( pick_detection_distance( detection_distance_source::none, 2.f, 1.7f ) == 0.f );
    CHECK( detection_distance_control_value( detection_distance_source::none ) == 0.f );
}

TEST_CASE( "detection-distance/application uses its own estimate only", "[detection-distance]" )
{
    CHECK( needs_application_detection_distance( detection_distance_source::application ) );
    CHECK( pick_detection_distance( detection_distance_source::application, 0.f, 1.7f ) == 1.7f );
    CHECK( pick_detection_distance( detection_distance_source::application, 2.f, 1.7f ) == 1.7f );
    CHECK( pick_detection_distance( detection_distance_source::application, 0.f, 0.f ) == 0.f );  // no depth at the box
    CHECK( detection_distance_control_value( detection_distance_source::application ) == 0.f );
}

TEST_CASE( "detection-distance/camera uses the FW distance only", "[detection-distance]" )
{
    // Also the default, so a camera without the control (e.g. D555 over DDS) shows the FW distance only
    CHECK_FALSE( needs_application_detection_distance( detection_distance_source::camera ) );
    CHECK( pick_detection_distance( detection_distance_source::camera, 2.f, 1.7f ) == 2.f );
    CHECK( pick_detection_distance( detection_distance_source::camera, 0.f, 1.7f ) == 0.f );  // FW out of range
    CHECK( detection_distance_control_value( detection_distance_source::camera ) == 1.f );
}

TEST_CASE( "detection-distance/the source follows the camera's control", "[detection-distance]" )
{
    using rs2::follow_detection_distance_control;
    // Control turned on (by the combo, another app, ROS): the camera calculates, so show its distance
    CHECK( follow_detection_distance_control( detection_distance_source::none, 1.f ) == detection_distance_source::camera );
    CHECK( follow_detection_distance_control( detection_distance_source::application, 1.f )
           == detection_distance_source::camera );
    CHECK( follow_detection_distance_control( detection_distance_source::camera, 1.f ) == detection_distance_source::camera );
    // Control off: no camera distance; Application stays, since it is the one choice that keeps the control off
    CHECK( follow_detection_distance_control( detection_distance_source::camera, 0.f ) == detection_distance_source::none );
    CHECK( follow_detection_distance_control( detection_distance_source::none, 0.f ) == detection_distance_source::none );
    CHECK( follow_detection_distance_control( detection_distance_source::application, 0.f )
           == detection_distance_source::application );
}
