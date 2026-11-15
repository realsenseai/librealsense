// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "nn-transport-hwm.h"

#include <src/ds/d500/d500-private.h>

namespace librealsense {

static_assert( nn::CHUNK_SIZE == HW_MONITOR_COMMAND_SIZE, "table chunks must fill an HW-monitor command" );


nn_transport_hwm::nn_transport_hwm( std::shared_ptr< hw_monitor > hwm )
    : _hwm( std::move( hwm ) )
{
}


int32_t nn_transport_hwm::send_chunk( uint16_t table_id, uint32_t param1, uint32_t param3, uint32_t param4,
                                      const uint8_t * data, size_t size )
{
    command cmd( ds::SET_HKR_CONFIG_TABLE, param1, table_id, param3, param4 );
    cmd.data.assign( data, data + size );
    // The base send: chunking is done by the caller, so the extended-buffers override must not chunk again
    hwmon_response_type response;
    _hwm->hw_monitor::send( cmd, &response );
    return response;
}


int32_t nn_transport_hwm::read_status( nn::status & status )
{
    command cmd( ds::GET_HKR_CONFIG_TABLE, nn::STATUS_REGION, nn::STATUS_TABLE_ID );
    hwmon_response_type response;
    auto reply = _hwm->hw_monitor::send( cmd, &response );
    if( response != ds::d500_hwmon_response::SUCCESS )
        return response;
    status = nn::parse_status( reply );
    return 0;
}

}  // namespace librealsense
