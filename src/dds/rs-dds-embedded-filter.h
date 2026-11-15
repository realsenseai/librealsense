// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/embedded-filter-interface.h>

#include <functional>
#include <memory>
#include <rsutils/json.h>


namespace realdds
{
    class dds_embedded_filter;
}


namespace librealsense {


// This class is used to group some commonalities between different embedded filter types
class rs_dds_embedded_filter 
{
public:
    typedef std::function< void( rsutils::json value ) > set_embedded_filter_callback;
    typedef std::function< rsutils::json() > query_embedded_filter_callback;
    virtual void add_option(std::shared_ptr< realdds::dds_option > option) = 0;

protected:
    std::shared_ptr< realdds::dds_embedded_filter > _dds_ef;
    set_embedded_filter_callback _set_ef_cb;
    query_embedded_filter_callback _query_ef_cb;

public:
    rs_dds_embedded_filter( const std::shared_ptr< realdds::dds_embedded_filter > & dds_embedded_filter, 
        set_embedded_filter_callback set_embedded_filter_cb,
        query_embedded_filter_callback query_embedded_filter_cb );

protected:
    static rsutils::json dds_option_to_name_and_value_json(std::shared_ptr<realdds::dds_option> option, const rsutils::json& value);
};

}  // namespace librealsense
