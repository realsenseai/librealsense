// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/frame-additional-data.h"
#include "callback-invocation.h"


namespace librealsense
{
    class frame_interface;
    class sensor_interface;

    class archive_interface
    {
    public:
        virtual callback_invocation_holder begin_callback() = 0;

        virtual frame_interface* alloc_and_track(const size_t size, frame_additional_data && additional_data, bool requires_memory) = 0;

        virtual void reserve_frame( size_t size ) = 0;

        virtual std::shared_ptr<metadata_parser_map> get_md_parsers() const = 0;

        virtual std::shared_ptr< sensor_interface > get_sensor() const = 0;
        virtual void set_sensor( const std::weak_ptr< sensor_interface > & ) = 0;

        virtual void flush() = 0;

        virtual frame_interface* publish_frame(frame_interface* frame) = 0;
        virtual void unpublish_frame(frame_interface* frame) = 0;
        virtual void keep_frame(frame_interface* frame) = 0;
        virtual ~archive_interface() = default;
    };

    std::shared_ptr<archive_interface> make_archive(rs2_extension type,
        std::atomic<uint32_t>* in_max_frame_queue_size,
        std::shared_ptr<metadata_parser_map> parsers);

}
