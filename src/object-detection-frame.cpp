// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "object-detection-frame.h"
#include "librealsense-exception.h"
#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>

namespace librealsense
{

bool object_detection_frame::validate() const
{
    if( data.size() < sizeof( object_detection_frame_header ) )
        return false;

    const object_detection_payload * payload = reinterpret_cast< const object_detection_payload * >( data.data() );

    if( payload->header.magic_number != MAGIC_NUMBER )
        return false;

    if( payload->header.data_type != static_cast< uint8_t >( inference_frame::type::OBJECT_DETECTION ) )
    {
        LOG_WARNING( "Unsupported Object Detection data_type: " << payload->header.data_type );
        return false;
    }

    uint16_t n = payload->number_of_detections;
    size_t expected_data_size_no_detections = sizeof( object_detection_payload ) - sizeof( object_detection_entry );
    size_t detections_size = sizeof( object_detection_entry ) * n;
    size_t expected_data_size_with_detections = expected_data_size_no_detections + detections_size;
    size_t expected_size_field = expected_data_size_with_detections - sizeof( object_detection_frame_header );

    if( data.size() != expected_data_size_with_detections || payload->header.size != expected_size_field )
    {
        LOG_WARNING( "Object Detection frame size mismatch: got " << data.size() << ", expected " << expected_data_size_with_detections <<
                     ", header size field: " << payload->header.size << ", expected size field: " << expected_size_field );
        return false;
    }

    return true;
}

size_t object_detection_frame::get_detection_count() const
{
    if( validate() )
        return reinterpret_cast< const object_detection_payload * >( data.data() )->number_of_detections;

    return 0;
}

object_detection_frame::object_detection_entry object_detection_frame::get_detection( size_t index ) const
{
    size_t count = get_detection_count(); // Validates frame as well
    if( index >= count )
        throw std::out_of_range(
            rsutils::string::from() << "Detection index " << index << " is out of range (count=" << count << ")" );
    return reinterpret_cast< const object_detection_payload * >( data.data() )->detections[index];
}

}  // namespace librealsense
