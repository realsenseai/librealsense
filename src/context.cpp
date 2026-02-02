// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include "device-info.h"

#include "backend-device-factory.h"
#ifdef BUILD_WITH_DDS
#include "dds/rsdds-device-factory.h"
#endif
#include "rscore-pp-block-factory.h"

#include <librealsense2/hpp/rs_types.hpp>  // rs2_devices_changed_callback
#include <librealsense2/rs.h>              // RS2_API_FULL_VERSION_STR
#include <src/librealsense-exception.h>

#include <rsutils/os/special-folder.h>
#include <rsutils/os/executable-name.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/string/from.h>
#include <rsutils/json.h>
#include <rsutils/json-config.h>
using json = rsutils::json;


namespace librealsense {


    static rsutils::json load_settings( rsutils::json const & context_settings )
    {
        // Allow ignoring of any other settings, global or not!
        if( ! context_settings.nested( "inherit" ).default_value( true ) )
            return context_settings;

        auto const filename = rsutils::os::get_special_folder( rsutils::os::special_folder::app_data ) + RS2_CONFIG_FILENAME;
        LOG_DEBUG( "Loading configuration file from: " << filename );
        auto config = rsutils::json_config::load_from_file( filename );

        if (config.is_discarded())
        {
            LOG_ERROR( "No valid configuration file found at : " << filename << " loading defaults" );
        }
        // Take only the 'context' part of it
        config = rsutils::json_config::load_settings( config, "context", "config-file" );

        // Patch the given context settings into the configuration
        config.override( context_settings, "context settings" );
        LOG_DEBUG( "Loaded context configuration: " << config.dump(4) );

        return config;
    }


    context::context( json const & settings )
    {
        static bool version_logged = false;
        if( ! version_logged )
        {
            version_logged = true;
            LOG_DEBUG( "Librealsense VERSION: " << RS2_API_FULL_VERSION_STR );
        }

         _settings = load_settings( settings );  // global | application | local
         _device_mask = _settings.nested( "device-mask" ).default_value< unsigned >( RS2_PRODUCT_LINE_ANY );
    }


    void context::create_factories( std::shared_ptr< context > const & sptr )
    {
        if( 0 == ( get_device_mask() & RS2_PRODUCT_LINE_SW_ONLY ) )
        {
            _factories.push_back( std::make_shared< backend_device_factory >(
                sptr,
                [this]( std::vector< std::shared_ptr< device_info > > const & removed,
                        std::vector< std::shared_ptr< device_info > > const & added )
                { invoke_devices_changed_callbacks( removed, added ); } ) );
        }

#ifdef BUILD_WITH_DDS
        _factories.push_back( std::make_shared< rsdds_device_factory >(
            sptr,
            [this]( std::vector< std::shared_ptr< device_info > > const & removed,
                    std::vector< std::shared_ptr< device_info > > const & added )
            { invoke_devices_changed_callbacks( removed, added ); } ) );
#endif
    }


    /*static*/ std::shared_ptr< context > context::make( json const & settings )
    {
        std::shared_ptr< context > sptr( new context( settings ) );
        sptr->create_factories( sptr );
        return sptr;
    }


    /*static*/ std::shared_ptr< context > context::make( char const * json_settings )
    {
        if ( ! json_settings || ! *json_settings )
            return make(json::object());

        json raw_j = json::parse(json_settings, nullptr, false);

        return make(raw_j);
    }


    context::~context()
    {
    }


    /*static*/ unsigned context::combine_device_masks( unsigned const requested_mask, unsigned const mask_in_settings )
    {
        unsigned mask = requested_mask;
        // The normal bits enable, so enable only those that are on
        mask &= mask_in_settings & ~RS2_PRODUCT_LINE_SW_ONLY;
        // But the above turned off the SW-only bits, so turn them back on again
        if( ( mask_in_settings & RS2_PRODUCT_LINE_SW_ONLY ) || ( requested_mask & RS2_PRODUCT_LINE_SW_ONLY ) )
            mask |= RS2_PRODUCT_LINE_SW_ONLY;
        return mask;
    }


    std::vector< std::shared_ptr< device_info > > context::query_devices( int requested_mask ) const
    {
        std::vector< std::shared_ptr< device_info > > list;
        for( auto & factory : _factories )
        {
            for( auto & dev_info : factory->query_devices( requested_mask ) )
            {
                LOG_DEBUG( "... " << dev_info->get_address() );
                list.push_back( dev_info );
            }
        }
        for( auto & item : _user_devices )
        {
            if( auto dev_info = item.second.lock() )
            {
                LOG_DEBUG( "... " << dev_info->get_address() );
                list.push_back( dev_info );
            }
        }
        LOG_DEBUG( "Found " << list.size() << " RealSense devices (0x" << std::hex << requested_mask
                            << " requested & 0x" << get_device_mask() << " from device-mask in settings)" << std::dec );
        return list;
    }


    void context::invoke_devices_changed_callbacks(
        std::vector< std::shared_ptr< device_info > > const & rs2_devices_info_removed,
        std::vector< std::shared_ptr< device_info > > const & rs2_devices_info_added )
    {
        _devices_changed.raise( rs2_devices_info_removed, rs2_devices_info_added );
    }


    rsutils::subscription context::on_device_changes( devices_changed_callback && callback )
    {
        return _devices_changed.subscribe( std::move( callback ) );
    }


    void context::add_device( std::shared_ptr< device_info > const & dev )
    {
        auto address = dev->get_address();

        auto it = _user_devices.find( address );
        if( it != _user_devices.end() && it->second.lock() )
            throw librealsense::invalid_value_exception( "device already in context: " + address );
        _user_devices[address] = dev;

        std::vector< std::shared_ptr< device_info > > rs2_device_info_added{ dev };
        invoke_devices_changed_callbacks( {}, rs2_device_info_added );
    }


    void context::remove_device( std::shared_ptr< device_info > const & dev )
    {
        auto address = dev->get_address();

        auto it = _user_devices.find( address );
        if(it == _user_devices.end() )
            return;  // Why not throw?!
        auto dev_info = it->second.lock();
        _user_devices.erase(it);

        if( dev_info )
        {
            std::vector< std::shared_ptr< device_info > > rs2_device_info_removed{ dev_info };
            invoke_devices_changed_callbacks( rs2_device_info_removed, {} );
        }
    }


    std::shared_ptr< processing_block_interface > context::create_pp_block( std::string const & name,
                                                                            rsutils::json const & settings )
    {
        return rscore_pp_block_factory().create_pp_block( name, settings );
    }

}  // namespace librealsense
