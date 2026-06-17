// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "rect.h"
#include <string>
#include <vector>
#include <mutex>
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
    rs2::rect normalized_color_bbox, normalized_depth_bbox;
    std::string name;
    float mean_depth;
    float metadata_depth;         // distance reported by the detection model (meters); 0 if unavailable
    float com_rel_u, com_rel_v;  // COM 2D point, [0,1] relative to the detection bbox; 0.5 if unavailable
    int   score;                  // detection confidence, 0-100
    size_t id;
    object_type type = object_type::other;

    object_in_frame( size_t _id, std::string const & _name, rs2::rect _bbox_color, rs2::rect _bbox_depth, float _depth,
                     float _metadata_depth, float _com_rel_u, float _com_rel_v, int _score,
                     object_type _type = object_type::other )
        : normalized_color_bbox( _bbox_color )
        , normalized_depth_bbox( _bbox_depth )
        , name( _name )
        , mean_depth( _depth )
        , metadata_depth( _metadata_depth )
        , com_rel_u( _com_rel_u )
        , com_rel_v( _com_rel_v )
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
};
