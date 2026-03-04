// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "error-handling.h"
#include "core/notification.h"
#include "librealsense-exception.h"

#include <rsutils/string/from.h>

#include <memory>


namespace librealsense
{
    polling_error_handler::polling_error_handler(unsigned int poll_intervals_ms, std::shared_ptr<option> option,
        std::shared_ptr <notifications_processor> processor, std::shared_ptr<notification_decoder> decoder)
        :_poll_intervals_ms(poll_intervals_ms),
        _option(option),
        _notifications_processor(processor),
        _decoder(decoder)
    {
        _active_object = std::make_shared<active_object<>>([this](dispatcher::cancellable_timer cancellable_timer)
            {  polling(cancellable_timer);  },
            rsutils::concurrency::thread_category_device_monitoring, "err-poll");
    }

    polling_error_handler::~polling_error_handler()
    {
        stop();
    }

    polling_error_handler::polling_error_handler(const polling_error_handler& h)
    {
        _poll_intervals_ms = h._poll_intervals_ms;
        _active_object = h._active_object;
        _option = h._option;
        _notifications_processor = h._notifications_processor;
        _decoder = h._decoder;
    }

    void polling_error_handler::start( unsigned int poll_intervals_ms )
    {
        if( poll_intervals_ms )
            _poll_intervals_ms = poll_intervals_ms;
        _active_object->start();
    }
    void polling_error_handler::stop()
    {
        _active_object->stop();
    }

    void polling_error_handler::polling( dispatcher::cancellable_timer cancellable_timer )
    {
        if( cancellable_timer.try_sleep( std::chrono::milliseconds( _poll_intervals_ms ) ) )
        {
            if( ! _silenced )
            {
                try
                {
                    auto val = static_cast< uint8_t >( _option->query() );

                    if( val != 0 )
                    {
                        LOG_DEBUG( "Error detected from FW, error ID: " <<  std::to_string(val)  );
                        // First reset the value in the FW.
                        auto reseted_val = static_cast< uint8_t >( _option->query() );
                        auto strong = _notifications_processor.lock();
                        if( ! strong )
                        {
                            LOG_DEBUG( "Could not lock the notifications processor" );
                            _silenced = true;
                            return;
                        }

                        strong->raise_notification( _decoder->decode( val ) );

                        // Reading from last-error control is supposed to set it to zero in the
                        // firmware If this is not happening there is some issue
                        // Note: if an error will be raised between the 2 queries, this will cause
                        // the error polling loop to stop
                        if( reseted_val != 0 )
                        {
                            std::string error_str = rsutils::string::from()
                                                 << "Error polling loop is not behaving as expected! "
                                                    "expecting value : 0 got : "
                                                 << std::to_string( val ) << "\nShutting down error polling loop";
                            LOG_ERROR( error_str );
                            notification postcondition_failed{
                                RS2_NOTIFICATION_CATEGORY_HARDWARE_ERROR,
                                0,
                                RS2_LOG_SEVERITY_WARN,
                                error_str };
                            strong->raise_notification( postcondition_failed );
                            _silenced = true;
                        }
                    }
                }
                catch( const std::exception & ex )
                {
                    LOG_ERROR( "Error during polling error handler: " << ex.what() );
                }
                catch( ... )
                {
                    LOG_ERROR( "Unknown error during polling error handler!" );
                }
            }
        }
        else
        {
            LOG_DEBUG( "Notification polling loop is being shut-down" );
        }
    }

    polling_errors_disable::~polling_errors_disable()
    {
        if( auto handler = _polling_error_handler.lock() )
            handler->stop();
    }

    void polling_errors_disable::set( float value )
    {
        if( value < 0 )
            throw invalid_value_exception( "invalid polling errors value " + std::to_string( value ) );

        if( auto handler = _polling_error_handler.lock() )
        {
            _value = value;
            if( value <= std::numeric_limits< float >::epsilon() )
                handler->stop();
            else
                handler->start( (unsigned int) (value * 1000.f) );
        }
        _recording_function( *this );
    }

    float polling_errors_disable::query() const
    {
        return _value;
    }

    option_range polling_errors_disable::get_range() const
    {
        return option_range{ 0, 1, 1, 0 };
    }

    bool polling_errors_disable::is_enabled() const
    {
        return true;
    }

    const char * polling_errors_disable::get_description() const
    {
        return "Enable / disable polling of camera internal errors";
    }

    const char * polling_errors_disable::get_value_description( float value ) const
    {
        if( value == 0 )
        {
            return "Disabled";
        }
        else
        {
            return "Enabled";
        }
    }

}  // namespace librealsense
