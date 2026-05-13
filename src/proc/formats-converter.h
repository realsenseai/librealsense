// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "processing-blocks-factory.h"

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>

namespace librealsense
{
    // Converts frames from camera formats to other (user requested) formats
    // Terminology, since `profiles` are used for many different meanings
    // 1. Camera outputs `raw profiles`
    // 2. Converters (processing blocks) are able to convert `source profiles` to `target profiles`
    // 3. User requests to convert `from profiles` into `to profiles`
    // Sometimes the meaning is similar but the struct internal fields are updated at different times

    class formats_converter
    {
    public:
        void register_converter( const std::vector< stream_profile > & source,
                                        const std::vector< stream_profile > & target,
                                        std::function< std::shared_ptr< processing_block >( void ) > generate_func );
        void register_converter( const processing_block_factory & pbf );
        void register_converters( const std::vector< processing_block_factory > & pbfs );
        void clear_registered_converters();

        // Don't convert to types other then the raw camera formats (use only identity formats)
        // Convert only interleaved formats (Y8I, Y12I), no colored infrared.
        void drop_non_basic_formats();

        stream_profiles get_all_possible_profiles( const stream_profiles & raw_profiles );
        void prepare_to_convert( stream_profiles to_profiles );

        stream_profiles get_active_source_profiles() const;
        std::vector< std::shared_ptr< processing_block > > get_active_converters() const;

        void set_frames_callback( rs2_frame_callback_sptr callback );
        rs2_frame_callback_sptr get_frames_callback() const { return _converted_frames_callback; }
        void convert_frame( frame_holder & f );

        stream_profiles const & get_source_profiles_from_target( std::shared_ptr< stream_profile_interface > const & target_profile ) const;

    protected:
        void clear_active_cache();
        void update_target_profiles_data( const stream_profiles & from_profiles );
        void cache_from_profiles( const stream_profiles & from_profiles );

        std::shared_ptr< stream_profile_interface > clone_profile( const std::shared_ptr< stream_profile_interface > & from_profile ) const;
        bool is_profile_in_list( const std::shared_ptr< stream_profile_interface > & profile, const stream_profiles & profiles ) const;

        std::pair< std::shared_ptr< processing_block_factory >, stream_profiles >
            find_pbf_matching_most_profiles( const stream_profiles & profiles );

        std::shared_ptr< stream_profile_interface > find_cached_profile_for_frame( const frame_interface * f );

        std::vector< std::shared_ptr< processing_block_factory > > _pb_factories;
        std::unordered_map< processing_block_factory *, stream_profiles > _pbf_supported_profiles;
        std::unordered_map< stream_profile, stream_profiles > _target_profiles_to_raw_profiles;

        std::unordered_map< std::shared_ptr< stream_profile_interface >,
                            std::unordered_set< std::shared_ptr< processing_block > > > _raw_profile_to_converters;
        std::unordered_map< rs2_format, stream_profiles > _format_mapping_to_from_profiles;

        rs2_frame_callback_sptr _converted_frames_callback;
    };
}
