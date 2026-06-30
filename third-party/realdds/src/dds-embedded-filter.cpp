// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <realdds/dds-embedded-filter.h>
#include <realdds/dds-device.h>
#include <realdds/dds-stream-base.h>
#include <realdds/dds-exceptions.h>

#include <rsutils/json.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>  // for std::find_if

using rsutils::json;


namespace realdds {

// Base class implementation
dds_embedded_filter::dds_embedded_filter()
    : _name("")
    , _options()
{
}

void dds_embedded_filter::init(const std::string& name)
{
    _name = name;
}

void dds_embedded_filter::init_options(const rsutils::json& options_j)
{
    if (!_options.empty())
        DDS_THROW(runtime_error, "filter '" + _name + "' options are already initialized");

    dds_options options;
    for (auto& option_j : options_j)
    {
        auto option = dds_option::from_json(option_j);
        options.push_back(option);
    }

    _options = options;
}

void dds_embedded_filter::init_stream(std::shared_ptr< dds_stream_base > const& stream)
{
    if (_stream.lock())
        DDS_THROW(runtime_error, "filter '" + get_name() + "' already has a stream");
    if (!stream)
        DDS_THROW(runtime_error, "null stream");
    _stream = stream;
}

void dds_embedded_filter::verify_uninitialized() const
{
    if (!_options.empty()) {
        DDS_THROW( runtime_error, "Cannot re-initialize embedded filter");
    }
}

rsutils::json dds_embedded_filter::props_to_json() const
{
    json props;
    props["name"] = _name;
    if (!_options.empty()) {
        props["options"] = dds_options_to_json(_options);
    }
    auto stream = _stream.lock();
    if (stream) {
        props["stream_type"] = stream->name();
    }
    return props;
}

rsutils::json dds_embedded_filter::to_json() const
{
    return props_to_json();
}

rsutils::json dds_embedded_filter::get_options_json()
{
    return dds_options_to_json(_options);
}

std::shared_ptr<dds_embedded_filter> dds_embedded_filter::from_json(const rsutils::json& j)
{
    std::string name_str;
    if (j.contains("name")) 
    {
        name_str = j["name"].get<std::string>();
    }
    else
    {
        DDS_THROW(runtime_error, "missing name");
    }
    // create the appropriate filter type
    auto filter = create_embedded_filter(name_str);
    filter->init(j["name"].get<std::string>());
    
    if (j.contains("options")) {
        filter->init_options(j["options"]);
    }
    
    return filter;
}

void dds_embedded_filter::check_options(const json& options) const
{
    if (!options.exists())
        DDS_THROW(runtime_error, "invalid options");

    if (options.is_null())
        DDS_THROW(runtime_error, "options null");
}

void dds_embedded_filter::set_current_value(const std::string& key, const rsutils::json& value)
{
    _current_values[key] = value;
}

rsutils::json dds_embedded_filter::get_current_value(const std::string& key) const
{
    auto it = _current_values.find(key);
    if (it != _current_values.end()) {
        return it->second;
    }
    return json{};
}

// Decimation filter implementation
dds_decimation_filter::dds_decimation_filter()
    : dds_embedded_filter()
{
    _name = "Decimation Filter";
}

void dds_embedded_filter::set_options(const rsutils::json& options)
{
    check_options(options);
    
    // Expect options to be an object where each key-value pair represents an option
    if (options.is_object()) {
        for (auto it = options.begin(); it != options.end(); ++it) {
            const std::string& key = it.key();
            const auto& value = it.value();
            
            auto found_option = std::find_if(_options.begin(), _options.end(),
                [&key](const std::shared_ptr<dds_option>& option) {
                    return option->get_name() == key;
                });
                
            if (found_option != _options.end()) {
                (*found_option)->set_value(value);
            } else {
                DDS_THROW(runtime_error, "Option '" + key + "' not found in filter");
            }
        }
    } else {
        DDS_THROW(runtime_error, "Options must be provided as an object");
    }
}

// Temporal filter implementation
dds_temporal_filter::dds_temporal_filter()
    : dds_embedded_filter()
{
    _name = "Temporal Filter";
}

// Improved Close Range Depth filter implementation
dds_close_range_filter::dds_close_range_filter()
    : dds_embedded_filter()
{
    _name = "Improved Close Range Depth";
}

// Factory and utility functions
std::shared_ptr<dds_embedded_filter> create_embedded_filter(const std::string& filter_name)
{
    if ( filter_name == "Decimation Filter")
        return std::make_shared<dds_decimation_filter>();
    else if ( filter_name == "Temporal Filter")
        return std::make_shared<dds_temporal_filter>();
    else if ( filter_name == "Improved Close Range Depth")
        return std::make_shared<dds_close_range_filter>();
    else
        DDS_THROW(runtime_error, "Unknown embedded filter name: " + filter_name);
}

}  // namespace realdds
