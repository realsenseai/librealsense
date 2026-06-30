// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <functional>  // std::hash
#include <tuple>


namespace librealsense {
namespace platform {


typedef std::tuple< uint32_t, uint32_t, uint32_t, uint32_t > stream_profile_tuple;


struct stream_profile
{
    uint32_t width;
    uint32_t height;
    uint32_t fps;
    uint32_t format;

    // Backend pin/endpoint identifier. Several UVC video-streaming interfaces (pins) can be exposed under a single
    // sensor and may advertise the SAME {width,height,fps,format} (e.g. two M420 RGB streams on different endpoints).
    // pin_index lets us tell those apart so they can be enumerated and routed independently.
    // Defaults to 0 so single-pin devices are unaffected.
    uint32_t pin_index = 0;

    operator stream_profile_tuple() const { return std::make_tuple( width, height, fps, format ); }
};


inline bool operator==( const stream_profile & a, const stream_profile & b )
{
    return ( a.width == b.width ) && ( a.height == b.height ) && ( a.fps == b.fps ) && ( a.format == b.format )
        && ( a.pin_index == b.pin_index );
}


}  // namespace platform
}  // namespace librealsense


namespace std {


template<>
struct hash< librealsense::platform::stream_profile >
{
    size_t operator()( const librealsense::platform::stream_profile & k ) const
    {
        using std::hash;

        return ( hash< uint32_t >()( k.height ) ) ^ ( hash< uint32_t >()( k.width ) ) ^ ( hash< uint32_t >()( k.fps ) )
             ^ ( hash< uint32_t >()( k.format ) ) ^ ( hash< uint32_t >()( k.pin_index ) );
    }
};


}  // namespace std
