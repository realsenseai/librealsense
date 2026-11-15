// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rect.h"
#include "detection-distance.h"
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <imgui.h>

enum class object_type
{
    person = 0,
    face = 1,
    other = 2
};

std::string object_type_to_string( object_type type );

struct object_in_frame
{
    rs2::rect normalized_color_bbox;
    std::string name;
    float mean_depth;
    float metadata_depth;         // distance reported by the detection model (meters); 0 if unavailable
    int   score;                  // detection confidence, 0-100
    size_t id;
    object_type type = object_type::other;

    object_in_frame( size_t _id, std::string const & _name, rs2::rect _bbox_color, float _depth,
                     float _metadata_depth, int _score,
                     object_type _type = object_type::other )
        : normalized_color_bbox( _bbox_color )
        , name( _name )
        , mean_depth( _depth )
        , metadata_depth( _metadata_depth )
        , score( _score )
        , id( _id )
        , type( _type )
    {
    }
};


typedef std::vector< object_in_frame > objects_in_frame;


struct atomic_objects_in_frame : public objects_in_frame
{
    std::mutex mutex;
    bool sensor_is_on = true;
    std::chrono::steady_clock::time_point last_od_frame_time{};
    // Chosen with the OD sensor's Detection Distance combo and kept in step with the camera's control; read and written
    // on the UI thread only. A camera without the control keeps the default and shows the camera's distance.
    rs2::detection_distance_source distance_source = rs2::detection_distance_source::camera;
};
