// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <rsutils/os/network-adapter-watcher.h>
#include <rsutils/shared-ptr-singleton.h>
#include <rsutils/signal.h>
#include <rsutils/easylogging/easyloggingpp.h>

#if ! defined( __APPLE__ ) && ! defined( __ANDROID__ )
#ifdef _WIN32

#include <windows.h>
#include <iphlpapi.h>
#pragma comment( lib, "iphlpapi.lib" )

#else

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <thread>

#endif
#endif  // ! __APPLE__ && ! __ANDROID__


namespace rsutils {
namespace os {
namespace detail {


#if ! defined( __APPLE__ ) && ! defined( __ANDROID__ )
class network_adapter_watcher_singleton
{
    using public_signal = rsutils::public_signal< network_adapter_watcher_singleton >;

#ifdef _WIN32
    HANDLE _done;
#else
    int _done;
    int _socket;
#endif
    std::thread _th;

public:
    public_signal callbacks;

    network_adapter_watcher_singleton()
#ifdef _WIN32
        : _done( CreateEvent(
            nullptr /*security attrs*/, TRUE /*manual reset*/, FALSE /*init unset*/, nullptr /*no name*/ ) )
        , _th(
              [this]
              {
                  // We're going to wait on the following handles:
                  HANDLE handles[2];
                  DWORD const n_handles = sizeof( handles ) / sizeof( handles[0] );
                  handles[0] = _done;
                  // https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-notifyaddrchange
                  OVERLAPPED overlap = { 0 };
                  overlap.hEvent = CreateEvent( nullptr /*security attrs*/,
                                                FALSE /*auto reset*/,
                                                FALSE /*init unset*/,
                                                nullptr /*no name*/ );
                  handles[1] = overlap.hEvent;

                  LOG_DEBUG( "starting network adapter watcher" );

                  while( true )
                  {
                      HANDLE h_file = NULL;
                      NotifyAddrChange( &h_file, &overlap );
                      // If multiple objects become signaled, the function returns the index of the first handle in the
                      // array whose object was signaled:
                      auto result = WaitForMultipleObjects( n_handles, handles, FALSE /*wait-for-all*/, INFINITE );
                      if( WAIT_OBJECT_0 == result )
                          break;  // we're done
                      if( ( WAIT_OBJECT_0 + 1 ) == result )
                      {
                          LOG_DEBUG( "network adapter changes detected!" );
                          callbacks.raise();
                      }
                      else
                      {
                          auto last_error = GetLastError();
                          LOG_ERROR( "Network adapter wait failed (" << result << "); GetLastError is " << last_error );
                          break;
                      }
                  }

                  LOG_DEBUG( "exiting network adapter watcher" );

                  CancelIPChangeNotify( &overlap );
              } )
#else
        // https://stackoverflow.com/questions/27008067/how-to-get-notified-about-network-interface-changes-with-netlist-and-rtmgrp-link
        : _socket( open_netlink() )
        , _done( eventfd( 0, 0 ) )
#endif
    {
#ifndef _WIN32
        if( _socket < 0 )
        {
            auto const error = errno;
            LOG_ERROR( "Failed to start network adapter watcher (errno= " << error << ")" );
        }
        else
        {
            _th = std::thread(
                [this]
                {
                    // We're going to wait on the following handles:
                    LOG_DEBUG( "starting network adapter watcher" );

                    int const timeout = -1;  // negative value = infinite
                    int const nfds = 2;
                    pollfd fds[nfds];
                    memset( fds, 0, sizeof( fds ) );
                    fds[0].fd = _done;
                    fds[0].events = POLLIN;
                    fds[1].fd = _socket;
                    fds[1].events = POLLIN;

                    while( true )
                    {
                        fds[0].revents = 0;
                        fds[1].revents = 0;
                        auto rv = poll( fds, nfds, timeout );
                        if( rv <= 0 )
                        {
                            LOG_ERROR( "poll failed: " << rv );
                            break;
                        }

                        if( fds[0].revents != 0 )
                            break;  // we're done

                        LOG_DEBUG( "network adapter changes detected!" );

                        char buf[4096];
                        struct sockaddr_nl snl;
                        iovec iov = { buf, sizeof( buf ) };
                        msghdr msg = { (void *)&snl, sizeof snl, &iov, 1, NULL, 0, 0 };
                        int status = recvmsg( _socket, &msg, 0 );
                        if( status < 0 )
                        {
                            // Socket non-blocking so bail out once we have read everything
                            if( errno == EWOULDBLOCK || errno == EAGAIN )
                                continue;

                            // Anything else is an error
                            LOG_DEBUG( "read_netlink: Error recvmsg: " << status );
                            break;
                        }
                        if( status == 0 )
                            LOG_DEBUG( "read_netlink: EOF" );

                        // We need to handle more than one message per recvmsg
                        for( auto h = (struct nlmsghdr *)buf; NLMSG_OK( h, (unsigned int)status );
                             h = NLMSG_NEXT( h, status ) )
                        {
                            // Finish reading
                            if( h->nlmsg_type == NLMSG_DONE )
                                continue;

                            // Message is some kind of error
                            if( h->nlmsg_type == NLMSG_ERROR )
                            {
                                LOG_DEBUG( "read_netlink: got NLMSG_ERROR" );
                                continue;
                            }
                        }

                        // Call our clients once for all the messages... outside the loop
                        callbacks.raise();
                    }

                    LOG_DEBUG( "exiting network adapter watcher" );
                } );
        }
#endif
    }

    ~network_adapter_watcher_singleton()
    {
#ifdef _WIN32
        if( _th.joinable() )
        {
            SetEvent( _done );
            _th.join();
        }
        CloseHandle( _done );
#else
        if( _th.joinable() )
        {
            uint64_t incr = 1;  // must be 8-byte integer value
            auto rv = write( _done, &incr, sizeof( incr ) );
            if( rv != sizeof( incr ) )
                LOG_WARNING( "failed to write to network adapter watcher done event: " << rv );
            else
                _th.join();
        }
        close( _socket );
        close( _done );
#endif
    }

#ifndef _WIN32
    static int open_netlink()
    {
        // NETLINK_ROUTE
        //      Receives routing and link updates and may be used to modify the routing tables
        //      (both IPv4 and IPv6), IP addresses, link parameters, neighbor setups, queueing
        //      disciplines, traffic classes, and packet classifiers.
        // https://manpages.ubuntu.com/manpages/oracular/en/man7/netlink.7.html
        int sock = socket( AF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
        if( sock < 0 )
            return -1;

        struct sockaddr_nl addr;
        memset( (void *)&addr, 0, sizeof( addr ) );
        addr.nl_family = AF_NETLINK;
        addr.nl_pid = getpid();
        addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
        if( bind( sock, (struct sockaddr *)&addr, sizeof( addr ) ) < 0 )
        {
            close( sock );
            return -1;
        }

        return sock;
    }
#endif
};


static rsutils::shared_ptr_singleton< detail::network_adapter_watcher_singleton > the_adapter_watcher;

#endif  // ! __APPLE__ && ! __ANDROID__


}  // namespace detail


network_adapter_watcher::network_adapter_watcher( callback && cb )
#if ! defined( __APPLE__ ) && ! defined( __ANDROID__ )
    : _singleton( detail::the_adapter_watcher.instance() )  // keep it alive
    , _subscription( _singleton->callbacks.subscribe( std::move( cb ) ) )
#endif
{
    // As long as someone keeps a pointer to an network_adapter_watcher, the singleton will be kept alive and it will watch for
    // changes; as soon as all instances disappear, the singleton will disappear and the watch should stop.
}


}  // namespace os
}  // namespace rsutils
