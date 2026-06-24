// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023-2024 RealSense, Inc. All Rights Reserved.

#include "backend-device-factory.h"
#include "context.h"
#include "backend.h"
#include "platform/platform-device-info.h"
#include "platform/device-watcher.h"

#include "backend-device.h"
#include "ds/d400/d400-info.h"
#include "ds/d500/d500-info.h"
#include "fw-update/fw-update-factory.h"
#include "platform-camera.h"

#include <librealsense2/h/rs_context.h>

#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/shared-ptr-singleton.h>
#include <rsutils/signal.h>
#include <rsutils/json.h>


namespace librealsense {
namespace platform {
std::shared_ptr< backend > create_backend();
}  // namespace platform
}  // namespace librealsense


namespace librealsense {


std::vector< std::shared_ptr< platform::platform_device_info > >
subtract_sets( const std::vector< std::shared_ptr< platform::platform_device_info > > & first,
               const std::vector< std::shared_ptr< platform::platform_device_info > > & second )
{
    std::vector< std::shared_ptr< platform::platform_device_info > > results;
    std::for_each(
        first.begin(),
        first.end(),
        [&]( std::shared_ptr< platform::platform_device_info > const & data )
        {
            if( std::find_if(
                    second.begin(),
                    second.end(),
                    [&]( std::shared_ptr< platform::platform_device_info > const & new_dev )
                    {
                        return data->get_group().is_contained_in( new_dev->get_group() );
                    } )
                == second.end() )
            {
                results.push_back( data );
            }
        } );
    return results;
}


// This singleton creates the actual backend; as long as someone holds it, the backend will stay alive.
// The instance is held below by the device-watcher. I.e., the device-watcher triggers creation of the
// backend!
//
class backend_singleton
{
    std::shared_ptr< platform::backend > const _backend;

public:
    backend_singleton()
        : _backend( platform::create_backend() )
    {
    }

    std::shared_ptr< platform::backend > get() const { return _backend; }
};


static rsutils::shared_ptr_singleton< backend_singleton > the_backend;


// The device-watcher is also a singleton: we don't need multiple agents of notifications. It is held alive by the
// device-factory below, which is held per context. I.e., as long as the context is alive, we'll stay alive and the
// backend-singleton will stay alive.
// 
// We are responsible for exposing the single notification from the platform-device-watcher to several subscribers:
// one device-watcher, but many contexts, each with further subscriptions.
//
class device_watcher_singleton
{
    // The device-watcher keeps a direct pointer to the backend instance, so we have to make sure it stays alive!
    std::shared_ptr< backend_singleton > const _backend;
    std::shared_ptr< platform::device_watcher > const _device_watcher;
    rsutils::signal< platform::backend_device_group const &, platform::backend_device_group const & > _callbacks;

public:
    device_watcher_singleton()
        : _backend( the_backend.instance() )
        , _device_watcher( _backend->get()->create_device_watcher() )
    {
        assert( _device_watcher->is_stopped() );
        _device_watcher->start(
            [this]( platform::backend_device_group const & old, platform::backend_device_group const & curr )
            { _callbacks.raise( old, curr ); } );
    }

    ~device_watcher_singleton()
    {
        _device_watcher->stop();
    }

    rsutils::subscription subscribe( platform::device_changed_callback && cb )
    {
        return _callbacks.subscribe( std::move( cb ) );
    }

    std::shared_ptr< platform::backend > const get_backend() const { return _backend->get(); }
};


static rsutils::shared_ptr_singleton< device_watcher_singleton > backend_device_watcher;


std::shared_ptr< platform::backend > backend_device::get_backend()
{
    auto singleton = the_backend.get();
    if( ! singleton )
        // Whoever is calling us, they are expecting a backend to exist, but it does not!
        throw std::runtime_error( "backend not created yet!" );

    return singleton->get();
}


backend_device_factory::backend_device_factory( std::shared_ptr< context > const & ctx, callback && cb )
    : super( ctx )
    , _device_watcher( backend_device_watcher.instance() )
    , _dtor( _device_watcher->subscribe(
          [this, liveliness = std::weak_ptr< context >( ctx ), cb = std::move( cb )](
              platform::backend_device_group const & old, platform::backend_device_group const & curr )
          {
              // the factory should be alive as long as the context is alive
              auto live_ctx = liveliness.lock();
              if( ! live_ctx )
                  return;

              auto old_list = create_devices_from_group( old, RS2_PRODUCT_LINE_ANY );
              auto new_list = create_devices_from_group( curr, RS2_PRODUCT_LINE_ANY );

              std::vector< std::shared_ptr< device_info > > devices_removed;
              for( auto & device_removed : subtract_sets( old_list, new_list ) )
              {
                  devices_removed.push_back( device_removed );
                  LOG_INFO( "Device disconnected: " << device_removed->get_address() );
              }

              std::vector< std::shared_ptr< device_info > > devices_added;
              for( auto & device_added : subtract_sets( new_list, old_list ) )
              {
                  devices_added.push_back( device_added );
                  LOG_INFO( "Device connected: " << device_added->get_address() );
              }

              if( devices_removed.size() + devices_added.size() )
              {
                  cb( devices_removed, devices_added );
              }
          } ) )
{
}


backend_device_factory::~backend_device_factory()
{
}


std::vector< std::shared_ptr< device_info > > backend_device_factory::query_devices( unsigned requested_mask ) const
{
    auto ctx = get_context();
    if( ! ctx )
        return {};

    if( ( requested_mask & RS2_PRODUCT_LINE_SW_ONLY ) || ( ctx->get_device_mask() & RS2_PRODUCT_LINE_SW_ONLY ) )
        return {};  // We don't carry any software devices

    auto backend = _device_watcher->get_backend();
    platform::backend_device_group group( backend->query_uvc_devices(),
                                          backend->query_usb_devices(),
                                          backend->query_mipi_devices(),
                                          backend->query_hid_devices() );
    auto devices = create_devices_from_group( group, requested_mask );
    return { devices.begin(), devices.end() };
}


std::vector< std::shared_ptr< platform::platform_device_info > >
backend_device_factory::create_devices_from_group( platform::backend_device_group devices, int requested_mask ) const
{
    auto ctx = get_context();
    std::vector< std::shared_ptr< platform::platform_device_info > > list;
    unsigned const mask = context::combine_device_masks( requested_mask, ctx->get_device_mask() );
    if( ! ( mask & RS2_PRODUCT_LINE_SW_ONLY ) )
    {
        if( mask & RS2_PRODUCT_LINE_D400 )
        {
            auto d400_devices = d400_info::pick_d400_devices( ctx, devices );
            std::copy( std::begin( d400_devices ), end( d400_devices ), std::back_inserter( list ) );
        }

        if( mask & RS2_PRODUCT_LINE_D500 )
        {
            auto d500_devices = d500_info::pick_d500_devices( ctx, devices );
            std::copy( begin( d500_devices ), end( d500_devices ), std::back_inserter( list ) );
        }

        // Supported recovery devices
        {
            auto recovery_devices
                = fw_update_info::pick_recovery_devices( ctx, devices.usb_devices, mask );
            std::copy( begin( recovery_devices ), end( recovery_devices ), std::back_inserter( list ) );
        }

        // Supported mipi recovery devices
        {
            auto recovery_devices
                = fw_update_info::pick_recovery_devices( ctx, devices.mipi_devices, mask );
            std::copy( begin( recovery_devices ), end( recovery_devices ), std::back_inserter( list ) );
        }

        if( mask & RS2_PRODUCT_LINE_NON_INTEL )
        {
            auto uvc_devices
                = platform_camera_info::pick_uvc_devices( ctx, devices.uvc_devices );
            std::copy( begin( uvc_devices ), end( uvc_devices ), std::back_inserter( list ) );
        }
    }
    return list;
}


}  // namespace librealsense
