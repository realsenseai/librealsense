// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/frame-interface.h"
#include "core/frame-continuation.h"
#include "core/frame-additional-data.h"
#include "core/frame-data-allocator.h"
#include "basics.h"
#ifdef RS2_USE_CUDA
#include "cuda/cuda-frame-memory.h"  // rs_frame_gpu_free for the cached GPU upload buffer
#endif
#include <atomic>
#include <vector>
#include <memory>
#include "archive.h"


namespace librealsense {


// Define a movable but explicitly noncopyable buffer type to hold our frame data
class LRS_EXTENSION_API frame : public frame_interface
{
public:
    // Frame pixel buffer. In zero-copy builds on an integrated GPU this is CUDA
    // pinned+mapped memory (see frame-data-allocator.h); otherwise it is identical to
    // a plain std::vector<uint8_t>. The pointer is always CPU-readable.
    std::vector< uint8_t, frame_data_allocator > data;
    frame_additional_data additional_data;
    std::shared_ptr< metadata_parser_map > metadata_parsers = nullptr;
    
    explicit frame()
        : ref_count( 0 )
        , owner( nullptr )
        , on_release()
        , _kept( false )
    {
    }
    frame( const frame & r ) = delete;
    frame(frame&& r)
        : ref_count(r.ref_count.exchange(0))
        , owner(r.owner)
        , on_release()
        , _kept(r._kept.exchange(false))
    {
        *this = std::move(r);
        if (owner)
            metadata_parsers = owner->get_md_parsers();
    }

    frame& operator=(frame&& r)
    {
        data = std::move(r.data);
        owner = r.owner;
        ref_count = r.ref_count.exchange(0);
        _kept = r._kept.exchange(false);
        on_release = std::move(r.on_release);
        additional_data = std::move(r.additional_data);
        r.owner.reset();
#ifdef RS2_USE_CUDA
        // Transfer the cached GPU upload buffer with the frame (it follows the data through
        // pool recycling); free ours first if we held a different one.
        if( _gpu_upload_buffer && _gpu_upload_buffer != r._gpu_upload_buffer )
            rs_frame_gpu_free( _gpu_upload_buffer );
        _gpu_upload_buffer = r._gpu_upload_buffer;
        _gpu_upload_capacity = r._gpu_upload_capacity;
        r._gpu_upload_buffer = nullptr;
        r._gpu_upload_capacity = 0;
#endif
        if (owner)
            metadata_parsers = owner->get_md_parsers();
        if (r.metadata_parsers)
            metadata_parsers = std::move(r.metadata_parsers);
        return *this;
    }

    frame & operator=( const frame & r ) = delete;

    virtual ~frame()
    {
        on_release.reset();
#ifdef RS2_USE_CUDA
        rs_frame_gpu_free( _gpu_upload_buffer );
#endif
    }
    frame_header const & get_header() const override { return additional_data; }
    bool find_metadata( rs2_frame_metadata_value, rs2_metadata_type * p_output_value ) const override;
    int get_frame_data_size() const override;
    const uint8_t * get_frame_data() const override;
    const void * get_gpu_data_or_upload( bool * copied ) override;
    rs2_time_t get_frame_timestamp() const override;
    rs2_timestamp_domain get_frame_timestamp_domain() const override;
    void set_timestamp( double new_ts ) override { additional_data.timestamp = new_ts; }
    unsigned long long get_frame_number() const override;
    void set_timestamp_domain( rs2_timestamp_domain timestamp_domain ) override
    {
        additional_data.timestamp_domain = timestamp_domain;
    }

    // Return FPS calculated as (1000*d_frames/d_timestamp), or 0 if this cannot be estimated
    double calc_actual_fps() const;

    rs2_time_t get_frame_system_time() const override;

    std::shared_ptr< stream_profile_interface > get_stream() const override { return stream; }
    void set_stream( std::shared_ptr< stream_profile_interface > sp ) override
    {
        stream = std::move( sp );
    }

    void acquire() override { ref_count.fetch_add( 1 ); }
    void release() override;
    void keep() override;

    frame_interface * publish( std::shared_ptr< archive_interface > new_owner ) override;
    void unpublish() override {}
    void attach_continuation( frame_continuation && continuation ) override
    {
        on_release = std::move( continuation );
    }
    void disable_continuation() override { on_release.reset(); }

    archive_interface * get_owner() const override;

    std::shared_ptr< sensor_interface > get_sensor() const override;
    void set_sensor( std::shared_ptr< sensor_interface > ) override;

    void mark_fixed() override { _fixed = true; }
    bool is_fixed() const override { return _fixed; }

    void set_blocking( bool state ) override { additional_data.is_blocking = state; }
    bool is_blocking() const override { return additional_data.is_blocking; }

private:
    // TODO: check boost::intrusive_ptr or an alternative
    std::atomic< int > ref_count;  // the reference count is on how many times this placeholder has
                                   // been observed (not lifetime, not content)
    std::shared_ptr< archive_interface > owner;  // pointer to the owner to be returned to by last observe
    std::weak_ptr< sensor_interface > sensor;
    frame_continuation on_release;
    bool _fixed = false;
    std::atomic_bool _kept;
    std::shared_ptr< stream_profile_interface > stream;
    // Cached GPU device buffer for get_gpu_data_or_upload() when true zero-copy isn't available.
    // Reused across pool recycling; (re)allocated on growth; freed in the destructor.
    void * _gpu_upload_buffer = nullptr;
    size_t _gpu_upload_capacity = 0;
};


}  // namespace librealsense
