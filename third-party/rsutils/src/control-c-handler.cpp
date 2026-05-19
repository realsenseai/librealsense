// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#include <rsutils/concurrency/control-c-handler.h>
#include <rsutils/concurrency/event.h>
#include <rsutils/easylogging/easyloggingpp.h>

#include <signal.h>
#include <atomic>
#include <stdexcept>


namespace rsutils {
namespace concurrency {


namespace {

event sigint_ev;
std::atomic_bool in_use( false );


void handle_signal( int signal )
{
    switch( signal )
    {
    case SIGINT:
        sigint_ev.set();
        break;
    }
}

}  // namespace


control_c_handler::control_c_handler()
{
    if( in_use.exchange( true ) )
        throw std::runtime_error( "a Control-C handler is already in effect" );
#ifdef _WIN32
    signal( SIGINT, handle_signal );
#else
    struct sigaction sa;
    sa.sa_handler = &handle_signal;
    sa.sa_flags = SA_RESTART;
    sigfillset( &sa.sa_mask );
    if( sigaction( SIGINT, &sa, NULL ) == -1 )
        LOG_ERROR( "Cannot install SIGINT handler" );
#endif
}


control_c_handler::~control_c_handler()
{
#ifdef _WIN32
    signal( SIGINT, SIG_DFL );
#else
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART;
    sigfillset( &sa.sa_mask );
    if( sigaction( SIGINT, &sa, NULL ) == -1 )
        LOG_DEBUG( "cannot uninstall SIGINT handler" );
#endif
    in_use = false;
}


bool control_c_handler::was_pressed() const
{
    return sigint_ev.is_set();
}


void control_c_handler::reset()
{
    sigint_ev.clear();
}


void control_c_handler::wait()
{
    sigint_ev.clear_and_wait();
}


}  // namespace concurrency
}  // namespace rsutils
