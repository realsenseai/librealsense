// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "object-detection-frame.h"
#include "librealsense-exception.h"
#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <utility>

namespace librealsense
{

object_detection_frame::object_detection_frame( object_detection_frame && other )
    : perception_frame( std::move( other ) )
{
}

object_detection_frame & object_detection_frame::operator=( object_detection_frame && other )
{
    perception_frame::operator=( std::move( other ) );
    _validated = false;
    return *this;
}

bool object_detection_frame::validate() const
{
    if( _validated )
        return true;
    if( ! validate_payload() )
        return false;
    _validated = true;
    return true;
}

bool object_detection_frame::validate_payload() const
{
    if( data.size() < MIN_FRAME_SIZE )
        return false;

    auto const * header = reinterpret_cast< const object_detection_frame_header * >( data.data() );

    // The firmware ABI excludes the fixed frame header from CRC coverage. Validate its fields
    // independently; header.size never determines a memory-access or CRC bound.
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

    auto const * payload
        = reinterpret_cast< const object_detection_payload_header * >( data.data() + FRAME_HEADER_SIZE );

    uint16_t const n = payload->number_of_detections;
    if( n > MAX_DETECTIONS )
    {
        LOG_WARNING( "Object Detection count exceeds ABI maximum: " << n << " > " << MAX_DETECTIONS );
        return false;
    }

    size_t const detections_size = wire_entry_size * n;
    size_t const expected_size_field = PAYLOAD_HEADER_SIZE + detections_size;
    size_t const expected_data_size_with_detections = FRAME_HEADER_SIZE + expected_size_field;

    // data.size() may exceed the logical payload if the transport adds trailing padding. The header
    // declares the valid length, and the bounded detection count keeps every read within the buffer.
    if( data.size() < expected_data_size_with_detections || header->size != expected_size_field )
    {
        LOG_WARNING( "Object Detection frame size mismatch: got " << data.size() << ", expected at least " << expected_data_size_with_detections <<
                     ", header size field: " << header->size << ", expected size field: " << expected_size_field );
        return false;
    }

    auto const payload_data = data.data() + FRAME_HEADER_SIZE;
    auto const computed_crc32 = rsutils::number::calc_crc32( payload_data, expected_size_field );
    if( header->crc32 != computed_crc32 )
    {
        LOG_WARNING( "Object Detection CRC mismatch: got " << header->crc32
                     << ", expected " << computed_crc32 );
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

object_detection_frame::decoded_object_detection object_detection_frame::get_detection( size_t index ) const
{
    size_t count = get_detection_count(); // Validates frame as well
    if( index >= count )
        throw std::out_of_range(
            rsutils::string::from() << "Detection index " << index << " is out of range (count=" << count << ")" );

    auto const * header = reinterpret_cast< const object_detection_frame_header * >( data.data() );
    auto const * entries = data.data() + FRAME_HEADER_SIZE + PAYLOAD_HEADER_SIZE;

    decoded_object_detection result;
    auto const & wire = reinterpret_cast< const object_detection_payload_entry * >( entries )[index];
    result.detection_id = wire.detection_id;
    result.detection_type = wire.detection_type;
    result.confidence = wire.confidence;
    result.top_left_x = wire.top_left_x;
    result.top_left_y = wire.top_left_y;
    result.bottom_right_x = wire.bottom_right_x;
    result.bottom_right_y = wire.bottom_right_y;
    result.distance = wire.distance;
    result.world_position = { wire.world_x, wire.world_y, wire.world_z };
    result.image_x = wire.image_x;
    result.image_y = wire.image_y;
    // world_z is never negative; firmware uses exactly 0 to mean COM wasn't calculated.
    result.com_valid = wire.world_z > 0.f;
    return result;
}

object_detection_frame::object_detection_payload_header object_detection_frame::get_payload_header() const
{
    if( data.size() < FRAME_HEADER_SIZE + PAYLOAD_HEADER_SIZE )
        throw invalid_value_exception( "Object Detection frame is too small" );
    return *reinterpret_cast< const object_detection_payload_header * >( data.data() + FRAME_HEADER_SIZE );
}

uint16_t object_detection_frame::get_version() const
{
    if( data.size() < FRAME_HEADER_SIZE )
        return 0;
    return reinterpret_cast< const object_detection_frame_header * >( data.data() )->version;
}

size_t object_detection_frame::entry_size() const
{
    return get_version() == VERSION_V3 ? sizeof( object_detection_payload_entry ) : 0;
}

}  // namespace librealsense
