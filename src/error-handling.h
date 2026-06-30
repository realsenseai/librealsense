/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2019 RealSense, Inc. All Rights Reserved. */
#pragma once

#include "core/option-interface.h"
#include <rsutils/concurrency/concurrency.h>

#include <atomic>
#include <memory>


namespace librealsense
{
    class notification_decoder;
    class notifications_processor;


    class polling_error_handler
    {
    public:
        polling_error_handler(unsigned int poll_intervals_ms, std::shared_ptr<option> option,
            std::weak_ptr<std::atomic<bool>> device_alive,
            std::shared_ptr<notifications_processor> processor, std::shared_ptr<notification_decoder> decoder);
        ~polling_error_handler();

        polling_error_handler(const polling_error_handler &) = delete;
        polling_error_handler & operator=(const polling_error_handler &) = delete;

        unsigned int get_polling_interval() const { return _poll_intervals_ms; }

        void start( unsigned int poll_intervals_ms = 0 );
        void stop();

    private:
        void polling(dispatcher::cancellable_timer cancellable_timer);

        unsigned int _poll_intervals_ms;
        bool _silenced = false;
        std::shared_ptr<option> _option;
        std::weak_ptr<std::atomic<bool>> _device_alive;
        std::shared_ptr < active_object<> > _active_object;
        std::weak_ptr<notifications_processor> _notifications_processor;
        std::shared_ptr<notification_decoder> _decoder;
    };


    class polling_errors_disable : public option
    {
    public:
        polling_errors_disable( std::shared_ptr< polling_error_handler > handler )
            : _polling_error_handler( handler )
            , _value( 1 )
        {
        }

        ~polling_errors_disable();

        void set( float value ) override;

        float query() const override;

        option_range get_range() const override;

        bool is_enabled() const override;


        const char * get_description() const override;

        const char * get_value_description( float value ) const override;
        void enable_recording( std::function< void( const option & ) > record_action ) override
        {
            _recording_function = record_action;
        }

    private:
        std::weak_ptr< polling_error_handler > _polling_error_handler;
        float _value;
        std::function< void( const option & ) > _recording_function = []( const option & ) {};
    };

}
