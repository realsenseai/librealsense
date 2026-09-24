// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-named-mutex.h"

#include "types.h"
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#ifdef ANDROID

// https://android.googlesource.com/platform/bionic/+/master/libc/include/bits/lockf.h
#define F_ULOCK 0
#define F_LOCK 1
#define F_TLOCK 2
#define F_TEST 3

// https://android.googlesource.com/platform/bionic/+/master/libc/bionic/lockf.cpp
int lockf64(int fd, int cmd, off64_t length)
{
    // Translate POSIX lockf into fcntl.
    struct flock64 fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_whence = SEEK_CUR;
    fl.l_start = 0;
    fl.l_len = length;

    if (cmd == F_ULOCK) {
        fl.l_type = F_UNLCK;
        cmd = F_SETLK64;
        return fcntl(fd, F_SETLK64, &fl);
    }

    if (cmd == F_LOCK) {
        fl.l_type = F_WRLCK;
        return fcntl(fd, F_SETLKW64, &fl);
    }

    if (cmd == F_TLOCK) {
        fl.l_type = F_WRLCK;
        return fcntl(fd, F_SETLK64, &fl);
    }
    if (cmd == F_TEST) {
        fl.l_type = F_RDLCK;
        if (fcntl(fd, F_GETLK64, &fl) == -1) return -1;
        if (fl.l_type == F_UNLCK || fl.l_pid == getpid()) return 0;
        errno = EACCES;
        return -1;
    }

    errno = EINVAL;
    return -1;
}

int lockf(int fd, int cmd, off_t length)
{
    return lockf64(fd, cmd, length);
}
#endif

namespace librealsense
{
    namespace platform
    {
        named_mutex::named_mutex(const std::string& device_path, unsigned timeout)
            : _device_path(device_path),
              _timeout(timeout), // TODO: try to lock with timeout
              _fildes(-1),
              _lock_counter(0)
        {
            // File descriptor will be opened lazily when lock() is called
            // This prevents holding open file descriptors across hardware resets
        }

        named_mutex::~named_mutex()
        {
            try
            {
                // OFD lock will automatically release if locked only when file description closes (not file descriptor)
                // If process was forked or cloned and unlock() was not properly called after a lock() then file will
                // remain locked for the child process
                close_fd();
            }
            catch(...)
            {
                LOG_DEBUG( "Error while unlocking mutex" );
            }
        }

        void named_mutex::ensure_fd_open()
        {
            if (_fildes < 0)
            {
                _fildes = open(_device_path.c_str(), O_RDWR, 0);
                if (_fildes < 0)
                {
                    throw linux_backend_exception( rsutils::string::from() << "named_mutex: Cannot open '" << _device_path << "'" );
                }
            }
        }

        void named_mutex::close_fd()
        {
            // Close file descriptor if it was opened
            if (_fildes >= 0)
            {
                close( _fildes );
                _fildes = -1;
            }
        }

        void named_mutex::lock()
        {
            if( _lock_counter.fetch_add( 1 ) == 0 )
            {
                try
                {
                    // Open file descriptor only when first lock is acquired
                    ensure_fd_open();

                    // Using OFD locks to synchronize both interprocess and interthread.
                    // flock() is not good if we have multiple instances of the same device. i.e.
                    //     auto devs = context.query_devices();
                    //     auto dev1 = devs[0];
                    //     auto dev2 = devs[0];
                    // Closing file descriptor for one instance will unlock file for other instances.
                    // Note - OFD locks are linux standart not POSIX.
                    struct flock fl;
                    memset( &fl, 0, sizeof( fl ) );
                    fl.l_whence = SEEK_SET;
                    fl.l_start = 0;
                    fl.l_len = 0; // Lock the entire file
                    fl.l_type = F_WRLCK;
                    auto ret = fcntl( _fildes, F_OFD_SETLKW, &fl );
                    if( 0 != ret )
                    {
                        // Close file descriptor since we failed to acquire lock
                        close_fd();
                        
                        throw linux_backend_exception( rsutils::string::from() << __FUNCTION__ << ": locking failed" );
                    }
                }
                catch(...)
                {
                    _lock_counter.fetch_add( -1 );
                    
                    // Close file descriptor if it was opened
                    close_fd();
                    
                    throw;
                }
            }
        }

        void named_mutex::unlock()
        {
            if( _lock_counter.fetch_add( -1 ) == 1 )
            {
                struct flock fl;
                memset( &fl, 0, sizeof( fl ) );
                fl.l_whence = SEEK_SET;
                fl.l_start = 0;
                fl.l_len = 0;
                fl.l_type = F_UNLCK;
                auto ret = fcntl( _fildes, F_OFD_SETLKW, &fl );
                if( 0 != ret )
                    throw linux_backend_exception( rsutils::string::from() << __FUNCTION__ << ": unlocking failed" );

                // Close file descriptor when last lock is released
                close_fd();
            }
        }

        bool named_mutex::try_lock()
        {
            if( _lock_counter.fetch_add( 1 ) == 0 )
            {
                try
                {
                    // Open file descriptor only when first lock is acquired
                    ensure_fd_open();

                    struct flock fl;
                    memset( &fl, 0, sizeof( fl ) );
                    fl.l_whence = SEEK_SET;
                    fl.l_start = 0;
                    fl.l_len = 0; // Lock the entire file
                    fl.l_type = F_WRLCK;
                    auto ret = fcntl( _fildes, F_OFD_SETLK, &fl );
                    if( 0 != ret )
                    {
                        // fcntl failed - need to clean up
                        // Close file descriptor since we failed to acquire lock
                        close_fd();
                        
                        // Return false instead of throwing, but still need to decrement counter
                        _lock_counter.fetch_add( -1 );
                        return false;
                    }
                }
                catch(...)
                {
                    _lock_counter.fetch_add( -1 );
                    
                    // Close file descriptor if it was opened
                    close_fd();
                    
                    return false;
                }
            }

            return true;
        }
    }  // namespace platform
}  // namespace librealsense
