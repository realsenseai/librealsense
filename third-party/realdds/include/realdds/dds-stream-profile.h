// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json-fwd.h>

#include <stdint.h>
#include <string>
#include <cstring>
#include <vector>
#include <memory>
#include <iosfwd>


namespace realdds {


// Similar to fourcc, this is a sequence of ASCII characters used to uniquely identify the data format utilized by a
// stream. This string of characters defines how the data is written and, therefore, how to read it back.
// 
// While fourcc is a 4-character string, we need to use encodings that are compatible with ROS for interoperability.
// The ROS encodings are detailed here:
//     https://docs.ros.org/en/latest/api/sensor_msgs/html/image__encodings_8h_source.html
// And also listed in the header:
//     https://github.com/ros2/common_interfaces/blob/rolling/sensor_msgs/include/sensor_msgs/image_encodings.hpp
// Note that the ROS encodings often take more than 4 characters.
// The encodings also seem to be case-sensitive.
// 
// This is intended to be communicated in DDS messages and so has a fixed maximum 'size', but can be otherwise easily
// converted to a string. It must be '<= size' characters long, and we actually store the terminating null for
// convenience.
// 
// Currently, the longest format we need is "yuv422_yuy2" so 10 characters are enough to fit our needs given their ROS
// encodings. We round it up to 16 (including the terminating null) for alignment and some flexibility.
//
struct dds_video_encoding
{
    static constexpr size_t size = 15;  // max length of encoding
    char data[size + 1];                // add a terminating NULL for ease

    dds_video_encoding()
        : data{ 0 }
    {
    }

    dds_video_encoding( std::string const & );

    bool is_valid() const { return data[0] != 0; }
    bool operator==( dds_video_encoding const & rhs ) const { return std::strncmp( data, rhs.data, size ) == 0; }
    bool operator!=( dds_video_encoding const & rhs ) const { return ! operator==( rhs ); }

    std::string to_string() const { return std::string( data ); }
    operator std::string() const { return to_string(); }

    static dds_video_encoding from_rs2( int rs2_format );
    int to_rs2() const;
};


class dds_stream_base;


class dds_stream_profile
{
    int16_t _frequency;  // "Frames" per second

    std::weak_ptr< dds_stream_base > _stream;

public:
    virtual ~dds_stream_profile() {}

protected:
    dds_stream_profile( int16_t frequency )
        : _frequency( frequency )
    {
    }
    dds_stream_profile( rsutils::json const &, int & index );

public:
    std::shared_ptr< dds_stream_base > stream() const { return _stream.lock(); }
    // This is for initialization and is called from dds_stream_base only!
    void init_stream( std::weak_ptr< dds_stream_base > const & stream );

    int16_t frequency() const { return _frequency; }

    // These are for debugging - not functional
    virtual std::string to_string() const;
    virtual std::string details_to_string() const;

    // Serialization to a JSON array representation
    virtual rsutils::json to_json() const;

    // Build a profile from a json array object, e.g.:
    //      auto profile = dds_stream_profile::from_json< dds_video_stream_profile >( j );
    // This is the reverse of to_json() which returns a json array
    template< class final_stream_profile >
    static std::shared_ptr< final_stream_profile > from_json( rsutils::json const & j )
    {
        int it = 0;
        auto profile = std::make_shared< final_stream_profile >( j, it );
        verify_end_of_json( j, it );  // just so it's not in the header
        return profile;
    }

private:
    static void verify_end_of_json( rsutils::json const &, int index );
};


typedef std::vector< std::shared_ptr< dds_stream_profile > > dds_stream_profiles;


std::ostream & operator<<( std::ostream &, dds_stream_profile const & );


class dds_video_stream_profile : public dds_stream_profile
{
    typedef dds_stream_profile super;

    dds_video_encoding _encoding;
    uint16_t _width;   // Resolution width [pixels]
    uint16_t _height;  // Resolution height [pixels]

public:
    dds_video_stream_profile( int16_t frequency, dds_video_encoding encoding, uint16_t width, uint16_t height )
        : super( frequency )
        , _width( width )
        , _height( height )
        , _encoding( encoding )
    {
    }
    dds_video_stream_profile( rsutils::json const &, int & index );

    uint16_t width() const { return _width; }
    uint16_t height() const { return _height; }
    dds_video_encoding const & encoding() const { return _encoding; }
    bool is_compressed_encoding() const;

    std::string details_to_string() const override;

    rsutils::json to_json() const override;
};


class dds_motion_stream_profile : public dds_stream_profile
{
    typedef dds_stream_profile super;

public:
    dds_motion_stream_profile( int16_t frequency )
        : super( frequency )
    {
    }
    dds_motion_stream_profile( rsutils::json const & j, int & index )
        : super( j, index )
    {
    }
};


class dds_inference_stream_profile : public dds_stream_profile
{
    typedef dds_stream_profile super;

public:
    dds_inference_stream_profile( int16_t frequency )
        : super( frequency )
    {
    }

    dds_inference_stream_profile( rsutils::json const & j, int & index )
        : super( j, index )
    {
    }
};


}  // namespace realdds
