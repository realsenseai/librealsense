// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-usb-logic.h"

#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <sys/stat.h>
#include <sys/sysmacros.h>  // major(...), minor(...)
#include <limits.h>         // PATH_MAX
#include <cstdlib>          // realpath

#include <sstream>
#include <fstream>
#include <regex>

namespace librealsense
{
    namespace platform
    {
        namespace v4l_usb_logic
        {
            static const size_t MAX_DEV_PARENT_DIR = 10;

            bool is_usb_device_path( const std::string & video_path )
            {
                static const std::regex uvc_pattern( "(\\/usb\\d+\\/)\\w+" );  // Locate UVC device path pattern ../usbX/...
                return std::regex_search( video_path, uvc_pattern );
            }

            bool is_usb_path_valid( const std::string & usb_video_path, const std::string & dev_name,
                                    std::string & busnum, std::string & devnum, std::string & devpath )
            {
                struct stat st = {};
                if( stat( dev_name.c_str(), &st ) < 0 )
                {
                    throw linux_backend_exception( rsutils::string::from() << "Cannot identify '" << dev_name );
                }
                if( ! S_ISCHR( st.st_mode ) )
                    throw linux_backend_exception( dev_name + " is no device" );

                // Search directory and up to three parent directories to find busnum/devnum
                auto valid_path = false;
                std::ostringstream ss;
                ss << "/sys/dev/char/" << major( st.st_rdev ) << ":" << minor( st.st_rdev ) << "/device/";

                auto char_dev_path = ss.str();

                for( auto i = 0U; i < MAX_DEV_PARENT_DIR; ++i )
                {
                    if( std::ifstream( char_dev_path + "busnum" ) >> busnum )
                    {
                        if( std::ifstream( char_dev_path + "devnum" ) >> devnum )
                        {
                            if( std::ifstream( char_dev_path + "devpath" ) >> devpath )
                            {
                                valid_path = true;
                                break;
                            }
                        }
                    }
                    char_dev_path += "../";
                }
                return valid_path;
            }

            std::string read_modalias( const std::string & name )
            {
                std::string modalias;
                if( ! ( std::ifstream( "/sys/class/video4linux/" + name + "/device/modalias" ) >> modalias ) )
                    throw linux_backend_exception( "Failed to read modalias" );
                return modalias;
            }

            uint16_t read_interface_number( const std::string & name )
            {
                uint16_t mi{};
                if( ! ( std::ifstream( "/sys/class/video4linux/" + name + "/device/bInterfaceNumber" ) >> std::hex >> mi ) )
                    throw linux_backend_exception( "Failed to read interface number" );
                return mi;
            }

            usb_spec get_usb_connection_type( std::string path )
            {
                usb_spec res{ usb_undefined };

                char usb_actual_path[PATH_MAX] = { 0 };
                if( realpath( path.c_str(), usb_actual_path ) != nullptr )
                {
                    path = std::string( usb_actual_path );
                    std::string camera_usb_version;
                    if( ! ( std::ifstream( path + "/version" ) >> camera_usb_version ) )
                        throw linux_backend_exception( "Failed to read usb version specification" );

                    // find a usb type whose name is contained in 'camera_usb_version' (contained, not equal,
                    // because of differences like "3.2" vs "3.20")
                    for( const auto usb_type : usb_name_to_spec )
                    {
                        std::string usb_name = usb_type.first;
                        if( std::string::npos != camera_usb_version.find( usb_name ) )
                        {
                            res = usb_type.second;
                            return res;
                        }
                    }
                }
                return res;
            }
        }  // namespace v4l_usb_logic
    }  // namespace platform
}  // namespace librealsense
