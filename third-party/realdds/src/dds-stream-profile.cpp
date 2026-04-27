// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-stream-profile.h>
#include <realdds/dds-stream-base.h>
#include <realdds/dds-exceptions.h>

#include <rsutils/json.h>
#include <map>


namespace realdds {


std::ostream & operator<<( std::ostream & os, dds_stream_profile const & profile )
{
    os << profile.to_string();
    return os;
}


dds_video_encoding::dds_video_encoding( std::string const & s )
{
    if( s.length() > size )
        DDS_THROW( runtime_error, "encoding is too long" );
    data[s.copy( data, size )] = 0;
}


enum rs2_format  // copy from rs2_sensor.h
{
    RS2_FORMAT_ANY, /**< When passed to enable stream, librealsense will try to provide best suited format */
    RS2_FORMAT_Z16, /**< 16-bit linear depth values. The depth is meters is equal to depth scale * pixel value. */
    RS2_FORMAT_DISPARITY16, /**< 16-bit float-point disparity values. Depth->Disparity conversion : Disparity =
                               Baseline*FocalLength/Depth. */
    RS2_FORMAT_XYZ32F,      /**< 32-bit floating point 3D coordinates. */
    RS2_FORMAT_YUYV,  /**< 32-bit y0, u, y1, v data for every two pixels. Similar to YUV422 but packed in a different
                         order - https://en.wikipedia.org/wiki/YUV */
    RS2_FORMAT_RGB8,  /**< 8-bit red, green and blue channels */
    RS2_FORMAT_BGR8,  /**< 8-bit blue, green, and red channels -- suitable for OpenCV */
    RS2_FORMAT_RGBA8, /**< 8-bit red, green and blue channels + constant alpha channel equal to FF */
    RS2_FORMAT_BGRA8, /**< 8-bit blue, green, and red channels + constant alpha channel equal to FF */
    RS2_FORMAT_Y8,    /**< 8-bit per-pixel grayscale image */
    RS2_FORMAT_Y16,   /**< 16-bit per-pixel grayscale image */
    RS2_FORMAT_RAW10, /**< Four 10 bits per pixel luminance values packed into a 5-byte macropixel */
    RS2_FORMAT_RAW16, /**< 16-bit raw image */
    RS2_FORMAT_RAW8,  /**< 8-bit raw image */
    RS2_FORMAT_UYVY,  /**< Similar to the standard YUYV pixel format, but packed in a different order */
    RS2_FORMAT_MOTION_RAW,    /**< Raw data from the motion sensor */
    RS2_FORMAT_MOTION_XYZ32F, /**< Motion data packed as 3 32-bit float values, for X, Y, and Z axis */
    RS2_FORMAT_GPIO_RAW,      /**< Raw data from the external sensors hooked to one of the GPIO's */
    RS2_FORMAT_6DOF, /**< Pose data packed as floats array, containing translation vector, rotation quaternion and
                        prediction velocities and accelerations vectors */
    RS2_FORMAT_DISPARITY32, /**< 32-bit float-point disparity values. Depth->Disparity conversion : Disparity =
                               Baseline*FocalLength/Depth */
    RS2_FORMAT_Y10BPACK,    /**< 16-bit per-pixel grayscale image unpacked from 10 bits per pixel packed
                               ([8:8:8:8:2222]) grey-scale image. The data is unpacked to LSB and padded with 6 zero
                               bits */
    RS2_FORMAT_DISTANCE,    /**< 32-bit float-point depth distance value.  */
    RS2_FORMAT_MJPEG, /**< Bitstream encoding for video in which an image of each frame is encoded as JPEG-DIB   */
    RS2_FORMAT_Y8I,   /**< 8-bit per pixel interleaved. 8-bit left, 8-bit right.  */
    RS2_FORMAT_Y12I,  /**< 12-bit per pixel interleaved. 12-bit left, 12-bit right. Each pixel is stored in a 24-bit
                         word in little-endian order. */
    RS2_FORMAT_INZI,  /**< multi-planar Depth 16bit + IR 10bit.  */
    RS2_FORMAT_INVI,  /**< 8-bit IR stream.  */
    RS2_FORMAT_W10,   /**< Grey-scale image as a bit-packed array. 4 pixel data stream taking 5 bytes */
    RS2_FORMAT_Z16H,  /**< DEPRECATED! - Variable-length Huffman-compressed 16-bit depth values. */
    RS2_FORMAT_FG,    /**< 16-bit per-pixel frame grabber format. */
    RS2_FORMAT_Y411,  /**< 12-bit per-pixel. */
    RS2_FORMAT_COMBINED_MOTION,
    RS2_FORMAT_COUNT  /**< Number of enumeration values. Not a valid input: intended to be used in for-loops. */
};


int dds_video_encoding::to_rs2() const
{
    static std::map< std::string, int > fcc_to_rs2{  // copy from ds5-device.cpp
        { "yuv422_yuy2", RS2_FORMAT_YUYV },  // Used by Color streams; ROS2-compatible
        { "uyvy", RS2_FORMAT_UYVY },
        { "mono8", RS2_FORMAT_Y8 },  // Used by IR streams; ROS2-compatible
        { "Y8I",  RS2_FORMAT_Y8I },
        { "W10",  RS2_FORMAT_W10 },
        { "mono16",  RS2_FORMAT_Y16 }, // Used by IR streams; ROS2-compatible
        { "Y12I", RS2_FORMAT_Y12I },
//      { "Y16I", RS2_FORMAT_Y16I },
        { "16UC1", RS2_FORMAT_Z16 },  // Used by depth streams; ROS2-compatible
        { "rgb8", RS2_FORMAT_RGB8 },  // Used by color streams; ROS2-compatible
        { "RGBA", RS2_FORMAT_RGBA8 },
        { "RGB2", RS2_FORMAT_BGR8 },
        { "BGRA", RS2_FORMAT_BGRA8 },
        { "jpeg", RS2_FORMAT_MJPEG }, // ROS2-compatible for compressed images
        { "CNF4", RS2_FORMAT_RAW8 },
        { "BYR2", RS2_FORMAT_RAW16 },
        { "R10", RS2_FORMAT_RAW10 },
        { "Y10B", RS2_FORMAT_Y10BPACK },
    };

    std::string s = to_string();
    auto it = fcc_to_rs2.find( s );
    if( it == fcc_to_rs2.end() )
        DDS_THROW( runtime_error, "Invalid encoding '" + s + "'" );
    return it->second;
}


dds_video_encoding dds_video_encoding::from_rs2( int rs2_format )
{
    char const * encoding = nullptr;
    switch( rs2_format )
    {
    case RS2_FORMAT_YUYV: encoding = "yuv422_yuy2"; break;
    case RS2_FORMAT_Y8: encoding = "mono8"; break;
    case RS2_FORMAT_Y8I: encoding = "Y8I"; break;
    case RS2_FORMAT_W10: encoding = "W10"; break;
    case RS2_FORMAT_Y16: encoding = "mono16"; break;
    case RS2_FORMAT_Y12I: encoding = "Y12I"; break;
//  case RS2_FORMAT_Y16I: encoding = "Y16I"; break;
    case RS2_FORMAT_Z16: encoding = "16UC1"; break;
    case RS2_FORMAT_RGB8: encoding = "rgb8"; break;
    case RS2_FORMAT_RGBA8: encoding = "RGBA"; break;  // todo
    case RS2_FORMAT_BGR8: encoding = "RGB2"; break;
    case RS2_FORMAT_BGRA8: encoding = "BGRA"; break;  // todo
    case RS2_FORMAT_MJPEG: encoding = "jpeg"; break;
    case RS2_FORMAT_RAW8: encoding = "CNF4"; break;
    case RS2_FORMAT_RAW16: encoding = "BYR2"; break;
    case RS2_FORMAT_RAW10: encoding = "R10"; break;
    case RS2_FORMAT_UYVY: encoding = "uyvy"; break;
    case RS2_FORMAT_Y10BPACK: encoding = "Y10B"; break;
    default:
        DDS_THROW( runtime_error, "cannot translate rs2_format " + std::to_string( rs2_format ) + " to any known dds_video_encoding" );
    };
    return dds_video_encoding( encoding );
}


dds_stream_profile::dds_stream_profile( rsutils::json const & j, int & it )
    : _frequency( j[it++].get< int16_t >() )
{
    // NOTE: the order of construction is the order of declaration -- therefore the to_json() function
    // should use the same ordering!
}


/* static */ void dds_stream_profile::verify_end_of_json( rsutils::json const & j, int index )
{
    if( index != j.size() )
        DDS_THROW( runtime_error, "expected end of json at index " + std::to_string( index ) );
}


std::string dds_stream_profile::to_string() const
{
    std::ostringstream os;
    os << '<';
    if( stream() )
        os << "'" << stream()->name() << "' ";
    os << details_to_string();
    os << '>';
    return os.str();
}

std::string dds_stream_profile::details_to_string() const
{
    std::ostringstream os;
    os << "@ " << _frequency << " Hz";
    return os.str();
}

std::string dds_video_stream_profile::details_to_string() const
{
    std::ostringstream os;
    os << _width << 'x' << _height << ' ' << _encoding.to_string() << ' ';
    os << super::details_to_string();
    return os.str();
}


void dds_stream_profile::init_stream( std::weak_ptr< dds_stream_base > const & stream )
{
    if( ! stream.lock() )
        DDS_THROW( runtime_error, "cannot set stream to null" );
    if( _stream.lock() )
        DDS_THROW( runtime_error, "profile is already associated with a stream" );
    _stream = stream;
}


rsutils::json dds_stream_profile::to_json() const
{
    // NOTE: same ordering as construction!
    return rsutils::json::array( { frequency() } );
}


dds_video_stream_profile::dds_video_stream_profile( rsutils::json const & j, int & index )
    : super( j, index )
    , _encoding( j[index++].get< std::string >() )
{
    _width = j[index++].get< int16_t >();
    _height = j[index++].get< int16_t >();
}

bool dds_video_stream_profile::is_compressed_encoding() const
{
    return _encoding.to_rs2() == RS2_FORMAT_MJPEG;
}


rsutils::json dds_video_stream_profile::to_json() const
{
    auto profile = super::to_json();
    profile += encoding().to_string();
    profile += width();
    profile += height();
    return profile;
}


}  // namespace realdds
