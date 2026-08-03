// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "hw_monitor_extended_buffers.h"
#include <ds/ds-private.h>
#include <ds/d500/d500-private.h>


namespace librealsense
{

    int hw_monitor_extended_buffers::get_number_of_chunks(size_t msg_length) const
    {
        return static_cast<int>(std::ceil(msg_length / (float)HW_MONITOR_COMMAND_SIZE));
    }

    hw_monitor_extended_buffers::hwm_buffer_type hw_monitor_extended_buffers::get_buffer_type(command cmd) const
    {
        bool provide_whole_table = (cmd.param4 == 0);
        switch( cmd.cmd)
        {
        case ds::d500_fw_cmd::GET_HKR_CONFIG_TABLE:
            return provide_whole_table ? hwm_buffer_type::extended_receive : hwm_buffer_type::standard;
        case ds::d500_fw_cmd::SET_HKR_CONFIG_TABLE:
            return provide_whole_table ? hwm_buffer_type::extended_send : hwm_buffer_type::standard;
        default:
            return hwm_buffer_type::standard;
        }
    }

    // 3 cases are foreseen in this method:
    // - no buffer bigger than 1 KB is expected with the current command => work as normal hw_monitor::send method
    // - buffer bigger than 1 KB expected to be received => iterate the hw_monitor::send method and append the results
    // - buffer bigger than 1 KB expected to be sent => iterate the hw_monitor, while iterating over the input
    std::vector<uint8_t> hw_monitor_extended_buffers::send(command const & cmd, hwmon_response_type* p_response, bool locked_transfer) const
    {
        hwm_buffer_type buffer_type = get_buffer_type(cmd);
        switch( buffer_type)
        {
        case hwm_buffer_type::standard:
            return hw_monitor::send(cmd, p_response, locked_transfer);
        case hwm_buffer_type::extended_receive:
            return extended_receive(cmd, p_response, locked_transfer);
        case  hwm_buffer_type::extended_send:
            extended_send(cmd, p_response, locked_transfer);
            break;
        default:
            return std::vector<uint8_t>();
        }
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> hw_monitor_extended_buffers::extended_receive(command cmd, hwmon_response_type* p_response, bool locked_transfer) const
    {
        std::vector< uint8_t > recv_msg;

        // send first command with 0/0 on param4, this should get the first chunk withoud knowing
        // the actual table size, actual size will be returned as part for the response header and
        // will be used to calculate the extended loop range
        auto ans = hw_monitor::send(cmd, p_response, locked_transfer);
        recv_msg.insert(recv_msg.end(), ans.begin(), ans.end());

        if (recv_msg.size() < sizeof(ds::table_header))
            throw std::runtime_error(rsutils::string::from() << "Table data has invalid size = " << recv_msg.size());


        ds::table_header* th = reinterpret_cast<ds::table_header*>( ans.data() );
        size_t recv_msg_length = sizeof(ds::table_header) + th->table_size;

        if (recv_msg_length > HW_MONITOR_BUFFER_SIZE)
        {
            uint16_t overall_chunks = get_number_of_chunks( recv_msg_length );

            // Since we already have the first chunk we start the loop from index 1
            for( int i = 1; i < overall_chunks; ++i )
            {
                // chunk number is in param4
                cmd.param4 = compute_chunks_param( overall_chunks, i );

                auto ans = hw_monitor::send( cmd, p_response, locked_transfer );
                recv_msg.insert( recv_msg.end(), ans.begin(), ans.end() );
            }
        }
        return recv_msg;
    }

    void hw_monitor_extended_buffers::extended_send(command cmd, hwmon_response_type* p_response, bool locked_transfer) const
    {
        // copying the data, so that this param can be reused for the sending via HWM
        auto table_data = cmd.data;
        uint16_t overall_chunks = get_number_of_chunks(table_data.size());

        std::vector<uint8_t> answer;
        for (int i = 0; i < overall_chunks; ++i)
        {
            // preparing data to be sent in current chunk
            cmd.data = std::move(get_data_for_current_iteration(table_data, i));
            // chunk number is in param4
            cmd.param4 = compute_chunks_param(overall_chunks, i);

            hw_monitor::send(cmd, p_response, locked_transfer);
        }
    }

    std::vector<uint8_t> hw_monitor_extended_buffers::get_data_for_current_iteration(const std::vector<uint8_t>& table_data, int iteration) const
    {
        auto remaining_data_size_to_be_sent = table_data.size() - iteration * HW_MONITOR_COMMAND_SIZE;

        auto start_iterator_for_current_chunk = table_data.begin() + iteration * HW_MONITOR_COMMAND_SIZE;
        auto end_iterator_for_current_chunk = (remaining_data_size_to_be_sent >= HW_MONITOR_COMMAND_SIZE) ?
            start_iterator_for_current_chunk + HW_MONITOR_COMMAND_SIZE :
            start_iterator_for_current_chunk + remaining_data_size_to_be_sent;

        return std::vector<uint8_t>(start_iterator_for_current_chunk, end_iterator_for_current_chunk);
    }
        
    std::vector<uint8_t> hw_monitor_extended_buffers::send(std::vector<uint8_t> const& data) const
    {
        command cmd = build_command_from_data(data);

        hwm_buffer_type buffer_type = get_buffer_type(cmd);
        if (buffer_type == hwm_buffer_type::standard)
            return hw_monitor::send(data);

        // returning the hwmc answer with 4 bytes for opcode as header
        // this is needed because the hw_monitor::send with command is used, while
        // the hw_monitor::send with vector<uint8_t> has been used
        std::vector<uint8_t> ans_from_hwmc = send(cmd);
        std::vector<uint8_t> rv;
        uint32_t opcode_data = static_cast<uint32_t>(cmd.cmd);
        rv.insert(rv.end(), reinterpret_cast<uint8_t*>(&opcode_data), reinterpret_cast<uint8_t*>(&opcode_data) + sizeof(opcode_data));
        rv.insert(rv.end(), ans_from_hwmc.begin(), ans_from_hwmc.end());
        return rv;
    }
}
