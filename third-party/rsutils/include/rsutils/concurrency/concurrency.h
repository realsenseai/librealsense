// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <cassert>
#include <string>
#include <rsutils/concurrency/thread-utils.h>

const int QUEUE_MAX_SIZE = 10;
// Simplest implementation of a blocking concurrent queue for thread messaging
template<class T>
class single_consumer_queue
{
    std::deque<T> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _deq_cv; // not empty signal
    std::condition_variable _enq_cv; // not full signal

    unsigned int const _cap;
    bool _accepting;

    std::function<void(T const &)> const _on_drop_callback;

public:
    explicit single_consumer_queue< T >( unsigned int cap = QUEUE_MAX_SIZE,
                                         std::function< void( T const & ) > on_drop_callback = nullptr )
        : _cap( cap )
        , _accepting( true )
        , _on_drop_callback( on_drop_callback )
    {
    }

    // Enqueue an item onto the queue.
    // If the queue grows beyond capacity, the front will be removed, losing whatever was there!
    bool enqueue(T&& item)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if( ! _accepting )
        {
            if( _on_drop_callback )
                _on_drop_callback( item );
            return false;
        }

        _queue.push_back(std::move(item));

        if( _queue.size() > _cap )
        {
            if( _on_drop_callback )
                _on_drop_callback( _queue.front() );
            _queue.pop_front();
        }

        lock.unlock();

        // We pushed something -- let others know there's something to dequeue
        _deq_cv.notify_one();

        return true;
    }


    // Enqueue an item, but wait for room if there isn't any
    // Returns true if the enqueue succeeded
    bool blocking_enqueue(T&& item)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _enq_cv.wait( lock, [this]() {
            return _queue.size() < _cap; } );
        if( ! _accepting )
        {
            // We shouldn't be adding anything to the queue when we're stopping
            if( _on_drop_callback )
                _on_drop_callback( item );
            return false;
        }

        _queue.push_back(std::move(item));
        lock.unlock();

        // We pushed something -- let another know there's something to dequeue
        _deq_cv.notify_one();

        return true;
    }


    // Remove one item; if unavailable, wait for it
    // Return true if an item was removed -- otherwise, false
    bool dequeue( T * item, unsigned int timeout_ms )
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if( ! _deq_cv.wait_for( lock,
                                std::chrono::milliseconds( timeout_ms ),
                                [this]() { return ! _accepting || ! _queue.empty(); } )
            || _queue.empty() )
        {
            return false;
        }

        *item = std::move(_queue.front());
        _queue.pop_front();

        // We've made room -- let whoever is waiting for room know about it
        _enq_cv.notify_one();

        return true;
    }

    // Remove one item if available; do not wait for one
    // Return true if an item was removed -- otherwise, false
    bool try_dequeue(T* item)
    {
        std::lock_guard< std::mutex > lock( _mutex );
        if( _queue.empty() )
            return false;

        *item = std::move(_queue.front());
        _queue.pop_front();

        // We've made room -- let whoever is waiting for room know about it
        _enq_cv.notify_one();

        return true;
    }

    template< class Fn >
    bool peek( Fn fn ) const
    {
        std::lock_guard< std::mutex > lock( _mutex );
        if( _queue.empty() )
            return false;
        fn( _queue.front() );
        return true;
    }

    template< class Fn >
    bool peek( Fn fn )
    {
        std::lock_guard< std::mutex > lock( _mutex );
        if( _queue.empty() )
            return false;
        fn( _queue.front() );
        return true;
    }

    void stop()
    {
        std::lock_guard< std::mutex > lock( _mutex );

        // We no longer accept any more items!
        _accepting = false;

        _clear();
    }

    void clear()
    {
        std::lock_guard< std::mutex > lock( _mutex );
        _clear();
    }

protected:
    void _clear()
    {
        _queue.clear();

        // Wake up anyone who is waiting for room to enqueue, or waiting for something to dequeue -- there's nothing now
        _enq_cv.notify_all();
        _deq_cv.notify_all();
    }

public:
    void start()
    {
        std::lock_guard< std::mutex > lock( _mutex );
        _accepting = true;
    }

    bool started() const { return _accepting; }
    bool stopped() const { return ! started(); }

    size_t size() const
    {
        std::lock_guard< std::mutex > lock( _mutex );
        return _queue.size();
    }

    bool empty() const { return ! size(); }
};

// A single_consumer_queue meant to hold frame_holder objects
template<class T>
class single_consumer_frame_queue
{
    single_consumer_queue<T> _queue;

public:
    single_consumer_frame_queue< T >( unsigned int cap = QUEUE_MAX_SIZE,
                                      std::function< void( T const & ) > on_drop_callback = nullptr )
        : _queue( cap, on_drop_callback )
    {
    }

    bool enqueue( T && item )
    {
        if( item->is_blocking() )
            return _queue.blocking_enqueue( std::move( item ) );
        else
            return _queue.enqueue( std::move( item ) );
    }

    bool dequeue(T* item, unsigned int timeout_ms)
    {
        return _queue.dequeue(item, timeout_ms);
    }

    bool try_dequeue(T* item)
    {
        return _queue.try_dequeue(item);
    }

    template< class Fn >
    bool peek( Fn fn ) const
    {
        return _queue.peek( fn );
    }

    template< class Fn >
    bool peek( Fn fn )
    {
        return _queue.peek( fn );
    }

    void clear()
    {
        _queue.clear();
    }

    void stop()
    {
        _queue.stop();
    }

    void start()
    {
        _queue.start();
    }

    size_t size() const
    {
        return _queue.size();
    }

    bool empty() const
    {
        return _queue.empty();
    }

    bool started() const { return _queue.started(); }
    bool stopped() const { return _queue.stopped(); }
};

// The dispatcher is responsible for dispatching generic 'actions': any thread can queue an action
// (lambda) for dispatch, while the dispatcher maintains a single thread that runs these actions one
// at a time.
//
class dispatcher
{
public:
    // An action, when run, takes a 'cancellable_timer', which it may ignore. This class allows the
    // action to perform some sleep and know that, if the dispatcher is shutting down, its sleep
    // will still be interrupted.
    //
    class cancellable_timer
    {
        dispatcher* _owner;
    
    public:
        cancellable_timer(dispatcher* owner)
            : _owner(owner)
        {}

        bool was_stopped() const { return _owner->_was_stopped.load(); }

        // Replacement for sleep() -- try to sleep for a time, but stop if the
        // dispatcher is stopped
        // 
        // Return false if the dispatcher was stopped, true otherwise
        //
        template< class Duration >
        bool try_sleep( Duration sleep_time )
        {
            using namespace std::chrono;

            std::unique_lock<std::mutex> lock(_owner->_was_stopped_mutex);
            if( was_stopped() )
                return false;
            // wait_for() returns "false if the predicate pred still evaluates to false after the
            // rel_time timeout expired, otherwise true"
            return ! (
                _owner->_was_stopped_cv.wait_for( lock, sleep_time, [&]() { return was_stopped(); } ) );
        }
    };

    // An action is any functor that accepts a cancellable timer
    typedef std::function<void(cancellable_timer const &)> action;

    // Certain conditions (see invoke()) may cause actions to be lost, e.g. when the queue gets full
    // and we're non-blocking. The on_drop_callback allows caputring of these instances, if we
    // want...
    //
    // The thread_category and thread_name parameters are used for OS-level thread naming and
    // the user-registerable thread-start callback. The thread name will be prefixed with "rs:".
    //
    dispatcher( unsigned int queue_capacity,
                rsutils::concurrency::thread_category thread_category,
                std::string const & thread_name,
                std::function< void( action ) > on_drop_callback = nullptr );

    ~dispatcher();

    bool empty() const { return _queue.empty(); }

    std::string const & get_thread_name() const { return _thread_name; }
    rsutils::concurrency::thread_category get_thread_category() const { return _thread_category; }

    // Main invocation of an action: this will be called from any thread, and basically just queues
    // up the actions for our dispatching thread to handle them.
    // 
    // A blocking invocation means that it will wait until there's room in the queue: if not
    // blocking and the queue is full, the action will be queued at the expense of the next action
    // in line (the oldest) for dispatch!
    //
    template<class T>
    void invoke(T item, bool is_blocking = false)
    {
        if (!_was_stopped)
        {
            if(is_blocking)
                _queue.blocking_enqueue(std::move(item));
            else
                _queue.enqueue(std::move(item));
        }
    }

    // Like above, but synchronous: will return only when the action has actually been dispatched.
    //
    template<class T>
    void invoke_and_wait(T item, std::function<bool()> exit_condition, bool is_blocking = false)
    {
        bool done = false;

        //action
        auto func = std::move(item);
        invoke([&, func](dispatcher::cancellable_timer c)
        {
            std::lock_guard<std::mutex> lk(_blocking_invoke_mutex);
            func(c);

            done = true;
            _blocking_invoke_cv.notify_one();
        }, is_blocking);

        //wait
        std::unique_lock<std::mutex> lk(_blocking_invoke_mutex);
        _blocking_invoke_cv.wait(lk, [&](){ return done || exit_condition(); });
    }

    // Stops the dispatcher. This is not a pause: it will clear out the queue, losing any pending
    // actions!
    //
    void stop();

    // A dispatcher starts out 'started' after construction. It can then be stopped and started
    // again if needed.
    //
    void start();

    // Return when all items in the queue are finished (within a timeout).
    // If additional items are added while we're waiting, those will not be waited on!
    //
    bool flush(std::chrono::steady_clock::duration timeout = std::chrono::seconds(10) );


private:
    // Return true if dispatcher is started (within a timeout).
    // false if not or the dispatcher is no longer alive
    //
    bool _wait_for_start( int timeout_ms );

    friend cancellable_timer;

    single_consumer_queue<std::function<void(cancellable_timer)>> _queue;
    std::thread _thread;

    std::atomic<bool> _was_stopped;
    std::condition_variable _was_stopped_cv;
    std::mutex _was_stopped_mutex;

    std::mutex _dispatch_mutex;

    std::condition_variable _blocking_invoke_cv;
    std::mutex _blocking_invoke_mutex;

    std::atomic<bool> _is_alive;

    rsutils::concurrency::thread_category _thread_category;
    std::string _thread_name;
};

template<class T = std::function<void(dispatcher::cancellable_timer)>>
class active_object
{
public:
    active_object(T operation, rsutils::concurrency::thread_category thread_category, std::string const & thread_name)
        : _operation(std::move(operation)), _dispatcher(1, thread_category, thread_name), _stopped(true)
    {
    }

    void start()
    {
        _stopped = false;
        _dispatcher.start();

        do_loop();
    }

    void stop()
    {
        if (!_stopped.load()) {
            _stopped = true;
            _dispatcher.stop();
        }
    }

    ~active_object()
    {
        stop();
    }

    bool is_active() const
    {
        return !_stopped;
    }
private:
    void do_loop()
    {
        _dispatcher.invoke([this](dispatcher::cancellable_timer ct)
        {
            _operation(ct);
            if (!_stopped)
            {
                do_loop();
            }
        });
    }

    T _operation;
    dispatcher _dispatcher;
    std::atomic<bool> _stopped;
};

class watchdog
{
public:
    watchdog(std::function<void()> operation, uint64_t timeout_ms,
             rsutils::concurrency::thread_category thread_category, std::string const & thread_name) :
            _timeout_ms(timeout_ms), _operation(std::move(operation))
    {
        _watcher = std::make_shared<active_object<>>([this](dispatcher::cancellable_timer cancellable_timer)
        {
            if(cancellable_timer.try_sleep( std::chrono::milliseconds( _timeout_ms )))
            {
                if(!_kicked)
                    _operation();
                std::lock_guard<std::mutex> lk(_m);
                _kicked = false;
            }
        }, thread_category, thread_name);
    }

    ~watchdog()
    {
        if(_running)
            stop();
    }

    void start() { std::lock_guard<std::mutex> lk(_m); _watcher->start(); _running = true; }
    void stop() { { std::lock_guard<std::mutex> lk(_m); _running = false; } _watcher->stop(); }
    bool running() { std::lock_guard<std::mutex> lk(_m); return _running; }
    void set_timeout(uint64_t timeout_ms) { std::lock_guard<std::mutex> lk(_m); _timeout_ms = timeout_ms; }
    void kick() { std::lock_guard<std::mutex> lk(_m); _kicked = true; }

private:
    std::mutex _m;
    uint64_t _timeout_ms;
    bool _kicked = false;
    bool _running = false;
    std::function<void()> _operation;
    std::shared_ptr<active_object<>> _watcher;
};
