// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include <rsutils/concurrency/concurrency.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/time/waiting-on.h>

dispatcher::dispatcher( unsigned int cap, std::string name, std::function< void( action ) > on_drop_callback )
    : _name( std::move( name ) )
    , _queue( cap, on_drop_callback )
    , _was_stopped( true )
    , _is_alive( true )
{
    LOG_DEBUG( "Dispatcher [" << _name << " @ " << this << "] created" );

    // We keep a running thread that takes stuff off our queue and dispatches them
    _thread = std::thread([&]()
    {
        int timeout_ms = 5000;
        while( _is_alive )
        {
            if( _wait_for_start( timeout_ms ) )
            {
                std::function< void(cancellable_timer) > item;
                if (_queue.dequeue(&item, timeout_ms))
                {
                    cancellable_timer time(this);

                    try
                    {
                        // While we're dispatching the item, we cannot stop!
                        std::lock_guard< std::mutex > lock(_dispatch_mutex);
                        item(time);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR( "Dispatcher [" << _name << " @ " << this << "] exception caught: " << e.what() );
                    }
                    catch (...)
                    {
                        LOG_ERROR( "Dispatcher [" << _name << " @ " << this << "] unknown exception caught!" );
                    }
                }
            }
        }
    });
}


dispatcher::~dispatcher()
{
    // The teardown path is deliberately unlogged: a dispatcher can be destroyed during static
    // destruction -- e.g. by an app holding an rs2::device in a global -- when the logger is
    // already gone, calling LOG_DEBUG will issue a segmentation fault.

    // Don't get into any more dispatches
    _is_alive = false;

    // Stop whatever's in-progress, if any
    stop();

    // Wait until our worker thread quits
    if( _thread.joinable() )
        _thread.join();
}


void dispatcher::start()
{
    bool was_stopped;
    {
        std::lock_guard< std::mutex > lock(_was_stopped_mutex);
        was_stopped = _was_stopped.exchange( false );
    }
    _queue.start();
    // Wake up all threads that wait for the dispatcher to start
    _was_stopped_cv.notify_all();

    // Only the actual transition, so repeated calls don't fill the log
    if( was_stopped )
        LOG_DEBUG( "Dispatcher [" << _name << " @ " << this << "] started" );
}


void dispatcher::stop()
{
    // With the following commented-out if, we have issues!
    // It seems stop is called multiple times and the queues are somehow waiting on something after
    // the first time. If we return, those queues aren't woken! If we continue, the only effect will
    // be to notify_all and we get good behavior...
    // 
    //if( _was_stopped )
    //    return;

    // First things first: don't accept any more incoming stuff, and get rid of anything
    // pending
    _queue.stop();

    // Wait until any dispatched is done...
    {
        std::lock_guard< std::mutex > lock(_dispatch_mutex);
        assert(_queue.empty());
    }
    // Signal we've stopped so any sleeping dispatched will wake up immediately
    {
        std::lock_guard< std::mutex > lock( _was_stopped_mutex );
        _was_stopped = true;
    }
    _was_stopped_cv.notify_all();

    // Stopping not logged: stop() is also reached from the destructor -- see the note there
}


// Return when all current items in the queue are finished (within a timeout).
// If additional items are added while we're waiting, those will not be waited on!
// Returns false if a timeout occurred before we were done
//
bool dispatcher::flush( std::chrono::steady_clock::duration timeout )
{
    if( _was_stopped )
        return true;  // Nothing to do - so success (no timeout)

    rsutils::time::waiting_on< bool > invoked( _was_stopped_cv, _was_stopped_mutex, false );
    // Blocking call, we don't want the item in the queue to drop if the queue is full.
    // TODO - Add a timeout to blocking invoke, Currently it can wait forever here.
    auto invoked_in_thread = invoked.in_thread();
    invoke( [invoked_in_thread]( cancellable_timer ) { invoked_in_thread.signal( true ); }, true );
    invoked.wait_until( timeout, [&]() { return invoked || _was_stopped; } );
    return invoked;
}

// Return true if dispatcher is started (within a timeout).
// false if not or the dispatcher is no longer alive
//
bool dispatcher::_wait_for_start( int timeout_ms )
{
    // If the dispatcher is not started wait for a start event, if not such event within given timeout do nothing.
    // If during the wait the thread destructor is called (_is_alive = false) do nothing as well.
    std::unique_lock< std::mutex > lock(_was_stopped_mutex);
    return _was_stopped_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
        return !_was_stopped.load() || !_is_alive;
        } ) && _is_alive;
}

