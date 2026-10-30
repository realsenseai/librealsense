// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "nn-transport.h"
#include <src/hw-monitor.h>

#include <memory>

namespace librealsense {

// HW-monitor transport: 1000-byte chunks of SET_HKR_CONFIG_TABLE, and the status table with GET_HKR_CONFIG_TABLE.
// The caller holds UVC power for the whole transfer; otherwise each chunk costs a power cycle (~95 ms instead of ~0.6 ms).
class nn_transport_hwm : public nn_transport
{
public:
    explicit nn_transport_hwm( std::shared_ptr< hw_monitor > hwm );

    int32_t send_chunk( uint16_t table_id, uint32_t param1, uint32_t param3, uint32_t param4,
                        const uint8_t * data, size_t size ) override;
    int32_t read_status( nn::status & status ) override;

private:
    std::shared_ptr< hw_monitor > _hwm;
};

}  // namespace librealsense
