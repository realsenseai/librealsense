// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>


namespace librealsense
{
    class hw_monitor;

    enum class d500_stream_group_branch : uint8_t
    {
        depth = 0,
        infrared = 1,
        color_left = 2,
        color_right = 3
    };

    enum class d500_stream_group_state : uint8_t
    {
        idle = 0,
        preparing = 1,
        prepared = 2,
        running = 3,
        failed = 4,
        cancelled = 5
    };

    struct d500_stream_group_profile
    {
        d500_stream_group_branch branch;
        uint32_t fourcc;
        uint16_t width;
        uint16_t height;
        uint16_t fps;
    };

    struct d500_stream_group_status
    {
        uint32_t transaction_id;
        uint8_t expected_mask;
        uint8_t started_mask;
        uint8_t built_mask;
        uint8_t state;
    };

    // Internal client for the explicit stream-group HWMC contract. Creating this
    // object has no effect on the existing stream open/start flow; a caller must
    // explicitly invoke prepare(), query(), or cancel().
    class d500_stream_group_transaction
    {
    public:
        explicit d500_stream_group_transaction( std::shared_ptr< hw_monitor > hwm );

        void prepare( uint32_t transaction_id,
                      std::vector< d500_stream_group_profile > const & profiles ) const;
        d500_stream_group_status query( uint32_t transaction_id = 0 ) const;
        bool query_if_supported( d500_stream_group_status & status,
                                 uint32_t transaction_id = 0 ) const;
        void cancel( uint32_t transaction_id ) const;

        static std::vector< uint8_t > encode_prepare(
            uint32_t transaction_id,
            std::vector< d500_stream_group_profile > const & profiles );
        static d500_stream_group_status decode_status( std::vector< uint8_t > const & response );

    private:
        std::shared_ptr< hw_monitor > _hwm;
    };
}
