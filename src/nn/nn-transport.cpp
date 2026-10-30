// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "nn-transport.h"

#include <src/librealsense-exception.h>

#include <rsutils/string/from.h>

#include <algorithm>
#include <thread>

namespace librealsense {


void nn_transport::send_table( uint16_t table_id, uint32_t param1, uint32_t param3,
                               const std::vector< uint8_t > & table, const progress_callback & progress )
{
    size_t const n_chunks = nn::chunk_count( table.size() );
    if( ! n_chunks || n_chunks > nn::MAX_CHUNKS )
        throw invalid_value_exception( rsutils::string::from() << "invalid NN table size " << table.size() );

    auto const before = get_status();
    if( nn::is_busy( before.state ) )
        throw io_exception( std::string( "NN upload failed: " ) + nn::BUSY_MESSAGE );

    for( size_t i = 0; i < n_chunks; ++i )
    {
        size_t const offset = i * nn::CHUNK_SIZE;
        if( auto err = send_chunk( table_id, param1, param3, nn::chunk_param( n_chunks, i ), table.data() + offset,
                                   std::min( nn::CHUNK_SIZE, table.size() - offset ) ) )
            nn::throw_error( err );
        if( progress )
            progress( 0.9f * ( i + 1 ) / n_chunks );
    }

    wait_for_commit( before.upload_seq, progress );
    if( progress )
        progress( 1.f );
}


nn::status nn_transport::get_status()
{
    nn::status status;
    if( auto err = read_status( status ) )
        nn::throw_error( err );
    return status;
}


// The last chunk's reply comes before the commit, which outlasts the 1 s XU reply window: the device reports its
// progress and result in NN_STATUS. It increments upload_seq on chunk 0, so only that number is this upload's: a
// status with the earlier one is a leftover result.
// Only a total limit: progress moves in steps seconds apart. A read that throws is retried: the device holds commands queued
// behind a calibration write for the whole flash write, past the HWM timeout, and that looks the same as a lost device.
void nn_transport::wait_for_commit( uint32_t seq_before, const progress_callback & progress )
{
    auto const start = std::chrono::steady_clock::now();
    std::string failure;
    while( true )
    {
        nn::status status;
        int32_t err = 0;
        bool read = false;
        try
        {
            err = read_status( status );
            read = true;
        }
        catch( std::exception const & e )
        {
            failure = e.what();
        }
        // An error code is the device answering, not a timeout
        if( err )
            throw io_exception( rsutils::string::from() << nn::INCOMPLETE_MESSAGE << " (" << nn::error_message( err ) << ")" );
        if( read && status.upload_seq == seq_before + 1 )
        {
            if( status.state == nn::upload_state::done )
                return;
            if( status.state == nn::upload_state::error )
                nn::throw_error( status.error );
            // 1.0 only once done
            if( progress )
                progress( 0.9f + 0.001f * std::min< uint8_t >( status.progress, 99 ) );
        }
        if( std::chrono::steady_clock::now() - start > _timing.commit_timeout )
            throw io_exception( failure.empty() ? nn::INCOMPLETE_MESSAGE : nn::INCOMPLETE_MESSAGE + ( " (" + failure + ")" ) );
        std::this_thread::sleep_for( _timing.poll_interval );
    }
}

}  // namespace librealsense
