// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once
#ifdef BUILD_WITH_CLOSE_RANGE_DEPTH

#include <rs_depth_calibration.hpp>
#include <rs_depth_range.hpp>  // FrameMetadata and extern "C" function declarations
#include <rsutils/easylogging/easyloggingpp.h>

#include <cstdint>
#include <dlfcn.h>
#include <stdexcept>

// Runtime loader for librs_depth_range.so.
// Wraps dlopen so the viewer starts even when the library is absent;
// callers check is_loaded() before use.  The singleton is initialised once
// on first access and lives for the process lifetime.
class rs_depth_range_loader
{
public:
    // Derive types directly from the extern "C" declarations in rs_depth_range.hpp
    // so the compiler validates our casts instead of accepting hand-typed signatures.
    using fn_create     = decltype( &rs_depth_range_create );
    using fn_destroy    = decltype( &rs_depth_range_destroy );
    using fn_process    = decltype( &rs_depth_range_process );
    using fn_last_error = decltype( &rs_depth_range_last_error );

    rs_depth_range_loader()
    {
        _handle = ::dlopen( "librs_depth_range.so", RTLD_LAZY | RTLD_LOCAL );
        if( ! _handle )
            _handle = ::dlopen( "/opt/librealsense2-enhanced-depth/lib/librs_depth_range.so",
                                RTLD_LAZY | RTLD_LOCAL );
        if( ! _handle )
        {
            LOG_WARNING( "Close Range Depth: dlopen(librs_depth_range.so) failed: " << ::dlerror() );
            return;
        }

        _create     = reinterpret_cast< fn_create >(
                          ::dlsym( _handle, "rs_depth_range_create" ) );
        _destroy    = reinterpret_cast< fn_destroy >(
                          ::dlsym( _handle, "rs_depth_range_destroy" ) );
        _process    = reinterpret_cast< fn_process >(
                          ::dlsym( _handle, "rs_depth_range_process" ) );
        _last_error = reinterpret_cast< fn_last_error >(
                          ::dlsym( _handle, "rs_depth_range_last_error" ) );

        if( ! _create || ! _destroy || ! _process || ! _last_error )
        {
            LOG_WARNING( "Close Range Depth: missing symbols in librs_depth_range.so: " << ::dlerror() );
            ::dlclose( _handle );
            _handle     = nullptr;
            _create     = nullptr;
            _destroy    = nullptr;
            _process    = nullptr;
            _last_error = nullptr;
        }
    }

    // Do NOT dlclose: the singleton outlives all rs_depth_range_impl objects only if
    // static-destruction order is guaranteed, which it is not.  The OS reclaims the
    // mapping on process exit without our help.
    ~rs_depth_range_loader() = default;

    rs_depth_range_loader( const rs_depth_range_loader & ) = delete;
    rs_depth_range_loader & operator=( const rs_depth_range_loader & ) = delete;

    bool is_loaded() const { return _handle != nullptr; }

    fn_create     create()     const { return _create; }
    fn_destroy    destroy()    const { return _destroy; }
    fn_process    process()    const { return _process; }
    fn_last_error last_error() const { return _last_error; }

private:
    void *        _handle     = nullptr;
    fn_create     _create     = nullptr;
    fn_destroy    _destroy    = nullptr;
    fn_process    _process    = nullptr;
    fn_last_error _last_error = nullptr;
};

inline rs_depth_range_loader & get_rs_depth_range_loader()
{
    static rs_depth_range_loader s_loader;
    return s_loader;
}

// RAII wrapper around the opaque library handle.
// Mirrors the rs_depth::DepthRangeImprover interface so call sites change minimally.
class rs_depth_range_impl
{
public:
    explicit rs_depth_range_impl( const rs_depth::Calibration & cal )
    {
        rs_depth_range_loader & ldr = get_rs_depth_range_loader();
        _handle = ldr.create()(
            cal.focal_length_px,
            cal.baseline_m,
            cal.min_z_threshold_mm(),
            0.35f,       // scale_factor — matches DepthRangeImprover default
            -1, -1, -1, -1  // no crop region
        );
        if( ! _handle )
        {
            const char * err = ldr.last_error()( nullptr );
            throw std::runtime_error( err ? err : "rs_depth_range_create failed" );
        }
    }

    ~rs_depth_range_impl()
    {
        if( _handle )
            get_rs_depth_range_loader().destroy()( _handle );
    }

    rs_depth_range_impl( const rs_depth_range_impl & ) = delete;
    rs_depth_range_impl & operator=( const rs_depth_range_impl & ) = delete;

    void process( const uint8_t * ir_l, const uint8_t * ir_r,
                  const uint16_t * depth_in, uint16_t * depth_out,
                  const FrameMetadata & meta )
    {
        rs_depth_range_loader & ldr = get_rs_depth_range_loader();
        int ret = ldr.process()( _handle, ir_l, ir_r, depth_in, depth_out, &meta );
        if( ret != 0 )
        {
            const char * err = ldr.last_error()( _handle );
            throw std::runtime_error( err ? err : "rs_depth_range_process failed" );
        }
    }

private:
    void * _handle = nullptr;
};

#endif  // BUILD_WITH_CLOSE_RANGE_DEPTH
