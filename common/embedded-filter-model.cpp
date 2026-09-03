// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <string>
#include "subdevice-model.h"
#include "embedded-filter-model.h"
#include "viewer.h"


namespace rs2
{
    embedded_filter_model::embedded_filter_model(
        subdevice_model* owner,
        const rs2_embedded_filter_type& type,
        std::shared_ptr<rs2::embedded_filter> filter,
        viewer_model& viewer,
        std::string& error_message)
        : _embedded_filter(filter), _viewer(viewer), _destructing(false)
    {
        _name = rs2_embedded_filter_type_to_string(type);

        std::stringstream ss;
        ss << "##" << ((owner) ? owner->dev.get_info(RS2_CAMERA_INFO_NAME) : _name)
            << "/" << ((owner) ? (*owner->s).get_info(RS2_CAMERA_INFO_NAME) : "_")
            << "/" << (long long)this;

        // following method also updates the data member "_enabled"
        populate_options(ss.str().c_str(), owner, owner ? &owner->_options_invalidated : nullptr, error_message);
    }


    embedded_filter_model::~embedded_filter_model()
    {
        _destructing.store(true);
        try
        {
            _embedded_filter->on_options_changed([](const options_list& list) {});
        }
        catch (...)
        {
        }
    }

    std::vector< option_model * > embedded_filter_model::drawable_options()
    {
        std::vector< option_model * > options;
        for( auto & id_model : _options_id_to_model )
            if( id_model.first != RS2_OPTION_EMBEDDED_FILTER_ENABLED )
                options.push_back( &id_model.second );
        return options;
    }

    void embedded_filter_model::embedded_filter_enable_disable(bool actual)
    {
        _embedded_filter->set_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED, actual ? 1.0f : 0.0f);
        _enabled = _embedded_filter->get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);
    }

    void embedded_filter_model::populate_options(const std::string& opt_base_label,
        subdevice_model* model,
        bool* options_invalidated,
        std::string& error_message)
    {
        for (option_value option : _embedded_filter->get_supported_option_values())
        {
            // Build the model first and insert only on success: an option whose range cannot be read
            // throws, and map::operator[] would leave a default-constructed (null-endpoint) entry
            // behind. Isolate per option so one bad control does not drop the rest.
            try
            {
                auto om = create_option_model( option,
                                               opt_base_label,
                                               model,
                                               _embedded_filter,
                                               model ? &model->_options_invalidated : nullptr,
                                               error_message );
                _options_id_to_model[option->id] = std::move( om );
            }
            catch( const std::exception & e )
            {
                if( _viewer.not_model )
                    _viewer.not_model->add_log( e.what(), RS2_LOG_SEVERITY_WARN );
            }
        }
        _enabled = _embedded_filter->get_option(RS2_OPTION_EMBEDDED_FILTER_ENABLED);

        try
        {
            _embedded_filter->on_options_changed([this](const options_list& list)
                {
                    for (auto changed_option : list)
                    {
                        auto it = _options_id_to_model.find(changed_option->id);
                        // Callback runs in different context, checking _options_id_to_model still valid
                        if (it != _options_id_to_model.end() && !_destructing)
                        {
                            it->second.update_value(changed_option, *_viewer.not_model);
                            if (it->first == RS2_OPTION_EMBEDDED_FILTER_ENABLED)
                            {
                                // rs2_option_value is a union over as_float/as_integer. For a FLOAT
                                // option, only as_float is initialized - reading as_integer would
                                // pick up the uninitialized upper 4 bytes (int64_t vs float).
                                if (changed_option->is_valid)
                                    _enabled = (changed_option->type == RS2_OPTION_TYPE_FLOAT)
                                             ? (changed_option->as_float != 0.0f)
                                             : (changed_option->as_integer != 0);
                            }
                        }
                    }
                });
        }
        catch (const std::exception& e)
        {
            if (_viewer.not_model)
                _viewer.not_model->add_log(e.what(), RS2_LOG_SEVERITY_WARN);
        }
    }
}
