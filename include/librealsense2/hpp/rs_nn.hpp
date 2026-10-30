// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_NN_HPP
#define LIBREALSENSE_RS2_NN_HPP

#include "rs_types.hpp"
#include "rs_device.hpp"
#include "../h/rs_nn.h"

#include <climits>

namespace rs2
{
    /**
     * \brief Device that accepts NN model packages from the Perception Service
     */
    class nn_loader : public device
    {
    public:
        nn_loader() : device() {}

        nn_loader( device d ) : device( d.get() )
        {
            rs2_error * e = nullptr;
            if( ! rs2_is_device_extendable_to( _dev.get(), RS2_EXTENSION_NN_LOADER, &e ) || e )
                _dev.reset();
            error::handle( e );
        }

        /**
         * Upload an NN model package, sent to the device as is. Stop the Perception sensor first. When the install
         * is done the SDK resets the camera: the device disconnects and must be opened again, as after a firmware
         * update. Firmware update on the device is refused while it installs a model, uploaded by any process. If
         * the upload does not complete, wait a minute before resetting or reconnecting the camera: it may still be
         * writing the model. Currently only RS2_NN_TARGET_FLASH, slot 0 and activate = true are supported.
         */
        void nn_upload( const std::vector< uint8_t > & file,
                        rs2_nn_target target = RS2_NN_TARGET_FLASH,
                        int slot = 0,
                        bool activate = true ) const
        {
            rs2_error * e = nullptr;
            rs2_nn_upload_cpp( _dev.get(), file.data(), file_size( file ), target, slot, activate, nullptr, &e );
            error::handle( e );
        }

        /**
         * Same as above, with progress notifications in [0, 1] via the callback
         */
        template< class T >
        void nn_upload( const std::vector< uint8_t > & file, rs2_nn_target target, int slot, bool activate,
                        T callback ) const
        {
            rs2_error * e = nullptr;
            rs2_nn_upload_cpp( _dev.get(), file.data(), file_size( file ), target, slot, activate,
                               new update_progress_callback< T >( std::move( callback ) ), &e );
            error::handle( e );
        }

        /**
         * Same as above, with the default target, slot and activate
         */
        template< class T >
        void nn_upload( const std::vector< uint8_t > & file, T callback ) const
        {
            nn_upload( file, RS2_NN_TARGET_FLASH, 0, true, std::move( callback ) );
        }

        /**
         * Whether the device supports NN model upload (product, firmware, not locked)
         * \param[out] reason  If non-null, receives why not
         */
        bool is_upload_supported( std::string * reason = nullptr ) const
        {
            rs2_error * e = nullptr;
            char buf[256] = {};
            bool supported = rs2_nn_is_upload_supported( _dev.get(), buf, sizeof( buf ), &e ) != 0;
            error::handle( e );
            if( reason )
                *reason = buf;
            return supported;
        }

    private:
        // The C API takes an int: anything larger is passed as INT_MAX, so the size check rejects it instead of a
        // truncated size going through
        static int file_size( const std::vector< uint8_t > & file )
        {
            return file.size() > size_t( INT_MAX ) ? INT_MAX : int( file.size() );
        }
    };
}
#endif // LIBREALSENSE_RS2_NN_HPP
