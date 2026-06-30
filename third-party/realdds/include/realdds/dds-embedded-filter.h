// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json.h>
#include <rsutils/type/ip-address.h>
#include "dds-option.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace realdds {

class dds_device;
class dds_stream_base;

// Class dds_embedded_filter - Handles DDS communication, JSON serialization, stream association
// Abstract base class for all embedded filters.
// Embedded filter types are Decimation and Temporal filter
class dds_embedded_filter
{
protected:
    std::string _name;

    dds_options _options;
    std::map< std::string, rsutils::json > _current_values;
    std::weak_ptr< dds_device > _dev;

private:
    friend class dds_stream_base;
    std::weak_ptr< dds_stream_base > _stream;
    void init_stream( std::shared_ptr< dds_stream_base > const & );

public:
    dds_embedded_filter();

    // Initialization functions - must be called before first set_value()
    virtual void init( const std::string & name);
    virtual void init_options( rsutils::json const & options );

    // Core functionality
    rsutils::json get_options_json();
    const dds_options& get_options() const { return _options; }
    void set_options(rsutils::json const& options);

    // Getters
    std::string const & get_name() const { return _name; }
    std::shared_ptr< dds_stream_base > get_stream() const { return _stream.lock(); }

    // JSON serialization
    virtual rsutils::json to_json() const;
    static std::shared_ptr< dds_embedded_filter > from_json( rsutils::json const & j);

protected:
    void verify_uninitialized() const;  // throws if already has a value (use by init_ functions)
    virtual rsutils::json props_to_json() const;

    // Helper methods for derived classes
    void set_current_value( std::string const & key, rsutils::json const & value );
    rsutils::json get_current_value( std::string const & key ) const;
    void check_options( rsutils::json const & options ) const;
};

// Decimation filter implementation
class dds_decimation_filter : public dds_embedded_filter
{
public:
    dds_decimation_filter();
    virtual ~dds_decimation_filter() = default;
};

// Temporal filter implementation
class dds_temporal_filter : public dds_embedded_filter
{

public:
    dds_temporal_filter();
    virtual ~dds_temporal_filter() = default;
};

// Improved Close Range Depth filter implementation
class dds_close_range_filter : public dds_embedded_filter
{
public:
    dds_close_range_filter();
    virtual ~dds_close_range_filter() = default;
};

typedef std::vector< std::shared_ptr< dds_embedded_filter > > dds_embedded_filters;

// Factory function to create appropriate filter based on type
std::shared_ptr< dds_embedded_filter > create_embedded_filter(const std::string& filter_name);

}  // namespace realdds
