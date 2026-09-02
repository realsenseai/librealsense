// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_types.h>
#include <librealsense2/h/rs_sensor.h>

#include <vector>
#include <memory>


namespace librealsense {


class stream_interface;
class matcher;


// Builds the matcher a syncer uses to group frames from different streams into a frameset.
//
// Frames of streams produced within one sensor usually carry a common frame number and are matched exactly.
// Frames from different sensors share only a timestamp. Matching by timestamp is also the RS2_MATCHER_DEFAULT behaviour.
class matcher_factory
{
public:
    static std::shared_ptr< matcher > create( rs2_matchers matcher,
                                              std::vector< stream_interface * > const & profiles );

private:
    // A stream taking part in a matcher's group; a negative index means every stream of that type
    struct match_group
    {
        rs2_stream stream;
        int index;
    };

    // How frames are related: by the exact frame number, by the time they were taken, or not at all -
    // for streams that are never matched to anything and simply pass through
    enum class sync_by
    {
        frame_number,
        timestamp,
        nothing
    };

    static std::vector< stream_interface * > select_group( std::vector< match_group > const & group,
                                                           std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > relate( std::vector< stream_interface * > const & profiles, sync_by );
    static std::shared_ptr< matcher > match_sensor_profiles( std::vector< match_group > const & group,
                                                      sync_by,
                                                      std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > match_sensor_profiles_with_color( std::vector< match_group > const & group,
                                                                 sync_by,
                                                                 std::vector< stream_interface * > const & profiles );
    static std::shared_ptr< matcher > create_identity_matcher( stream_interface * profiles );
    static std::shared_ptr< matcher >
        create_composite_identity_matcher( std::vector< std::shared_ptr< matcher > > const & matchers );
    static std::shared_ptr< matcher >
        create_frame_number_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers );
    static std::shared_ptr< matcher >
        create_timestamp_composite_matcher( std::vector< std::shared_ptr< matcher > > const & matchers );
    // Take the streams of one kind out of 'profiles', leaving every other stream in 'rest'
    static std::vector< stream_interface * > separate_color( std::vector< stream_interface * > const & profiles,
                                                             std::vector< stream_interface * > & rest );
    static std::vector< stream_interface * > separate_perception( std::vector< stream_interface * > const & profiles,
                                                                  std::vector< stream_interface * > & rest );
};


}  // namespace librealsense