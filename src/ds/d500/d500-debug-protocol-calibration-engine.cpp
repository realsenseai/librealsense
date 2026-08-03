// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <src/ds/d500/d500-debug-protocol-calibration-engine.h>
#include <src/ds/d500/d500-types/calibration-config.h>
#include "d500-device.h"

#include <cstring>


namespace librealsense
{
bool d500_debug_protocol_calibration_engine::check_buffer_size_from_get_calib_status(std::vector<uint8_t> res) const
{
    // the GET_CALIB_STATUS command will return:
    // - 3 bytes during the whole process
    // - 515 bytes (3 bytes + 512 bytes of the depth calibration) when the state is Complete

    bool is_size_ok = false;
    if (res.size() > 1)
    {
        // if state is not COMPLETE - answer should be returned without calibration table
        if (res[0] < static_cast<int>(calibration_state::COMPLETE) &&
            res.size() == (sizeof(d500_calibration_answer) - sizeof(ds::d500_coefficients_table)))
            is_size_ok = true;

        // if state is COMPLETE - answer should be returned with calibration table (modified by the calibration process)
        if (res[0] == static_cast<int>(calibration_state::COMPLETE) &&
            res.size() == sizeof(d500_calibration_answer))
            is_size_ok = true;
    }
    return is_size_ok;
}

bool d500_debug_protocol_calibration_engine::check_buffer_size_interactive(std::vector<uint8_t> res) const
{
    // D5x5 interactive triggered calibration: 3-byte header for IDLE/PROCESS, 535 bytes from HEALTH_CHECK onward
    // (3 header + 20 health + 512 candidate/committed table).
    if (res.size() < 3)
        return false;

    // Wire state byte 2 on this path means HEALTH_CHECK, not SUCCESS. Anything at or beyond that carries the full payload.
    const bool has_payload = res[0] >= 2;
    if (!has_payload)
        return res.size() == (sizeof(interactive_calibration_answer) - sizeof(calibration_health_metrics) - sizeof(ds::d500_coefficients_table));
    return res.size() == sizeof(interactive_calibration_answer);
}

void d500_debug_protocol_calibration_engine::update_triggered_calibration_status()
{
    if (!_dev)
        throw std::runtime_error("device has not been set");

    auto cmd = _dev->build_command(ds::GET_CALIB_STATUS);
    auto res = _dev->send_receive_raw_data(cmd);

    if (res.size() < 4)
        throw io_exception(rsutils::string::from() << "Triggered calibration status polling failure");

    // slicing 4 first bytes - opcode
    res.erase(res.begin(), res.begin() + 4);

    if (_interactive_triggered_calibration)
    {
        if (!check_buffer_size_interactive(res))
            throw std::runtime_error("GET_CALIB_STATUS (interactive) returned struct with wrong size");

        // Header (3 bytes) is always present; health + candidate table (532 more) only from HEALTH_CHECK onward.
        _interactive_ans = {};
        _interactive_ans.state    = static_cast<calibration_state >(res[0]);
        _interactive_ans.progress = static_cast<int8_t             >(res[1]);
        _interactive_ans.result   = static_cast<calibration_result >(res[2]);
        if (res.size() == sizeof(interactive_calibration_answer))
        {
            constexpr size_t health_off = 3;
            constexpr size_t table_off  = 3 + sizeof(calibration_health_metrics);
            std::memcpy(&_interactive_ans.health, res.data() + health_off, sizeof(_interactive_ans.health));
            std::memcpy(&_interactive_ans.depth_calibration, res.data() + table_off, sizeof(_interactive_ans.depth_calibration));
        }

        // Re-map wire state byte to enum: on the interactive path, byte 2 means HEALTH_CHECK, byte 3 FLASH_UPDATE, byte 4 COMPLETE.
        // If FW ships with a different numbering, degrade to PROCESS + LOG_WARNING rather than aborting the entire flow —
        // the wire contract is unverified until FW is available and a hard throw would brick every poll.
        const uint8_t raw_state = static_cast<uint8_t>(_interactive_ans.state);
        switch (raw_state)
        {
            case 0: _interactive_ans.state = calibration_state::IDLE;         break;
            case 1: _interactive_ans.state = calibration_state::PROCESS;      break;
            case 2: _interactive_ans.state = calibration_state::HEALTH_CHECK; break;
            case 3: _interactive_ans.state = calibration_state::FLASH_UPDATE; break;
            case 4: _interactive_ans.state = calibration_state::COMPLETE;     break;
            default:
                LOG_WARNING("GET_CALIB_STATUS (interactive) unknown state byte " << static_cast<int>(raw_state)
                            << " — treating as PROCESS");
                _interactive_ans.state = calibration_state::PROCESS;
                break;
        }
        return;
    }

    // checking size of received buffer
    if (!check_buffer_size_from_get_calib_status(res))
        throw std::runtime_error("GET_CALIB_STATUS returned struct with wrong size");

    _calib_ans = *reinterpret_cast<d500_calibration_answer*>(res.data());
}


std::vector<uint8_t> d500_debug_protocol_calibration_engine::run_triggered_calibration(calibration_mode _mode)
{
    if (!_dev)
        throw std::runtime_error("device has not been set");

    auto cmd = _dev->build_command(ds::SET_CALIB_MODE, static_cast<uint32_t>(_mode), 1 /*always*/);
    return _dev->send_receive_raw_data(cmd);
}

std::vector<uint8_t> d500_debug_protocol_calibration_engine::run_triggered_calibration_try(try_calibration_selection selection)
{
    if (!_dev)
        throw std::runtime_error("device has not been set");

    // Preserve the legacy `param2 = 1 /*always*/` marker so FW's SET_CALIB_MODE validity check still passes;
    // encode the NEW/OLD selector in param3 to avoid colliding with the marker (TRY_NEW = 0 would otherwise look like "not always").
    auto cmd = _dev->build_command(ds::SET_CALIB_MODE,
                                   static_cast<uint32_t>(calibration_mode::TRY),
                                   1 /*always, matches legacy*/,
                                   static_cast<uint32_t>(selection));
    return _dev->send_receive_raw_data(cmd);
}

calibration_state d500_debug_protocol_calibration_engine::get_triggered_calibration_state() const
{
    return _interactive_triggered_calibration ? _interactive_ans.state : _calib_ans.state;
}
calibration_result d500_debug_protocol_calibration_engine::get_triggered_calibration_result() const
{
    return _interactive_triggered_calibration ? _interactive_ans.result : _calib_ans.result;
}
int8_t d500_debug_protocol_calibration_engine::get_triggered_calibration_progress() const
{
    return _interactive_triggered_calibration ? _interactive_ans.progress : _calib_ans.progress;
}

calibration_health_metrics d500_debug_protocol_calibration_engine::get_triggered_calibration_health() const
{
    return _interactive_triggered_calibration ? _interactive_ans.health : calibration_health_metrics{};
}

std::vector<uint8_t> d500_debug_protocol_calibration_engine::get_calibration_table(std::vector<uint8_t>& current_calibration) const
{
    // Getting depth calibration table. RGB table is currently not supported by auto_calibrated_interface API

    // prepare command
    using namespace ds;
    auto cmd = _dev->build_command(ds::GET_HKR_CONFIG_TABLE,
                                   static_cast<int>(d500_calib_location::d500_calib_flash_memory),
                                   static_cast<int>(d500_calibration_table_id::depth_calibration_id),
                                   static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));

    // sending command
    auto calib = _dev->send_receive_raw_data(cmd);

    if (calib.size() < (sizeof(ds::table_header) + 4))
        throw std::runtime_error("GET_HKR_CONFIG_TABLE response is smaller then calibration header!");

    // slicing 4 first bytes - opcode
    calib.erase(calib.begin(), calib.begin() + 4);

    auto header = (ds::table_header*)(calib.data());
    if (calib.size() < sizeof(ds::table_header) + header->table_size)
        throw std::runtime_error("GET_HKR_CONFIG_TABLE response is smaller then expected table size!");

    return calib;
}

void d500_debug_protocol_calibration_engine::write_calibration(std::vector<uint8_t>& current_calibration) const
{
    auto table_header = reinterpret_cast<ds::table_header*>(current_calibration.data());
    table_header->crc32 = rsutils::number::calc_crc32(current_calibration.data() + sizeof(ds::table_header),
                                                      current_calibration.size() - sizeof(ds::table_header));

    // prepare command
    using namespace ds;
    auto cmd = _dev->build_command(ds::SET_HKR_CONFIG_TABLE,
                                   static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
                                   static_cast<int>(table_header->table_type),
                                   static_cast<int>(ds::d500_calib_type::d500_calib_dynamic), 0,
                                   current_calibration.data(), current_calibration.size());

    // sending command
    _dev->send_receive_raw_data(cmd);
}

std::string d500_debug_protocol_calibration_engine::get_calibration_config() const
{
    calibration_config_with_header* result;

    // prepare command
    using namespace ds;
    auto cmd = _dev->build_command(ds::GET_HKR_CONFIG_TABLE,
        static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
        static_cast<int>(ds::d500_calibration_table_id::calib_cfg_id),
        static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));

    // send command to device and get response (calibration config entry + header)
    std::vector< uint8_t > response = _dev->send_receive_raw_data(cmd);

    if (response.size() < (sizeof(calibration_config_with_header) + 4))
    {
        throw io_exception(rsutils::string::from() << "Calibration Config Read Failed");
    }

    // slicing 4 first bytes - opcode
    response.erase(response.begin(), response.begin() + 4);


    // check CRC before returning result
    auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(librealsense::table_header),
        sizeof(calibration_config));
    result = reinterpret_cast<calibration_config_with_header*>(response.data());
    if (computed_crc32 != result->get_table_header().get_crc32())
    {
        throw invalid_value_exception(rsutils::string::from() << "Calibration Config Invalid CRC Value");
    }

    rsutils::json j = result->get_calibration_config().to_json();
    return j.dump();
}

void d500_debug_protocol_calibration_engine::set_calibration_config(const std::string& calibration_config_json_str) const
{
    rsutils::json json_data = rsutils::json::parse(calibration_config_json_str);
    calibration_config calib_config(json_data["calibration_config"]);

    // calculate CRC
    uint32_t computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&calib_config), sizeof(calibration_config));

    // prepare vector of data to be sent (header + calibration_config)
    uint16_t version = ((uint16_t)0x01 << 8) | 0x01;  // major=0x01, minor=0x01 --> ver = major.minor
    uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
    table_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::calib_cfg_id), sizeof(calibration_config),
        calib_version, computed_crc32);
    calibration_config_with_header calib_config_with_header(header, calib_config);
    auto data_as_ptr = reinterpret_cast<const uint8_t*>(&calib_config_with_header);

    // prepare command
    using namespace ds;
    auto cmd = _dev->build_command(SET_HKR_CONFIG_TABLE,
        static_cast<int>(d500_calib_location::d500_calib_flash_memory),
        static_cast<int>(d500_calibration_table_id::calib_cfg_id),
        static_cast<int>(d500_calib_type::d500_calib_dynamic), 0,
        data_as_ptr, sizeof(calibration_config_with_header));

    // sending command
    _dev->send_receive_raw_data(cmd);
}

ds::d500_coefficients_table d500_debug_protocol_calibration_engine::get_depth_calibration() const
{
    return _interactive_triggered_calibration ? _interactive_ans.depth_calibration : _calib_ans.depth_calibration;
}

}// namespace librealsense