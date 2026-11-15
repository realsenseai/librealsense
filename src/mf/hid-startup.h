// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace librealsense {
namespace platform {

// Control-thread startup waits for callback-thread reports. Report and start
// timestamps share the Windows FILETIME clock, so cached reports cannot admit
// a new capture. No upstream callbacks are allowed until the wait succeeds.
class hid_startup
{
public:
    // Check the D585 2C/3C USB products (including prototypes) whose
    // shared-consumer refusal path is handled here. Other Windows HID devices
    // keep asynchronous start; new PIDs are not automatically opted in.
    static bool is_required( const std::string & vid, const std::string & pid )
    {
        return vid == "38e5" && ( pid == "c04" || pid == "c05" || pid == "c07" || pid == "c08" );
    }

    hid_startup( const std::vector<std::string> & names, uint64_t started_at )
        : _pending( names.begin(), names.end() ), _started_at( started_at ) {}

    bool on_report( const std::string & name, uint64_t timestamp )
    {
        if( _ready.load( std::memory_order_acquire ) )
            return true;
        std::lock_guard<std::mutex> lock( _mutex );
        if( timestamp >= _started_at )
        {
            _pending.erase( name );
            _cv.notify_one();
        }
        return false;
    }

    void wait_for( std::chrono::milliseconds timeout )
    {
        std::unique_lock<std::mutex> lock( _mutex );
        if( ! _cv.wait_for( lock, timeout, [this] { return _pending.empty(); } ) )
        {
            std::string missing;
            for( auto const & name : _pending )
                missing += " " + name;
            throw std::runtime_error( "Windows HID start timed out waiting for fresh data from:" + missing );
        }
        _ready.store( true, std::memory_order_release );
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    std::set<std::string> _pending;
    const uint64_t _started_at;
    std::atomic<bool> _ready{false};
};

}  // namespace platform
}  // namespace librealsense
