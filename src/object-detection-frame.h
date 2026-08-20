// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "frame.h"
#include "perception-frame.h"
#include "core/extension.h"
#include <librealsense2/h/rs_types.h>
#include <rsutils/string/from.h>
#include <atomic>
#include <cstddef>

namespace librealsense {

class object_detection_frame : public perception_frame
{
public:
    // Frames received over the object detection stream are binary blobs with a versioned ODET layout.

    static constexpr uint32_t MAGIC_NUMBER = 0x5445444F;  // ASCII "ODET" as a little-endian uint32
    static constexpr uint16_t VERSION_V2 = 0x0200;
    static constexpr uint16_t VERSION_V3 = 0x0300;
    static constexpr uint32_t MAX_DETECTIONS = 64;

    enum class source : uint8_t
    {
        RGB = 0,
        DEPTH = 1
    };

#pragma pack( push, 1 )
    struct object_detection_frame_header
    {
        uint32_t magic_number;
        uint16_t version;
        uint8_t data_type;
        uint8_t flags;
        uint32_t size;
        uint32_t spare;
        uint32_t crc32;
    };

    struct object_detection_payload_header
    {
        double timestamp_ms;
        uint64_t frame_id;
        uint16_t number_of_detections;
        uint8_t source;
        uint32_t source_frame_id;
    };

    struct object_detection_entry_v2
    {
        uint16_t detection_id;
        uint8_t detection_type;
        uint8_t confidence;
        uint16_t top_left_x;
        uint16_t top_left_y;
        uint16_t bottom_right_x;
        uint16_t bottom_right_y;
        float distance;
    };

    struct object_detection_entry_v3
    {
        object_detection_entry_v2 detection;
        float world_x;
        float world_y;
        float world_z;
        float image_x;
        float image_y;
    };
#pragma pack( pop )

    struct object_detection_entry
    {
        uint16_t detection_id = 0;
        uint8_t detection_type = 0;
        uint8_t confidence = 0;
        uint16_t top_left_x = 0;
        uint16_t top_left_y = 0;
        uint16_t bottom_right_x = 0;
        uint16_t bottom_right_y = 0;
        float distance = 0.f;
        rs2_vector world_position = {};
        float image_x = 0.f;
        float image_y = 0.f;
        bool com_valid = false;
    };

    static constexpr size_t FRAME_HEADER_SIZE = sizeof( object_detection_frame_header );
    static constexpr size_t PAYLOAD_HEADER_SIZE = sizeof( object_detection_payload_header );
    static constexpr size_t V2_ENTRY_SIZE = sizeof( object_detection_entry_v2 );
    static constexpr size_t V3_ENTRY_SIZE = sizeof( object_detection_entry_v3 );
    static constexpr size_t MIN_FRAME_SIZE = FRAME_HEADER_SIZE + PAYLOAD_HEADER_SIZE;

    static_assert( FRAME_HEADER_SIZE == 20, "Object Detection frame header ABI must be 20 bytes" );
    static_assert( PAYLOAD_HEADER_SIZE == 23, "Object Detection payload header ABI must be 23 bytes" );
    static_assert( V2_ENTRY_SIZE == 16, "Object Detection v2 entry ABI must be 16 bytes" );
    static_assert( V3_ENTRY_SIZE == 36, "Object Detection v3 entry ABI must be 36 bytes" );
    static_assert( MIN_FRAME_SIZE == 43, "Object Detection minimum frame ABI must be 43 bytes" );

    object_detection_frame() = default;
    object_detection_frame( object_detection_frame && other );
    object_detection_frame & operator=( object_detection_frame && other );

    size_t get_detection_count() const;
    object_detection_entry get_detection( size_t index ) const;
    object_detection_payload_header get_payload_header() const;
    uint16_t get_version() const;

private:
    bool validate() const;
    bool validate_payload() const;
    size_t entry_size() const;

    mutable std::atomic_bool _validated{ false };
};

MAP_EXTENSION(RS2_EXTENSION_OBJECT_DETECTION_FRAME, librealsense::object_detection_frame);

}  // namespace librealsense
