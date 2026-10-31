// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "nn-protocol.h"

#include <functional>

namespace librealsense {

// Outside nn_transport: GCC and Clang reject a nested class's default member initializers in a default argument
struct nn_transport_timing
{
    std::chrono::milliseconds poll_interval = nn::STATUS_POLL_INTERVAL;
    std::chrono::milliseconds commit_timeout = nn::COMMIT_TIMEOUT;
};

// Moves a table to the device and follows its commit in NN_STATUS. The link is implemented per channel, so others
// (e.g. a bulk endpoint) can be added later.
class nn_transport
{
public:
    using progress_callback = std::function< void( float ) >;

    explicit nn_transport( nn_transport_timing const & t = {} ) : _timing( t ) {}
    virtual ~nn_transport() = default;

    // Sends the whole table (header included) and returns once the device has committed it; throws on any device error
    void send_table( uint16_t table_id, uint32_t param1, uint32_t param3, const std::vector< uint8_t > & table,
                     const progress_callback & progress );

    // Return 0, or the device error code
    virtual int32_t send_chunk( uint16_t table_id, uint32_t param1, uint32_t param3, uint32_t param4,
                                const uint8_t * data, size_t size ) = 0;
    virtual int32_t read_status( nn::status & status ) = 0;

private:
    nn::status get_status();
    void wait_for_commit( uint32_t seq_before, const progress_callback & progress );

    nn_transport_timing _timing;
};

}  // namespace librealsense
