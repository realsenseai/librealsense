// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-config-tables.h"
#include "d500-device.h"
#include "d500-private.h"
#include "d500-types/safety-preset.h"
#include "d500-types/safety-interface-config.h"
#include "d500-types/application-config.h"

#include <rsutils/number/crc32.h>
#include <rsutils/json.h>

namespace librealsense
{
    namespace
    {
        // Which config table an exposed parameter is stored in. A parameter may map to more than
        // one table (e.g. a value written to several tables), so set_parameters groups by table.
        enum class cfg_table { safety_interface_config };

        struct param_target
        {
            cfg_table table;
            std::vector< std::string > path;  // nested json keys within the table
        };

        // The curated set of parameters exposed to users. Add new parameters here; a parameter that
        // lives in several tables simply lists several targets.
        const std::map< std::string, std::vector< param_target > >& exposed_params()
        {
            static const std::map< std::string, std::vector< param_target > > params = {
                { "grid_cell_size",        { { cfg_table::safety_interface_config, { "occupancy_grid_params", "grid_cell_size" } } } },
                { "surface_height",        { { cfg_table::safety_interface_config, { "occupancy_grid_params", "surface_height" } } } },
                { "cell_threshold_factor", { { cfg_table::safety_interface_config, { "occupancy_grid_params", "cell_threshold_factor" } } } },
                { "polynomial_bias",       { { cfg_table::safety_interface_config, { "occupancy_grid_params", "polynomial_bias" } } } },
            };
            return params;
        }

        std::string read_cfg_table( const d500_device& dev, cfg_table table )
        {
            switch( table )
            {
            case cfg_table::safety_interface_config: return dev.get_safety_interface_config( RS2_CALIB_LOCATION_FLASH );
            }
            throw invalid_value_exception( "unknown config table" );
        }

        void write_cfg_table( const d500_device& dev, cfg_table table, const std::string& json_str )
        {
            switch( table )
            {
            case cfg_table::safety_interface_config: dev.set_safety_interface_config( json_str ); return;
            }
            throw invalid_value_exception( "unknown config table" );
        }

        void set_at( nlohmann::json& root, const std::vector< std::string >& path, const nlohmann::json& value )
        {
            nlohmann::json* node = &root;
            for( size_t i = 0; i + 1 < path.size(); ++i )
                node = &( ( *node )[path[i]] );
            ( *node )[path.back()] = value;
        }

        const nlohmann::json& get_at( const nlohmann::json& root, const std::vector< std::string >& path )
        {
            const nlohmann::json* node = &root;
            for( const auto& key : path )
                node = &node->at( key );
            return *node;
        }
    }

    std::string d500_device::get_safety_preset(int index) const
    {
        safety_preset_with_header* result;

        // prepare command
        command cmd(ds::SAFETY_PRESET_READ);
        cmd.require_response = true;
        cmd.param1 = index;

        // send command to device and get response (safety_preset entry + safety_preset_header)
        std::vector< uint8_t > response = _hw_monitor->send(cmd);
        if (response.size() < sizeof(safety_preset_with_header))
        {
            throw io_exception(rsutils::string::from() << "Safety preset read at index=" << index << " failed");
        }

        // check CRC before returning result
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(safety_preset_header),
            sizeof(safety_preset));
        result = reinterpret_cast<safety_preset_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Safety preset invalid CRC value");
        }

        rsutils::json j = result->get_safety_preset().to_json();
        return j.dump();
    }

    void d500_device::set_safety_preset(int index, const std::string& sp_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(sp_json_str);
        safety_preset sp(json_data["safety_preset"]);

        //calculate CRC
        auto computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&sp), sizeof(safety_preset));

        // prepare vector of data to be sent (header + sp)
        uint16_t version = ((uint16_t)0x03 << 8) | 0x01;  // major=0x03, minor=0x01 --> ver = major.minor
        safety_preset_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::safety_preset_id), sizeof(safety_preset_with_header), computed_crc32);
        safety_preset_with_header sp_with_header(header, sp);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&sp_with_header);

        // prepare command
        command cmd(ds::SAFETY_PRESET_WRITE);
        cmd.param1 = index;
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(safety_preset_with_header));
        cmd.require_response = true;

        // send command
        _hw_monitor->send(cmd);
    }

    std::string d500_device::get_safety_interface_config(rs2_calib_location loc) const
    {
        if (loc != RS2_CALIB_LOCATION_FLASH && loc != RS2_CALIB_LOCATION_RAM)
            throw io_exception(rsutils::string::from() << "Safety Interface Config can be read only from Flash or RAM");
        ds::d500_calib_location d500_loc = (loc == RS2_CALIB_LOCATION_RAM) ? ds::d500_calib_location::d500_calib_ram_memory :
            ds::d500_calib_location::d500_calib_flash_memory;

        safety_interface_config_with_header* result;

        // prepare command
        command cmd(ds::GET_HKR_CONFIG_TABLE,
            static_cast<int>(d500_loc),
            static_cast<int>(ds::d500_calibration_table_id::safety_interface_cfg_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_gold));
        cmd.require_response = true;

        // send command to device and get response (safety_interface_config entry + header)
        std::vector< uint8_t > response = _hw_monitor->send(cmd);
        if (response.size() < sizeof(safety_interface_config_with_header))
        {
            throw io_exception(rsutils::string::from() << "Safety Interface Config failed");
        }

        // check CRC before returning result
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(table_header), sizeof(safety_interface_config));
        result = reinterpret_cast<safety_interface_config_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Safety Interface Config invalid CRC value");
        }

        rsutils::json j = result->get_safety_interface_config().to_json();
        return j.dump();
    }

    void d500_device::set_safety_interface_config(const std::string& sic_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(sic_json_str);
        safety_interface_config sic(json_data["safety_interface_config"]);

        // calculate CRC
        uint32_t computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&sic), sizeof(safety_interface_config));

        // prepare vector of data to be sent (header + safety_interface_config)
        uint16_t version = ((uint16_t)0x04 << 8) | 0x00;  // major=0x04, minor=0x00 --> ver = major.minor
        uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
        table_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::safety_interface_cfg_id), sizeof(safety_interface_config), calib_version, computed_crc32);
        safety_interface_config_with_header sic_with_header(header, sic);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&sic_with_header);

        // prepare command
        command cmd(ds::SET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::safety_interface_cfg_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_gold));
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(safety_interface_config_with_header));
        cmd.require_response = true;

        // send command
        _hw_monitor->send(cmd);
    }

    std::string d500_device::get_application_config() const
    {
        application_config_with_header* result;

        // prepare command
        command cmd(ds::GET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::app_config_table_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));
        cmd.require_response = true;

        // send command to device and get response (application_config entry + header)
        std::vector< uint8_t > response = _hw_monitor->send(cmd);
        if (response.size() < sizeof(application_config_with_header))
        {
            throw io_exception(rsutils::string::from() << "Applicaion Config Read Failed");
        }

        // check CRC before returning result
        auto computed_crc32 = rsutils::number::calc_crc32(response.data() + sizeof(table_header), sizeof(application_config));
        result = reinterpret_cast<application_config_with_header*>(response.data());
        if (computed_crc32 != result->get_table_header().get_crc32())
        {
            throw invalid_value_exception(rsutils::string::from() << "Applicaion Config Invalid CRC Value");
        }

        rsutils::json j = result->get_application_config().to_json();
        return j.dump();
    }

    void d500_device::set_application_config(const std::string& application_config_json_str) const
    {
        rsutils::json json_data = rsutils::json::parse(application_config_json_str);
        application_config app_config(json_data["application_config"]);

        // calculate CRC
        uint32_t computed_crc32 = rsutils::number::calc_crc32(reinterpret_cast<const uint8_t*>(&app_config), sizeof(application_config));

        // prepare vector of data to be sent (header + application_config)
        uint16_t version = ((uint16_t)0x01 << 8) | 0x02;  // major=0x01, minor=0x02 --> ver = major.minor
        uint32_t calib_version = 0;  // ignoring this field, as requested by sw architect
        table_header header(version, static_cast<uint16_t>(ds::d500_calibration_table_id::app_config_table_id), sizeof(application_config), calib_version, computed_crc32);
        application_config_with_header app_config_with_header(header, app_config);
        auto data_as_ptr = reinterpret_cast<const uint8_t*>(&app_config_with_header);

        // prepare command
        command cmd(ds::SET_HKR_CONFIG_TABLE,
            static_cast<int>(ds::d500_calib_location::d500_calib_flash_memory),
            static_cast<int>(ds::d500_calibration_table_id::app_config_table_id),
            static_cast<int>(ds::d500_calib_type::d500_calib_dynamic));
        cmd.data.insert(cmd.data.end(), data_as_ptr, data_as_ptr + sizeof(application_config_with_header));
        cmd.require_response = true;

        // send command
        _hw_monitor->send(cmd);
    }

    void d500_device::set_parameters(const std::string& params_json_str) const
    {
        nlohmann::json input = nlohmann::json::parse(params_json_str);
        if (!input.is_object())
            throw invalid_value_exception("parameters must be a JSON object");

        // group requested updates by the table they live in, so each table is written only once
        std::map<cfg_table, std::vector<std::pair<const std::vector<std::string>*, nlohmann::json>>> updates;
        for (auto it = input.begin(); it != input.end(); ++it)
        {
            auto param = exposed_params().find(it.key());
            if (param == exposed_params().end())
                throw invalid_value_exception("unknown parameter: " + it.key());
            for (const auto& target : param->second)
                updates[target.table].push_back({ &target.path, it.value() });
        }

        for (const auto& table_updates : updates)
        {
            nlohmann::json full = nlohmann::json::parse(read_cfg_table(*this, table_updates.first));
            for (const auto& update : table_updates.second)
                set_at(full, *update.first, update.second);
            write_cfg_table(*this, table_updates.first, full.dump());
        }
    }

    std::string d500_device::get_parameters() const
    {
        std::map<cfg_table, nlohmann::json> table_cache;
        nlohmann::json out = nlohmann::json::object();
        for (const auto& param : exposed_params())
        {
            const auto& target = param.second.front();  // any target holds the value; read the first
            auto cached = table_cache.find(target.table);
            if (cached == table_cache.end())
                cached = table_cache.emplace(target.table, nlohmann::json::parse(read_cfg_table(*this, target.table))).first;
            out[param.first] = get_at(cached->second, target.path);
        }
        return out.dump();
    }
}
