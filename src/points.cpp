// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "points.h"
#include "core/video.h"
#include "core/video-frame.h"
#include "core/frame-holder.h"
#include "librealsense-exception.h"
#include <rsutils/string/from.h>
#include <fstream>
#include <cmath>
#include <algorithm>

#define MIN_DISTANCE 1e-6

namespace librealsense {

float3 * points::get_vertices()
{
    get_frame_data();  // call GetData to ensure data is in main memory
    return (float3*)data.data();
}

std::tuple< uint8_t, uint8_t, uint8_t >
get_texcolor( const frame_holder & texture, float u, float v )
{
    auto ptr = dynamic_cast< video_frame * >( texture.frame );
    if( ptr == nullptr )
    {
        throw librealsense::invalid_value_exception( "frame must be video frame" );
    }
    const int w = ptr->get_width(), h = ptr->get_height();
    int x = std::min( std::max( int( u * w + .5f ), 0 ), w - 1 );
    int y = std::min( std::max( int( v * h + .5f ), 0 ), h - 1 );
    int idx = x * ptr->get_bpp() / 8 + y * ptr->get_stride();
    const auto texture_data = reinterpret_cast< const uint8_t * >( ptr->get_frame_data() );
    return std::make_tuple( texture_data[idx], texture_data[idx + 1], texture_data[idx + 2] );
}


void points::export_to_ply( const std::string & fname, const frame_holder & texture_in )
{
    auto stream_profile = get_stream().get();
    auto video_stream_profile = dynamic_cast< video_stream_profile_interface * >( stream_profile );
    if( ! video_stream_profile )
        throw librealsense::invalid_value_exception( "stream must be video stream" );
    const auto vertices = get_vertices();
    const auto texcoords = get_texture_coordinates();
    const frame_holder no_texture;
    const frame_holder & texture = texcoords ? texture_in : no_texture;  // device cloud: vertices only
    std::vector< float3 > new_vertices;
    std::vector< std::tuple< uint8_t, uint8_t, uint8_t > > new_tex;
    std::map< int, int > index2reducedIndex;

    const size_t vertex_count = get_vertex_count();
    new_vertices.reserve( vertex_count );
    new_tex.reserve( vertex_count );
    assert( vertex_count );
    for( size_t i = 0; i < vertex_count; ++i )
        if( fabs( vertices[i].x ) >= MIN_DISTANCE || fabs( vertices[i].y ) >= MIN_DISTANCE
            || fabs( vertices[i].z ) >= MIN_DISTANCE )
        {
            index2reducedIndex[i] = (int)new_vertices.size();
            new_vertices.push_back( { vertices[i].x, -1 * vertices[i].y, -1 * vertices[i].z } );
            if( texture && texcoords )
            {
                auto color = get_texcolor( texture, texcoords[i].x, texcoords[i].y );
                new_tex.push_back( color );
            }
        }

    const auto threshold = 0.05f;
    auto width = video_stream_profile->get_width();
    std::vector< std::tuple< int, int, int > > faces;
    for( uint32_t x = 0; x < width - 1; ++x )
    {
        for( uint32_t y = 0; y < video_stream_profile->get_height() - 1; ++y )
        {
            auto a = y * width + x, b = y * width + x + 1, c = ( y + 1 ) * width + x,
                 d = ( y + 1 ) * width + x + 1;
            if( vertices[a].z && vertices[b].z && vertices[c].z && vertices[d].z
                && std::abs( vertices[a].z - vertices[b].z ) < threshold
                && std::abs( vertices[a].z - vertices[c].z ) < threshold
                && std::abs( vertices[b].z - vertices[d].z ) < threshold
                && std::abs( vertices[c].z - vertices[d].z ) < threshold )
            {
                if( index2reducedIndex.count( a ) == 0 || index2reducedIndex.count( b ) == 0
                    || index2reducedIndex.count( c ) == 0 || index2reducedIndex.count( d ) == 0 )
                    continue;

                faces.emplace_back( index2reducedIndex[a],
                                    index2reducedIndex[d],
                                    index2reducedIndex[b] );
                faces.emplace_back( index2reducedIndex[d],
                                    index2reducedIndex[a],
                                    index2reducedIndex[c] );
            }
        }
    }

    std::ofstream out( fname );
    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "comment pointcloud saved from Realsense Viewer\n";
    out << "element vertex " << new_vertices.size() << "\n";
    out << "property float" << sizeof( float ) * 8 << " x\n";
    out << "property float" << sizeof( float ) * 8 << " y\n";
    out << "property float" << sizeof( float ) * 8 << " z\n";
    if( texture )
    {
        out << "property uchar red\n";
        out << "property uchar green\n";
        out << "property uchar blue\n";
    }
    out << "element face " << faces.size() << "\n";
    out << "property list uchar int vertex_indices\n";
    out << "end_header\n";
    out.close();

    out.open( fname, std::ios_base::app | std::ios_base::binary );
    for( int i = 0; i < new_vertices.size(); ++i )
    {
        // we assume little endian architecture on your device
        out.write( reinterpret_cast< const char * >( &( new_vertices[i].x ) ), sizeof( float ) );
        out.write( reinterpret_cast< const char * >( &( new_vertices[i].y ) ), sizeof( float ) );
        out.write( reinterpret_cast< const char * >( &( new_vertices[i].z ) ), sizeof( float ) );

        if( texture )
        {
            uint8_t x, y, z;
            std::tie( x, y, z ) = new_tex[i];
            out.write( reinterpret_cast< const char * >( &x ), sizeof( uint8_t ) );
            out.write( reinterpret_cast< const char * >( &y ), sizeof( uint8_t ) );
            out.write( reinterpret_cast< const char * >( &z ), sizeof( uint8_t ) );
        }
    }
    auto size = faces.size();
    for( int i = 0; i < size; ++i )
    {
        int three = 3;
        out.write( reinterpret_cast< const char * >( &three ), sizeof( uint8_t ) );
        out.write( reinterpret_cast< const char * >( &( std::get< 0 >( faces[i] ) ) ),
                   sizeof( int ) );
        out.write( reinterpret_cast< const char * >( &( std::get< 1 >( faces[i] ) ) ),
                   sizeof( int ) );
        out.write( reinterpret_cast< const char * >( &( std::get< 2 >( faces[i] ) ) ),
                   sizeof( int ) );
    }
}

// Device point cloud (RS2_STREAM_POINT_CLOUD, D5xx EP12 Mapping): the bare W x H vertex array, 12 B per
// point, with no texture coordinates after it.
bool points::is_device_cloud() const
{
    auto profile = get_stream();
    return profile && profile->get_stream_type() == RS2_STREAM_POINT_CLOUD;
}

size_t points::get_vertex_count() const
{
    if( is_device_cloud() )
    {
        size_t count = data.size() / sizeof( float3 );
        auto vsp = dynamic_cast< video_stream_profile_interface * >( get_stream().get() );
        if( vsp )
            count = std::min( count, size_t( vsp->get_width() ) * vsp->get_height() );
        return count;
    }
    return data.size() / ( sizeof( float3 ) + sizeof( int2 ) );
}

float2 * points::get_texture_coordinates()
{
    if( is_device_cloud() )
        return nullptr;
    get_frame_data();  // call GetData to ensure data is in main memory
    auto xyz = (float3 *)data.data();
    auto ijs = (float2 *)( xyz + get_vertex_count() );
    return ijs;
}

} // namespace librealsense
