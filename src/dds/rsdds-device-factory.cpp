// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-4 RealSense, Inc. All Rights Reserved.

#include "rsdds-device-factory.h"
#include "context.h"
#include "rs-dds-device-info.h"

#include <librealsense2/h/rs_context.h>  // RS2_PRODUCT_LINE_...

#include <realdds/dds-device-watcher.h>
#include <realdds/dds-participant.h>
#include <realdds/dds-device.h>
#include <realdds/dds-serialization.h>
#include <realdds/topics/device-info-msg.h>

#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/shared-ptr-singleton.h>
#include <rsutils/os/executable-name.h>
#include <rsutils/string/slice.h>
#include <rsutils/string/from.h>
#include <rsutils/signal.h>
#include <rsutils/json.h>

#include <mutex>


using rsutils::json;


namespace librealsense {


// The device-watcher is a singleton per domain. It is held alive by the device-factory below, which is held per
// context. I.e., as long as a context is alive, it stays alive alongside the participant.
//
// We are responsible for exposing the single notification from the dds-device-watcher to several subscribers:
// one device-watcher to many contexts, each with further subscriptions.
//
class rsdds_watcher_singleton
{
    std::shared_ptr< realdds::dds_device_watcher > const _device_watcher;
    using signal = rsutils::signal< std::shared_ptr< realdds::dds_device > const &, bool /*added*/ >;
    signal _callbacks;

public:
    rsdds_watcher_singleton( std::shared_ptr< realdds::dds_participant > const & participant,
                             size_t initialization_timeout_ms = 5000,
                             bool partial_device_allowed = false )
        : _device_watcher( std::make_shared< realdds::dds_device_watcher >( participant ) )
    {
        assert( _device_watcher->is_stopped() );

        _device_watcher->on_device_added(
            [this, initialization_timeout_ms, partial_device_allowed]( std::shared_ptr< realdds::dds_device > const & dev )
            {
                try
                {
                    // Make sure handshake was (even partially) performed, might throw
                    if( ! dev->wait_until_ready( initialization_timeout_ms, partial_device_allowed ) )
                        LOG_ERROR( "Discovered DDS device " << dev->debug_name()
                                   << " failed to be ready within timeout, using partial capabilities." );
                    _callbacks.raise( dev, true );
                }
                catch (std::runtime_error e)
                {
                    LOG_ERROR( "Discovered DDS device failed to be ready within timeout" << e.what() );
                }
            } );

        _device_watcher->on_device_removed( [this]( std::shared_ptr< realdds::dds_device > const & dev )
                                            { _callbacks.raise( dev, false ); } );

        _device_watcher->start();
    }

    rsutils::subscription subscribe( signal::callback && cb ) { return _callbacks.subscribe( std::move( cb ) ); }

    std::shared_ptr< realdds::dds_device_watcher > const get_device_watcher() const { return _device_watcher; }
};


// We manage one participant and device-watcher per domain:
// Two contexts with the same domain-id will share the same participant and watcher, while a third context on a
// different domain will have its own.
//
struct domain_context
{
    rsutils::shared_ptr_singleton< realdds::dds_participant > participant;
    rsutils::shared_ptr_singleton< rsdds_watcher_singleton > device_watcher;
    int query_devices_max = 0;
    int query_devices_min = 0;
    std::mutex wait_mutex;
};
//
// Domains are mapped by ID:
// Two contexts with the same participant name on different domain-ids are using two different participants!
//
static std::map< realdds::dds_domain_id, domain_context > domain_context_by_id;
static std::mutex domain_context_by_id_mutex;


rsdds_device_factory::rsdds_device_factory( std::shared_ptr< context > const & ctx, callback && cb )
    : super( ctx )
{
    auto dds_settings = ctx->get_settings().nested( std::string( "dds", 3 ) );
    if( dds_settings.nested( std::string( "enabled", 7 ) ).default_value( false ) )
    {
        auto domain_id = dds_settings.nested( std::string( "domain", 6 ) ).default_value< realdds::dds_domain_id >( 0 );
        auto participant_name_j = dds_settings.nested( std::string( "participant", 11 ) );
        auto participant_name = participant_name_j.default_value( rsutils::os::executable_name() );

        std::lock_guard< std::mutex > lock( domain_context_by_id_mutex );
        auto & domain = domain_context_by_id[domain_id];
        _participant = domain.participant.instance();
        if( ! _participant->is_valid() )
        {
            realdds::dds_participant::qos qos( participant_name );  // default settings
            
            // As a client, we send messages to a server; sometimes big messages. E.g., for DFU these may be up to
            // 20MB... flexible messages are up to 4K. The UDP protocol is supposed to break messages (by default, up to
            // 64K) into fragmented packed, but the server may not be able to handle these; certain hardware cannot
            // handle more than 1500 bytes!
            // 
            // FastDDS does provide a 'udp/max-message-size' setting (as of v2.10.4), but this applies to both send and
            // receive. We want to only limit sends! So we use a new property of FastDDS:
            qos.properties().properties().emplace_back( "fastdds.max_message_size", "1470" );
            // (overridable with "max-out-message-bytes" in our settings)

            // Create a flow-controller that will be used for DFU, because of similar possible IP stack limitations at
            // the server side: if we send too many packets all at once, some servers get overloaded...
            auto dfu_flow_control = std::make_shared< eprosima::fastdds::rtps::FlowControllerDescriptor >();
            dfu_flow_control->name = "dfu";
            // Max bytes to be sent to network per period; [1, 2147483647]; default=0 -> no limit.
            // -> We allow 256 buffers, each the size of the UDP max-message-size
            dfu_flow_control->max_bytes_per_period = 256 * 1470; // qos.transport().user_transports.front()->maxMessageSize;
            // -> Every 250ms
            dfu_flow_control->period_ms = 250;  // default=100
            // Further override with settings from device/dfu
            realdds::override_flow_controller_from_json( *dfu_flow_control, dds_settings.nested( "device", "dfu" ) );
            qos.flow_controllers().push_back( dfu_flow_control );

            // qos will get further overriden with the settings we pass in
            _participant->init( domain_id, qos, dds_settings.default_object() );

            // allow a certain number of seconds to wait for devices to appear (if set to 0, no waiting will occur)
            domain.query_devices_max
                = dds_settings.nested( std::string( "query-devices-max", 17 ), &json::is_number_integer )
                      .default_value( 5 );
            domain.query_devices_min
                = dds_settings.nested( std::string( "query-devices-min", 17 ), &json::is_number_integer )
                      .default_value( 3 );
        }
        else if( participant_name_j.exists() && participant_name != _participant->name() )
        {
            throw std::runtime_error( rsutils::string::from()
                                      << "A DDS participant '" << _participant->name() << "' already exists in domain "
                                      << domain_id << "; cannot create '" << participant_name << "'" );
        }
        size_t device_initialization_timeout = dds_settings.nested( "device-initialization-timeout-ms" ).default_value< size_t >( 5000 );
        bool partial_capabilities_allowed = ctx->get_settings().nested( "partial-device-allowed" ).default_value< bool >( false );
        _watcher_singleton = domain.device_watcher.instance( _participant, device_initialization_timeout, partial_capabilities_allowed );
        _subscription = _watcher_singleton->subscribe(
            [liveliness = std::weak_ptr< context >( ctx ),
             cb = std::move( cb )]( std::shared_ptr< realdds::dds_device > const & dev, bool added )
            {
                // the factory should be alive as long as the context is alive
                auto ctx = liveliness.lock();
                if( ! ctx )
                    return;
                std::vector< std::shared_ptr< device_info > > infos_added;
                std::vector< std::shared_ptr< device_info > > infos_removed;
                auto dev_info = std::make_shared< dds_device_info >( ctx, dev );
                if( added )
                {
                    infos_added.push_back( dev_info );
                    LOG_INFO( "Device connected: " << dev_info->get_address() );
                }
                else
                {
                    infos_removed.push_back( dev_info );
                    LOG_INFO( "Device disconnected: " << dev_info->get_address() );
                }
                cb( infos_removed, infos_added );
            } );
    }
}


rsdds_device_factory::~rsdds_device_factory() {}


std::vector< std::shared_ptr< device_info > > rsdds_device_factory::query_devices( unsigned requested_mask ) const
{
    std::vector< std::shared_ptr< device_info > > list;
    if( _watcher_singleton )
    {
        unsigned const mask = context::combine_device_masks( requested_mask, get_context()->get_device_mask() );

        auto participant = _watcher_singleton->get_device_watcher()->get_participant();
        domain_context * p_domain = nullptr;
        {
            std::lock_guard< std::mutex > lock( domain_context_by_id_mutex );
            auto it = domain_context_by_id.find( participant->domain_id() );
            if( it != domain_context_by_id.end() )
                p_domain = &it->second;
        }
        if( p_domain )
        {
            std::lock_guard< std::mutex > lock( p_domain->wait_mutex );
            if( p_domain->query_devices_max > 0 )
            {
                // Wait for devices the first time (per domain)
                // Do this with the mutex locked: if multiple threads all try to query_devices, the others will also
                // wait until we're done
                int timeout = p_domain->query_devices_max;
                p_domain->query_devices_max = 0;

                // We have to wait a minimum amount of time; this time really depends on several factors including
                // network variables:
                //      - the camera's announcement-period needs to fit into this time (e.g., if the camera announces
                //        every 10 seconds then we might miss its announcement if we only wait 5); our cameras currently
                //        have a period of 1.5 seconds
                if( timeout <= p_domain->query_devices_min )
                {
                    timeout = p_domain->query_devices_min;
                    LOG_DEBUG( "waiting " << timeout << " seconds for devices on domain " << participant->domain_id() << " ..." );
                }
                else
                {
                    LOG_DEBUG( "waiting " << p_domain->query_devices_min << '-' << timeout
                                          << " seconds for devices on domain " << participant->domain_id() << " ..." );
                }

                // New devices take a bit of time (less than 2 seconds, though) to initialize, but they'll be preceeded
                // with a new participant notification. So we set up a separate timer and short-circuit if the minimum
                // time has elapsed and we haven't seen any new participants in the last 2 seconds:
                auto listener = participant->create_listener();
                std::atomic< int > seconds_left( p_domain->query_devices_min );
                listener->on_participant_added( [&]( realdds::dds_guid, char const * )
                                                { seconds_left = std::max( seconds_left.load(), 2 ); } );

                while( true )
                {
                    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
                    if( --timeout <= 0 )
                        break;  // max time exceeded - can't wait any more
                    if( --seconds_left <= 0 )
                    {
                        LOG_DEBUG( "query_devices wait stopped with " << timeout << " seconds left" );
                        break;
                    }
                }
            }
        }

        _watcher_singleton->get_device_watcher()->foreach_device(
            [&]( std::shared_ptr< realdds::dds_device > const & dev ) -> bool
            {
                if( ! dev->is_ready() )
                {
                    LOG_DEBUG( "device '" << dev->device_info().debug_name() << "' is not ready" );
                    return true;
                }
                std::string product_line;
                if( dev->device_info().to_json().nested( "product-line" ).get_ex( product_line ) )
                {
                    if( product_line == "D400" )
                    {
                        if( ! ( mask & RS2_PRODUCT_LINE_D400 ) )
                            return true;
                    }
                    else if( product_line == "D500" )
                    {
                        if( ! ( mask & RS2_PRODUCT_LINE_D500 ) )
                            return true;
                    }
                    else if( ! ( mask & RS2_PRODUCT_LINE_NON_INTEL ) )
                    {
                        return true;
                    }
                }
                else if( ! ( mask & RS2_PRODUCT_LINE_NON_INTEL ) )
                {
                    return true;
                }

                if( auto ctx = get_context() )
                {
                    std::shared_ptr< device_info > info = std::make_shared< dds_device_info >( ctx, dev );
                    list.push_back( info );
                }
                return true;  // continue iteration
            } );
    }
    return list;
}


}  // namespace librealsense
