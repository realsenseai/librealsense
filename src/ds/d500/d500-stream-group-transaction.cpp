// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-stream-group-transaction.h"

#include <src/hw-monitor.h>
#include <src/librealsense-exception.h>

#include <utility>


namespace librealsense
{
    namespace
    {
        uint8_t const stream_group_opcode = 0xbd;
        uint32_t const prepare_command = 1;
        uint32_t const cancel_command = 2;
        uint32_t const query_command = 3;
        uint8_t const protocol_version = 1;
        size_t const prepare_header_size = 8;
        size_t const profile_entry_size = 12;
        size_t const status_size = 8;

        uint32_t make_fourcc( char a, char b, char c, char d )
        {
            return ( static_cast< uint32_t >( static_cast< uint8_t >( a ) ) << 24 )
                | ( static_cast< uint32_t >( static_cast< uint8_t >( b ) ) << 16 )
                | ( static_cast< uint32_t >( static_cast< uint8_t >( c ) ) << 8 )
                | static_cast< uint32_t >( static_cast< uint8_t >( d ) );
        }

        void append_u16_le( std::vector< uint8_t > & bytes, uint16_t value )
        {
            bytes.push_back( static_cast< uint8_t >( value ) );
            bytes.push_back( static_cast< uint8_t >( value >> 8 ) );
        }

        void append_u32_le( std::vector< uint8_t > & bytes, uint32_t value )
        {
            bytes.push_back( static_cast< uint8_t >( value ) );
            bytes.push_back( static_cast< uint8_t >( value >> 8 ) );
            bytes.push_back( static_cast< uint8_t >( value >> 16 ) );
            bytes.push_back( static_cast< uint8_t >( value >> 24 ) );
        }

        uint32_t read_u32_le( uint8_t const * bytes )
        {
            return static_cast< uint32_t >( bytes[0] )
                | ( static_cast< uint32_t >( bytes[1] ) << 8 )
                | ( static_cast< uint32_t >( bytes[2] ) << 16 )
                | ( static_cast< uint32_t >( bytes[3] ) << 24 );
        }

        bool is_valid_fourcc( d500_stream_group_branch branch, uint32_t fourcc )
        {
            switch( branch )
            {
            case d500_stream_group_branch::depth:
                return fourcc == make_fourcc( 'Z', '1', '6', ' ' );
            case d500_stream_group_branch::infrared:
                return fourcc == make_fourcc( 'Y', '8', 'I', ' ' );
            case d500_stream_group_branch::color_left:
            case d500_stream_group_branch::color_right:
                return fourcc == make_fourcc( 'M', '4', '2', '0' )
                    || fourcc == make_fourcc( 'N', 'V', '1', '2' )
                    || fourcc == make_fourcc( 'Y', 'U', 'Y', '2' )
                    || fourcc == make_fourcc( 'Y', 'U', 'Y', 'V' );
            }
            return false;
        }
    }

    d500_stream_group_transaction::d500_stream_group_transaction( std::shared_ptr< hw_monitor > hwm )
        : _hwm( std::move( hwm ) )
    {
        if( ! _hwm )
            throw invalid_value_exception( "stream-group transaction requires a hardware monitor" );
    }

    std::vector< uint8_t > d500_stream_group_transaction::encode_prepare(
        uint32_t transaction_id,
        std::vector< d500_stream_group_profile > const & profiles )
    {
        if( ! transaction_id )
            throw invalid_value_exception( "stream-group transaction ID must be nonzero" );
        if( profiles.empty() || profiles.size() > 4 )
            throw invalid_value_exception( "stream-group manifest must contain 1 to 4 profiles" );

        uint8_t expected_mask = 0;
        int previous_branch = -1;
        for( auto const & profile : profiles )
        {
            auto branch = static_cast< int >( profile.branch );
            if( branch < 0 || branch > 3 || branch <= previous_branch )
                throw invalid_value_exception( "stream-group profiles must use unique branches in canonical order" );
            if( ! profile.width || ! profile.height || ! profile.fps )
                throw invalid_value_exception( "stream-group profile dimensions and FPS must be nonzero" );
            if( ! is_valid_fourcc( profile.branch, profile.fourcc ) )
                throw invalid_value_exception( "stream-group profile FourCC is invalid for its branch" );

            expected_mask |= static_cast< uint8_t >( 1u << branch );
            previous_branch = branch;
        }

        std::vector< uint8_t > payload;
        payload.reserve( prepare_header_size + profiles.size() * profile_entry_size );
        payload.push_back( protocol_version );
        payload.push_back( static_cast< uint8_t >( profiles.size() ) );
        payload.push_back( expected_mask );
        payload.push_back( 0 );
        append_u32_le( payload, transaction_id );

        for( auto const & profile : profiles )
        {
            payload.push_back( static_cast< uint8_t >( profile.branch ) );
            payload.push_back( 0 );
            append_u16_le( payload, profile.width );
            append_u16_le( payload, profile.height );
            append_u16_le( payload, profile.fps );
            append_u32_le( payload, profile.fourcc );
        }
        return payload;
    }

    d500_stream_group_status d500_stream_group_transaction::decode_status(
        std::vector< uint8_t > const & response )
    {
        if( response.size() != status_size )
            throw invalid_value_exception( "stream-group QUERY response must contain exactly 8 bytes" );

        d500_stream_group_status status = {
            read_u32_le( response.data() ), response[4], response[5], response[6], response[7]
        };
        uint8_t const valid_mask = 0x0f;
        if( ( status.expected_mask | status.started_mask | status.built_mask ) & ~valid_mask )
            throw invalid_value_exception( "stream-group QUERY response contains an invalid branch mask" );
        return status;
    }

    void d500_stream_group_transaction::prepare(
        uint32_t transaction_id,
        std::vector< d500_stream_group_profile > const & profiles ) const
    {
        command cmd( stream_group_opcode, prepare_command );
        cmd.data = encode_prepare( transaction_id, profiles );
        _hwm->send( cmd );
    }

    d500_stream_group_status d500_stream_group_transaction::query( uint32_t transaction_id ) const
    {
        command cmd( stream_group_opcode, query_command, transaction_id );
        return decode_status( _hwm->send( cmd ) );
    }

    void d500_stream_group_transaction::cancel( uint32_t transaction_id ) const
    {
        if( ! transaction_id )
            throw invalid_value_exception( "stream-group CANCEL transaction ID must be nonzero" );
        command cmd( stream_group_opcode, cancel_command, transaction_id );
        _hwm->send( cmd );
    }
}
