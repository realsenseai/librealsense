// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-nn-loader.h"
#include "d500-info.h"
#include "d500-private.h"

#include <src/perception-sensor.h>
#include <src/uvc-sensor.h>
#include <src/nn/nn-transport-hwm.h>

#include <rsutils/string/from.h>

namespace librealsense
{
    // First FW with the NN package tables; a placeholder until FW releases the feature
    static const firmware_version NN_UPLOAD_MIN_FW( "7.60.0.0" );

    // The FW answers the status read with -1 / -12 / -19 when it is built without the NN upload
    static bool is_unsupported_firmware( int32_t err )
    {
        return err == ds::d500_hwmon_response::INVALID_COMMAND || err == ds::d500_hwmon_response::PARAMS_TABLE_ID_NOT_VALID
            || err == ds::d500_hwmon_response::COMMAND_NOT_SUPPORTED;
    }

    d500_nn_loader::d500_nn_loader( std::shared_ptr< const d500_info > const & dev_info )
        : device( dev_info )
        , d500_device( dev_info )
    {
    }

    bool d500_nn_loader::supports_nn_upload() const
    {
        return unsupported_reason().empty();
    }

    // A FW version alone can't tell the products sharing the FW apart, so the product ID and connection decide first
    std::string d500_nn_loader::unsupported_reason() const
    {
        if( ! ds::nn_upload_pids.count( get_pid() ) || _is_mipi_device )
            return "NN model upload is not supported on this camera";
        if( _fw_version < NN_UPLOAD_MIN_FW )
            return firmware_reason();
        return {};
    }

    std::string d500_nn_loader::firmware_reason() const
    {
        std::string reason = rsutils::string::from() << "NN model upload is not supported by this camera's firmware (FW "
                                                     << _fw_version << ")";
        if( _fw_version < NN_UPLOAD_MIN_FW )
            reason += rsutils::string::from() << ". Update to FW " << NN_UPLOAD_MIN_FW << " or later to use it";
        return reason;
    }

    // The lock state is read fresh, as it can change while the device is connected
    bool d500_nn_loader::is_locked_now() const
    {
        std::vector< uint8_t > gvd( HW_MONITOR_BUFFER_SIZE );
        _hw_monitor->get_gvd( gvd.size(), gvd.data(), ds::fw_cmd::GVD );
        return gvd[ds::d500_gvd_offsets::is_camera_locked_offset] != 0;
    }

    std::string d500_nn_loader::locked_reason() const
    {
        return rsutils::string::from() << nn::error_message( ds::d500_hwmon_response::UNAUTHORIZED_USER_ACTION ) << ". ("
                                       << get_info( RS2_CAMERA_INFO_NAME ) << ", S/N "
                                       << get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) << ", FW " << _fw_version << ")";
    }

    void d500_nn_loader::nn_upload( const uint8_t * file, size_t size, rs2_nn_target target, int slot, bool activate,
                                    rs2_update_progress_callback_sptr callback )
    {
        validate_nn_upload_args( size, target, slot, activate );
        auto const unsupported = unsupported_reason();
        if( ! unsupported.empty() )
            throw not_implemented_exception( unsupported );

        std::lock_guard< std::mutex > lock( _nn_upload_mutex );
        for( size_t i = 0; i < get_sensors_count(); ++i )
        {
            auto & sensor = get_sensor( i );
            if( dynamic_cast< perception_sensor * >( &sensor ) && sensor.is_streaming() )
                throw wrong_api_call_sequence_exception( "NN upload: stop the Perception sensor before uploading" );
        }
        if( is_locked_now() )
            throw not_implemented_exception( locked_reason() );

        {
            auto power = get_raw_depth_sensor()->bulk_operation();
            nn_transport_hwm transport( _hw_monitor );
            nn::status status;
            if( is_unsupported_firmware( transport.read_status( status ) ) )
                throw not_implemented_exception( firmware_reason() );
            transport.send_table( nn::PACKAGE_TABLE_ID, nn::target_param( target ), nn::slot_param( slot, activate ),
                                  nn::build_table( nn::PACKAGE_TABLE_ID, file, size ),
                                  [&]( float progress ) { if( callback ) callback->on_update_progress( progress ); } );
        }

        // The device loads the new model only at boot
        try
        {
            hardware_reset();
        }
        catch( std::exception const & e )
        {
            throw io_exception( rsutils::string::from()
                                << "NN model installed, but the camera reset failed: reset the camera to use the new model ("
                                << e.what() << ")" );
        }
    }

    // NN_STATUS shows an install by any process. FW without the NN tables answers an error: not installing
    void d500_nn_loader::enter_update_state() const
    {
        nn::status status;
        if( supports_nn_upload()
            && nn_transport_hwm( _hw_monitor ).read_status( status ) == ds::d500_hwmon_response::SUCCESS
            && nn::is_busy( status.state ) )
            throw wrong_api_call_sequence_exception( nn::INSTALLING_MESSAGE );
        d500_device::enter_update_state();
    }

    // Static reasons only: an upload in progress or a streaming sensor is refused by nn_upload itself
    bool d500_nn_loader::is_upload_supported( std::string & reason )
    {
        reason = unsupported_reason();
        if( ! reason.empty() )
            return false;
        try
        {
            nn::status status;
            if( is_locked_now() )
                reason = locked_reason();
            else if( auto const err = nn_transport_hwm( _hw_monitor ).read_status( status ) )
                reason = is_unsupported_firmware( err ) ? firmware_reason() : nn::error_message( err );
        }
        catch( std::exception const & e )
        {
            reason = e.what();
        }
        return reason.empty();
    }
}
