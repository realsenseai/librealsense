// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include <string>
#include <thread>


namespace rsutils {
namespace concurrency {


// Thread categories for classifying the purpose of threads created by the library.
// Values must match rs2_thread_category in the public C API.
enum thread_category
{
    thread_category_usb_io            = 0,
    thread_category_video_capture     = 1,
    thread_category_sensor_io         = 2,
    thread_category_frame_processing  = 3,
    thread_category_device_monitoring = 4,
    thread_category_dispatch          = 5,
    thread_category_network           = 6,
    thread_category_utility           = 7,
};


// Type for the user-provided thread-start callback.
// Called from within the new thread before any work begins.
// Parameters: category (thread_category enum), name (thread name string).
using thread_start_callback_fn = std::function< void( thread_category category, std::string const & name ) >;


// Register a global callback that is invoked at the start of every library-created thread.
// The callback runs inside the new thread, so the user can call platform-specific APIs
// (e.g., pthread_setpriority, SetThreadPriority) to adjust thread priority.
// Pass an empty function (or nullptr-equivalent) to clear the callback.
void set_thread_start_callback( thread_start_callback_fn callback );


// Set the current thread's OS-level name. The name is prefixed with "rs:" automatically.
// On Linux, the name is truncated to 15 characters (pthread_setname_np limit).
// On Windows, SetThreadDescription is used (no length limit).
// On macOS, pthread_setname_np is used (no length limit).
void set_current_thread_name( std::string const & name );


// Create a new std::thread with proper naming and callback invocation.
// The thread body will:
//   1. Set the OS thread name to "rs:<name>" (truncated to 15 chars on Linux)
//   2. Invoke the registered thread-start callback (if any)
//   3. Run the user-provided function
std::thread create_thread( thread_category category, std::string const & name, std::function< void() > fn );


}  // namespace concurrency
}  // namespace rsutils
