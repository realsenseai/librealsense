// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "object-detection-frame.h"
#include "librealsense-exception.h"
#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>

namespace librealsense
{

bool object_detection_frame::validate() const
{
    size_t const base_size = sizeof( object_detection_frame_header )
                           + sizeof( object_detection_payload_header );
    if( data.size() < base_size )
        return false;

    auto const * header = reinterpret_cast< const object_detection_frame_header * >( data.data() );

    if( header->magic_number != MAGIC_NUMBER )
        return false;

    if( header->data_type != static_cast< uint8_t >( perception_frame::type::OBJECT_DETECTION ) )
    {
        LOG_WARNING( "Unsupported Object Detection data_type: " << header->data_type );
        return false;
    }

    size_t const wire_entry_size = entry_size();
    if( ! wire_entry_size )
    {
        LOG_WARNING( "Unsupported Object Detection frame version: 0x" << std::hex << header->version );
        return false;
    }

    auto const * payload = reinterpret_cast< const object_detection_payload_header * >(
        data.data() + sizeof( object_detection_frame_header ) );
    size_t const detections_size = wire_entry_size * payload->number_of_detections;
    if( payload->number_of_detections && detections_size / wire_entry_size != payload->number_of_detections )
        return false;
    size_t const expected_size = base_size + detections_size;
    size_t const expected_size_field = expected_size - sizeof( object_detection_frame_header );

    // UVC transports fixed-size frames and may append padding. The header size
    // identifies the valid payload and must match the versioned entry count.
    if( data.size() < expected_size || header->size != expected_size_field )
    {
        LOG_WARNING( "Object Detection frame size mismatch: got " << data.size() << ", expected at least " << expected_size
                     << ", header size field: " << header->size << ", expected size field: " << expected_size_field );
        return false;
    }

    auto const * payload_data = data.data() + sizeof( object_detection_frame_header );
    auto const crc = rsutils::number::calc_crc32( payload_data, header->size );
    if( crc != header->crc32 )
    {
        LOG_WARNING( "Object Detection frame CRC mismatch: got 0x" << std::hex << header->crc32
                     << ", expected 0x" << crc );
        return false;
    }

    return true;
}

size_t object_detection_frame::get_detection_count() const
{
    if( validate() )
        return get_payload_header().number_of_detections;

    return 0;
}

object_detection_frame::object_detection_entry object_detection_frame::get_detection( size_t index ) const
{
    size_t count = get_detection_count(); // Validates frame as well
    if( index >= count )
        throw std::out_of_range(
            rsutils::string::from() << "Detection index " << index << " is out of range (count=" << count << ")" );
    auto const * header = reinterpret_cast< const object_detection_frame_header * >( data.data() );
    auto const * entries = data.data() + sizeof( object_detection_frame_header )
                        + sizeof( object_detection_payload_header );
    object_detection_entry result;
    if( header->version == VERSION_V2 )
    {
        auto const & wire = reinterpret_cast< const object_detection_entry_v2 * >( entries )[index];
        result.detection_id = wire.detection_id;
        result.detection_type = wire.detection_type;
        result.confidence = wire.confidence;
        result.top_left_x = wire.top_left_x;
        result.top_left_y = wire.top_left_y;
        result.bottom_right_x = wire.bottom_right_x;
        result.bottom_right_y = wire.bottom_right_y;
        result.distance = wire.distance;
    }
    else
    {
        auto const & wire = reinterpret_cast< const object_detection_entry_v3 * >( entries )[index];
        result.detection_id = wire.detection.detection_id;
        result.detection_type = wire.detection.detection_type;
        result.confidence = wire.detection.confidence;
        result.top_left_x = wire.detection.top_left_x;
        result.top_left_y = wire.detection.top_left_y;
        result.bottom_right_x = wire.detection.bottom_right_x;
        result.bottom_right_y = wire.detection.bottom_right_y;
        result.distance = wire.detection.distance;
        result.world_position = { wire.world_x, wire.world_y, wire.world_z };
        result.image_x = wire.image_x;
        result.image_y = wire.image_y;
        result.com_valid = wire.detection.distance > 0.f && wire.world_z > 0.f;
    }
    return result;
}

object_detection_frame::object_detection_payload_header object_detection_frame::get_payload_header() const
{
    if( data.size() < sizeof( object_detection_frame_header ) + sizeof( object_detection_payload_header ) )
        throw invalid_value_exception( "Object Detection frame is too small" );
    return *reinterpret_cast< const object_detection_payload_header * >(
        data.data() + sizeof( object_detection_frame_header ) );
}

uint16_t object_detection_frame::get_version() const
{
    if( data.size() < sizeof( object_detection_frame_header ) )
        return 0;
    return reinterpret_cast< const object_detection_frame_header * >( data.data() )->version;
}

size_t object_detection_frame::entry_size() const
{
    switch( get_version() )
    {
    case VERSION_V2:
        return sizeof( object_detection_entry_v2 );
    case VERSION_V3:
        return sizeof( object_detection_entry_v3 );
    default:
        return 0;
    }
}

}  // namespace librealsense
