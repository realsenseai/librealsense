// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-stream-group-adapter.h"

#include <src/librealsense-exception.h>

#include <limits>
#include <map>
#include <set>


namespace librealsense
{
    namespace
    {
        uint32_t make_fourcc( char a, char b, char c, char d )
        {
            return ( static_cast< uint32_t >( static_cast< uint8_t >( a ) ) << 24 )
                | ( static_cast< uint32_t >( static_cast< uint8_t >( b ) ) << 16 )
                | ( static_cast< uint32_t >( static_cast< uint8_t >( c ) ) << 8 )
                | static_cast< uint32_t >( static_cast< uint8_t >( d ) );
        }

        bool is_native_color( uint32_t fourcc )
        {
            return fourcc == make_fourcc( 'M', '4', '2', '0' )
                || fourcc == make_fourcc( 'N', 'V', '1', '2' );
        }

        bool is_yuy2( uint32_t fourcc )
        {
            return fourcc == make_fourcc( 'Y', 'U', 'Y', '2' )
                || fourcc == make_fourcc( 'Y', 'U', 'Y', 'V' );
        }

        bool is_color_format( uint32_t fourcc )
        {
            return is_native_color( fourcc ) || is_yuy2( fourcc );
        }

        std::set< uint32_t > find_color_pins(
            std::vector< platform::stream_profile > const & profiles )
        {
            std::set< uint32_t > native_color_pins;
            std::set< uint32_t > yuy2_pins;
            for( auto const & profile : profiles )
            {
                if( is_native_color( profile.format ) )
                    native_color_pins.insert( profile.pin_index );
                if( is_yuy2( profile.format ) )
                    yuy2_pins.insert( profile.pin_index );
            }

            std::set< uint32_t > color_pins;
            for( auto pin : native_color_pins )
                if( yuy2_pins.count( pin ) )
                    color_pins.insert( pin );
            return color_pins;
        }

        d500_stream_group_branch color_branch(
            uint32_t pin, std::set< uint32_t > const & color_pins )
        {
            if( color_pins.size() != 2 || ! color_pins.count( pin ) )
                throw invalid_value_exception( "stream-group color profile does not belong to a complete dual-color pin pair" );

            // The UVC OPEN lifecycle uses the firmware endpoint mapping:
            // lowest color pin -> EP4 / left, highest -> EP8 / right.
            // This is intentionally independent of the public SDK Color 1/2
            // labels, whose pin ranking is reversed by resolve_color_stream().
            return pin == *color_pins.rbegin()
                ? d500_stream_group_branch::color_right
                : d500_stream_group_branch::color_left;
        }
    }

    std::vector< d500_stream_group_profile > d500_dual_color_stream_group_adapter::build_manifest(
        std::vector< platform::stream_profile > const & selected_profiles,
        std::vector< platform::stream_profile > const & advertised_profiles )
    {
        if( selected_profiles.empty() )
            throw invalid_value_exception( "stream-group manifest cannot be built from an empty profile set" );

        auto const color_pins = find_color_pins( advertised_profiles );
        std::map< d500_stream_group_branch, d500_stream_group_profile > manifest;
        for( auto const & profile : selected_profiles )
        {
            d500_stream_group_branch branch;
            if( profile.format == make_fourcc( 'Z', '1', '6', ' ' ) )
                branch = d500_stream_group_branch::depth;
            else if( profile.format == make_fourcc( 'Y', '8', 'I', ' ' )
                     || profile.format == make_fourcc( 'G', 'R', 'E', 'Y' ) )
                branch = d500_stream_group_branch::infrared;
            else if( is_color_format( profile.format ) )
                branch = color_branch( profile.pin_index, color_pins );
            else
                throw invalid_value_exception( "resolved raw profile is not supported by the stream-group V1 adapter" );

            if( profile.width > std::numeric_limits< uint16_t >::max()
                || profile.height > std::numeric_limits< uint16_t >::max()
                || profile.fps > std::numeric_limits< uint16_t >::max() )
                throw invalid_value_exception( "resolved raw profile exceeds the stream-group V1 field width" );

            d500_stream_group_profile entry = {
                branch,
                profile.format,
                static_cast< uint16_t >( profile.width ),
                static_cast< uint16_t >( profile.height ),
                static_cast< uint16_t >( profile.fps )
            };
            if( ! manifest.emplace( branch, entry ).second )
                throw invalid_value_exception( "resolved raw profiles contain more than one profile for a physical branch" );
        }

        std::vector< d500_stream_group_profile > result;
        result.reserve( manifest.size() );
        for( auto const & branch : manifest )
            result.push_back( branch.second );
        return result;
    }
}
