// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "camera-identifier-v4l.h"
#include "v4l-mipi-logic.h"
#include "v4l-usb-logic.h"

#include <src/librealsense-exception.h>

#include <rsutils/number/crc32.h>
#include <rsutils/string/from.h>

#include <sstream>
#include <cstring>

namespace librealsense
{
    namespace platform
    {
        const uint16_t D457_PID      = 0xABCD;
        const uint16_t D430_GMSL_PID = 0xABCE;
        const uint16_t D415_GMSL_PID = 0xABCF;
        const uint16_t D401_GMSL_PID = 0xABCC;

        // The MIPI GVD control returns a 4-byte HW-monitor opcode header before the GVD struct.
        static constexpr size_t GVD_OPCODE_HEADER = 4;
        // D500 GVD struct field offsets, relative to the struct start.
        static constexpr size_t D500_GVD_PAYLOAD_SIZE_OFFSET = 0x02;  // uint16
        static constexpr size_t D500_GVD_CRC32_OFFSET        = 0x04;  // uint32
        static constexpr size_t D500_GVD_HEADER_SIZE         = 0x08;  // version + payload size + CRC32
        static constexpr size_t D500_GVD_VID_OFFSET          = 0x0C;  // uint16
        static constexpr size_t D500_GVD_PID_OFFSET          = 0x0E;  // uint16

        // A D500 GVD is recognized by validating its CRC32 over the payload sized by its own size field.
        static bool is_d500_gvd( const std::vector< uint8_t > & gvd )
        {
            if( gvd.size() < GVD_OPCODE_HEADER + D500_GVD_HEADER_SIZE )
                return false;
            const uint8_t * gvd_struct = gvd.data() + GVD_OPCODE_HEADER;
            uint16_t payload_size = gvd_struct[D500_GVD_PAYLOAD_SIZE_OFFSET]
                                  | ( gvd_struct[D500_GVD_PAYLOAD_SIZE_OFFSET + 1] << 8 );
            if( payload_size == 0 || GVD_OPCODE_HEADER + D500_GVD_HEADER_SIZE + payload_size > gvd.size() )
                return false;
            uint32_t stored_crc;
            std::memcpy( &stored_crc, gvd_struct + D500_GVD_CRC32_OFFSET, sizeof( stored_crc ) );
            return rsutils::number::calc_crc32( gvd_struct + D500_GVD_HEADER_SIZE, payload_size ) == stored_crc;
        }

        void camera_identifier_v4l_mipi::resolve( const std::string & dev_name )
        {
            std::vector< uint8_t > gvd = v4l_mipi_logic::get_gvd( dev_name );

            if( is_d500_gvd( gvd ) )
            {
                // d500 MIPI (e.g. D585 GMSL): the real USB VID/PID are stored in the GVD struct.
                const uint8_t * gvd_struct = gvd.data() + GVD_OPCODE_HEADER;
                _vid = gvd_struct[D500_GVD_VID_OFFSET] | ( gvd_struct[D500_GVD_VID_OFFSET + 1] << 8 );
                _pid = gvd_struct[D500_GVD_PID_OFFSET] | ( gvd_struct[D500_GVD_PID_OFFSET + 1] << 8 );
                return;
            }

            // d400 MIPI GVD: a single-byte product id maps to the MIPI/GMSL PID, under the Intel VID.
            const uint8_t GVD_PID_OFFSET = 4;
            const uint8_t GVD_PID_D457 = 0x12;
            const uint8_t GVD_PID_D401_GMSL = 0x13;
            const uint8_t GVD_PID_D430_GMSL = 0x0F;
            const uint8_t GVD_PID_D415_GMSL = 0x06;

            if( gvd.size() <= 4u + GVD_PID_OFFSET )
                throw linux_backend_exception( "GVD response too short to identify device" );

            uint16_t device_pid = 0;
            switch( gvd[4 + GVD_PID_OFFSET] )
            {
            case GVD_PID_D457:      device_pid = D457_PID;      break;
            case GVD_PID_D430_GMSL: device_pid = D430_GMSL_PID; break;
            case GVD_PID_D415_GMSL: device_pid = D415_GMSL_PID; break;
            case GVD_PID_D401_GMSL: device_pid = D401_GMSL_PID; break;
            default:
                throw linux_backend_exception( rsutils::string::from()
                    << "Unidentified MIPI device product id: 0x" << std::hex << (int)gvd[4 + GVD_PID_OFFSET] );
            }

            _vid = 0x8086;
            _pid = device_pid;
        }

        void camera_identifier_v4l_usb::resolve( const std::string & name )
        {
            uint16_t vid{}, pid{};

            std::string modalias = v4l_usb_logic::read_modalias( name );
            if( modalias.size() < 14 || modalias.substr( 0, 5 ) != "usb:v" || modalias[9] != 'p' )
                throw linux_backend_exception( "Not a usb format modalias" );
            if( ! ( std::istringstream( modalias.substr( 5, 4 ) ) >> std::hex >> vid ) )
                throw linux_backend_exception( "Failed to read vendor ID" );
            if( ! ( std::istringstream( modalias.substr( 10, 4 ) ) >> std::hex >> pid ) )
                throw linux_backend_exception( "Failed to read product ID" );

            _vid = vid;
            _pid = pid;
        }
    }  // namespace platform
}  // namespace librealsense
