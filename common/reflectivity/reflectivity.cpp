// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2020 RealSense, Inc. All Rights Reserved.

#include "reflectivity.h"
#include <vector>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <limits>



using namespace rs2;




static const size_t N_STD_FRAMES = 100;
static const int    NINETY_FIVE_PERCENT_OF_STD_PERIOD = static_cast< int >( 0.95 * N_STD_FRAMES );
static const float  MAX_RANGE_IN_UNIT = 65536.0f;
static const float  LONG_THERMAL = 74.5f;
static const float  INDOOR_MAX_RANGE = 9.0f;

// TODO try to read from LRS
static const float FOV_H = 0.610865f;
static const float FOV_V = 0.479966f;

static const int VGA_WIDTH = 640;
static const int VGA_HEIGHT = 480;
static const int VGA_HALF_WIDTH = VGA_WIDTH / 2;
static const int VGA_HALF_HEIGHT = VGA_HEIGHT / 2;

static bool is_close_to_zero( float x )
{
    return ( std::abs( x ) < std::numeric_limits< float >::epsilon() );
}


reflectivity::reflectivity()
    : _history_size( 0 )
{
    _dist_queue.assign( N_STD_FRAMES,
                        0 );  // Allocate size for all samples in advance to minimize runtime.
}

// Reflectivity analysis is based on STD and normalized IR.Final reflectivity values are weighted
// average between them
float reflectivity::get_reflectivity( float raw_noise_estimation,
                                      float max_usable_range,
                                      float ir_val ) const
{
    // Calculating STD over time(temporal noise), when there are more than 5 % invalid return high
    // value
    std::vector< float > _filt_dist_arr( _dist_queue.size() );

    // Copy non zero (or close to zero) elements in history
    auto new_end = std::copy_if( _dist_queue.begin(),
                                 _dist_queue.end(),
                                 _filt_dist_arr.begin(),
                                 []( float elem ) { return ! is_close_to_zero( elem ); } );

    _filt_dist_arr.resize( new_end - _filt_dist_arr.begin() );

    if( 0 == _filt_dist_arr.size() )
        throw std::logic_error( "reflectivity N/A, not enough data was collected" );

    float sum = std::accumulate( _filt_dist_arr.begin(), _filt_dist_arr.end(), 0.0f );
    float mean = sum / _filt_dist_arr.size();

    float standard_deviation = 9999.0f;

    if( _filt_dist_arr.size() >= NINETY_FIVE_PERCENT_OF_STD_PERIOD )
    {
        float variance = 0.0f;
        for( auto val : _filt_dist_arr )
            variance += std::pow( val - mean, 2.0f );

        variance = variance / _filt_dist_arr.size();
        standard_deviation = std::sqrt( variance );
    }

    // Range is calculated based on position in map(assuming 0 tilt) Distance is just based on plane distance
    auto range = mean / 1000.0f;

    // Normalized IR instead of IR is used to be robust to high ambient, RSS is assumed between echo to noise
    auto nest = raw_noise_estimation / 16.0f;
    auto normalized_ir = 0.0f;
    if( ir_val > nest )
        normalized_ir = std::pow( std::pow( ir_val, 2.0f ) - std::pow( nest, 2.0f ), 0.5f );

    auto ref_from_ir = 0.0f;
    auto ref_from_std = 0.0f;
    auto i_ref = 0.0f;
    auto s_ref = 0.0f;
    auto i_dist_85 = 0.0f;
    auto s_dist_85 = 0.0f;
    auto ref = 0.0f;


    if( nest <= LONG_THERMAL * 2.0f )
    {
        // We can hold reflectivity information from IR
        if( normalized_ir < 70.0f )
            ref_from_ir = 0.5f;  // Low confidence as IR values are low
        else
            ref_from_ir = 1.0f;  // High confidence as IR values hold good SNR

        // Analyzing reflectivity based on 85 % reflectivity data
        i_dist_85 = 6.75f * std::exp( -0.012f * normalized_ir );

        i_dist_85 = i_dist_85 * max_usable_range / INDOOR_MAX_RANGE;

        if( 0.0f < i_dist_85 )  // protect devision by zero
        {
            i_ref = static_cast<float>(0.85f * std::pow( ( range / i_dist_85 ), 2 ));

            if( i_ref > 0.95f )
            {
                ref_from_ir = 0.75f;
                i_ref = 0.95f;
            }
        }
        else
            ref_from_ir = 0.0f;
    }

    // STD based analysis
    // STD values are at the edges, hard to get data
    if( standard_deviation >= 1.5f && standard_deviation <= 10.f )
    {
        if( standard_deviation > 2.5f )
            ref_from_std = 0.5f;  // Low confidence data due to low STD values
        else
            ref_from_std = 1.0f;  // High confidence data

        // STD based analysis based on 85% reflectivity data
        if( range > 0.0f )
        {
            s_dist_85 = ( 2.25f * ( std::log( standard_deviation ) ) + 1.56f ) * max_usable_range
                      / INDOOR_MAX_RANGE;
            if( 0 < s_dist_85 )  // protect devision by zero
            {
                s_ref = 0.85f * std::pow( ( range / s_dist_85 ), 2.0f );
                if( s_ref > 0.95f )
                {
                    ref_from_std = ref_from_std * 0.75f;
                    s_ref = 0.95f;
                }
            }
            else
                ref_from_std = 0.0f;
        }
        else
        {
            ref_from_std = 0.0f;
        }
    }
    // Calculating Final reflectivity
    if( is_close_to_zero( ref_from_ir + ref_from_std ) )
        throw std::logic_error( "reflectivity N/A" );


    ref = ( i_ref * ref_from_ir + s_ref * ref_from_std ) / ( ref_from_ir + ref_from_std );

    // Force 15% resolution
    if( ref >= 0.85f )
        ref = 0.85f;
    else if( ref >= 0.7f )
        ref = 0.7f;
    else if( ref >= 0.55f )
        ref = 0.55f;
    else if( ref >= 0.4f )
        ref = 0.4f;
    else if( ref >= 0.25f )
        ref = 0.25f;
    else
        ref = 0.1f;

    return ref;
}

void reflectivity::add_depth_sample( float depth_val, int x_in_image, int y_in_image )
{
    if( x_in_image >= 0 && x_in_image < VGA_WIDTH && y_in_image >= 0 && y_in_image < VGA_HEIGHT )
    {
        auto dist_z = round( depth_val * 16000.f / MAX_RANGE_IN_UNIT );  // convert to mm units
        float x_ang = FOV_H * std::abs( VGA_HALF_WIDTH - x_in_image ) / VGA_HALF_WIDTH;
        float y_ang = FOV_V * std::abs( VGA_HALF_HEIGHT - y_in_image ) / VGA_HALF_HEIGHT;
        auto dist_r = dist_z * std::sqrt( 1.0f + (std::pow( 2.0f * std::tan( x_ang ), 2.0f ) + std::pow( 2.0f * std::tan( y_ang ), 2.0f ) ) / 4.0f );

        if( _dist_queue.size() >= N_STD_FRAMES )  // Keep queue as N_STD_FRAMES size queue
            _dist_queue.pop_front();

        _dist_queue.push_back( dist_r );
        if( _history_size < N_STD_FRAMES )
            _history_size++;
    }
}

void rs2::reflectivity::reset_history()
{
    if( _history_size > 0 )
    {
        _dist_queue.assign( N_STD_FRAMES, 0 );
        _history_size = 0;
    }
}

float rs2::reflectivity::get_samples_ratio() const
{
    return static_cast< float >( history_size() ) / history_capacity();
}

bool rs2::reflectivity::is_history_full() const
{
    return history_size() == history_capacity();
}

// Return the history queue capacity
size_t rs2::reflectivity::history_capacity() const 
{ 
    return N_STD_FRAMES;
}

// Return the history queue current size
size_t rs2::reflectivity::history_size() const 
{ 
    return _history_size; 
}
