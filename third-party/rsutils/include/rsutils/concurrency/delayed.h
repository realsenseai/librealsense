// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/concurrency/event.h>
#include <rsutils/concurrency/thread-utils.h>
#include <rsutils/time/timer.h>
#include <functional>
#include <thread>


namespace rsutils {
namespace concurrency {


// Runs a callback function "later", as in:
//     delayed on_event( [] { std::cout << "on_event" << std::endl; } );
//     context.on_event_that_happens_many_times( [&] {
//         on_event.call_in( std::chrono::seconds( 1 ) );
//         } );
//     ...
// Once call_in() returns, we can keep on doing other stuff. A timer will be started and only when it expires will the
// function be called.
// 
// This is a variation of 'rsutils::deferred', but requiring threading and a timer.
// 
// If another call_in() is used before the time expires and the callback called, the timer will be reset: the callback
// will run only once. If the callback is run and then call_in() occurs again, the callback will be run again. This
// class can therefore be used as a "delayed reaction" when you expect a trigger to happen multiple times and you want
// to respond only once.
// 
// If the object is destroyed, nothing will happen even if a timer is active. The object must stay alive.
//
class delayed
{
public:
    using callback = std::function< void() >;

private:
    callback _fn;
    rsutils::concurrency::event _done;
    rsutils::time::timer _timer;
    std::thread _th;

    using duration = rsutils::time::clock::duration;


public:
    delayed( callback && fn )
        : _done( true )
        , _timer( duration( 0 ) )
        , _fn( std::move( fn ) )
    {
    }
    ~delayed()
    {
        if( _th.joinable() )
        {
            _done.set();
            _th.join();
        }
    }

    void call_in( duration timeout )
    {
        _timer.reset( timeout );
        if( _done.clear() )  // was set, i.e. no thread
        {
            if( _th.joinable() )
                _th.join();
            _th = rsutils::concurrency::create_thread(
                rsutils::concurrency::thread_category_utility, "delayed",
                [this]
                {
                    while( ! _done.wait( _timer.time_left() ) )  // i.e., timeout occurred
                    {
                        // Timer could have been changed while we waited
                        if( _timer.has_expired() )
                        {
                            _fn();
                            _done.set();  // Thread is about to exit
                            break;
                        }
                    }
                } );
        }
    }

    void call_now() { _fn(); }
};


}  // namespace concurrency
}  // namespace rsutils
