// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include "frame.h"
#include "inference-frame.h"
#include "core/extension.h"
#include <librealsense2/h/rs_types.h>
#include <rsutils/string/from.h>

namespace librealsense {

class object_detection_frame : public inference_frame
{
public:
    // Frames received over the object detection stream are binary blobs with object_detection_payload layout.

    static constexpr uint32_t MAGIC_NUMBER = 0x4F444554;  // "ODET"

    enum class source : uint8_t
    {
        RGB = 0,
        DEPTH = 1
    };

#pragma pack( push, 1 )
    struct object_detection_frame_header
    {
        uint32_t magic_number;  // Must equal OD_FRAME_MAGIC (0x4F444554, "ODET")
        uint16_t version;       // major.minor SDK/HKR API version
        uint8_t data_type;      // 0 = object detection
        uint8_t flags;
        uint32_t size;          // Expected frame size, header excluded
        uint32_t spare;
        uint32_t crc32;         // CRC of the data, header excluded
    };

    struct object_detection_entry
    {
        uint32_t detection_id;    // For detection/tracking traceability
        uint8_t detection_type;   // 0 = person
        uint8_t confidence;       // 0-100
        uint16_t top_left_x;      // Bounding box top-left X [pixels]
        uint16_t top_left_y;      // Bounding box top-left Y [pixels]
        uint16_t bottom_right_x;  // Bounding box bottom-right X [pixels]
        uint16_t bottom_right_y;  // Bounding box bottom-right Y [pixels]
        float distance;           // Object distance from camera [meters]
    };

    struct object_detection_payload
    {
        object_detection_frame_header header;
        double timestamp;          // Frame timestamp
        uint64_t frame_id;         // Frame counter
        uint16_t number_of_detections;
        uint8_t source;            // 0 = RGB, 1 = depth
        uint32_t source_frame_id;  // ID of the frame detection was calculated on
        object_detection_entry detections[1]; // `number_of_detections` entries of type `object_detection_entry`
    };
#pragma pack( pop )

    size_t get_detection_count() const;
    object_detection_entry get_detection( size_t index ) const;

private:
    bool validate() const;
};

MAP_EXTENSION(RS2_EXTENSION_OBJECT_DETECTION_FRAME, librealsense::object_detection_frame);

}  // namespace librealsense
