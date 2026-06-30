// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <string>
#include "subdevice-model.h"
#include "processing-block-model.h"
#include "viewer.h"


namespace rs2
{
    processing_block_model::processing_block_model(
        subdevice_model* owner,
        const std::string& name,
        std::shared_ptr<rs2::filter> block,
        std::function<rs2::frame(rs2::frame)> invoker,
        std::string& error_message, bool enable)
        : _owner(owner), _name(name), _block(block), _invoker(invoker), _enabled(enable)
    {
        std::stringstream ss;
        ss << "##" << ((owner) ? owner->dev.get_info(RS2_CAMERA_INFO_NAME) : _name)
            << "/" << ((owner) ? (*owner->s).get_info(RS2_CAMERA_INFO_NAME) : "_")
            << "/" << (long long)this;

        if (_owner)
            _full_name = get_post_processing_device_sensor_name(_owner) + "." + _name;
        else
            _full_name = _name;

        _enabled = restore_processing_block(_full_name.c_str(),
            block, _enabled);

        populate_options(ss.str().c_str(), owner, owner ? &owner->_options_invalidated : nullptr, error_message);
    }

    option_model * processing_block_model::get_option_model( rs2_option opt )
    {
        auto it = _options_id_to_model.find( opt );
        return it == _options_id_to_model.end() ? nullptr : &it->second;
    }

    void processing_block_model::save_to_config_file()
    {
        save_processing_block_to_config_file(_full_name.c_str(), _block, _enabled);
    }

    void processing_block_model::draw_options( viewer_model & viewer,
                                               bool update_read_only_options,
                                               bool is_streaming,
                                               std::string & error_message )
    {
        for( auto & id_model : _options_id_to_model )
        {
            if( viewer.is_option_skipped( id_model.first ) )
                continue;
            
            switch( id_model.first )
            {
            case RS2_OPTION_MIN_DISTANCE:
            case RS2_OPTION_MAX_DISTANCE:
            case RS2_OPTION_HISTOGRAM_EQUALIZATION_ENABLED:
                id_model.second.update_all_fields( error_message, *viewer.not_model );
                break;
            }

            id_model.second.draw_option( update_read_only_options, is_streaming, error_message, *viewer.not_model );
        }
    }


    void processing_block_model::populate_options(const std::string& opt_base_label,
        subdevice_model* model,
        bool* options_invalidated,
        std::string& error_message)
    {
        for( option_value option : _block->get_supported_option_values() )
        {
            option_model om = create_option_model( option,
                                                   opt_base_label,
                                                   model,
                                                   _block,
                                                   model ? &model->_options_invalidated : nullptr,
                                                   error_message );
            // Software filter: write synchronously (no FW round-trip) so the value is read back
            // and the control doesn't revert to a stale value.
            om.write_synchronously = true;
            _options_id_to_model[option->id] = om;
        }
    }

    bool restore_processing_block(const char* name,
        std::shared_ptr<rs2::processing_block> pb, bool enable)
    {
        for (auto opt : pb->get_supported_option_values())
        {
            std::string key = name;
            key += ".";
            key += pb->get_option_name(opt->id);
            if (config_file::instance().contains(key.c_str()))
            {
                float val = config_file::instance().get(key.c_str());
                try
                {
                    auto range = pb->get_option_range(opt->id);
                    if (val >= range.min && val <= range.max)
                        pb->set_option(opt->id, val);
                }
                catch (...)
                {
                }
            }
        }

        std::string key = name;
        key += ".enabled";
        if (config_file::instance().contains(key.c_str()))
        {
            return config_file::instance().get(key.c_str());
        }
        return enable;
    }

    void save_processing_block_to_config_file(const char* name,
        std::shared_ptr<rs2::processing_block> pb, bool enable)
    {
        for (auto opt : pb->get_supported_options())
        {
            auto val = pb->get_option(opt);
            std::string key = name;
            key += ".";
            key += pb->get_option_name(opt);
            config_file::instance().set(key.c_str(), val);
        }

        std::string key = name;
        key += ".enabled";
        config_file::instance().set(key.c_str(), enable);
    }
}
