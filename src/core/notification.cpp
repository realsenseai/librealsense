// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#include "notification.h"
#include <rsutils/easylogging/easyloggingpp.h>


namespace librealsense {


notification::notification( rs2_notification_category category,
                            int type,
                            rs2_log_severity severity,
                            std::string const & description )
    : category( category )
    , type( type )
    , severity( severity )
    , description( description )
{
    timestamp
        = std::chrono::duration< double, std::milli >( std::chrono::system_clock::now().time_since_epoch() ).count();
    LOG_INFO( description );
}


notifications_processor::notifications_processor()
    : _dispatcher( 10, rsutils::concurrency::thread_category_dispatch, "notif-d" )
{
}


notifications_processor::~notifications_processor()
{
    _dispatcher.stop();
}


void notifications_processor::set_callback( rs2_notifications_callback_sptr callback )
{

    _dispatcher.stop();

    std::lock_guard< std::mutex > lock( _callback_mutex );
    _callback = std::move( callback );
    _dispatcher.start();
}


rs2_notifications_callback_sptr notifications_processor::get_callback() const
{
    std::lock_guard< std::mutex > lock( _callback_mutex );
    return _callback;
}


}  // namespace librealsense