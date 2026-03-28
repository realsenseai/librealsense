// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_HPP
#define LIBREALSENSE_RS2_HPP

#include "rs.h"
#include "hpp/rs_types.hpp"
#include "hpp/rs_context.hpp"
#include "hpp/rs_device.hpp"
#include "hpp/rs_frame.hpp"
#include "hpp/rs_processing.hpp"
#include "hpp/rs_record_playback.hpp"
#include "hpp/rs_sensor.hpp"
#include "hpp/rs_safety_sensor.hpp"
#include "hpp/rs_pipeline.hpp"
#include "hpp/rs_eth_config.hpp"

namespace rs2
{
    inline void log_to_console(rs2_log_severity min_severity)
    {
        rs2_error* e = nullptr;
        rs2_log_to_console(min_severity, &e);
        error::handle(e);
    }

    inline void log_to_file(rs2_log_severity min_severity, const char * file_path = nullptr)
    {
        rs2_error* e = nullptr;
        rs2_log_to_file(min_severity, file_path, &e);
        error::handle(e);
    }

    inline void reset_logger()
    {
        rs2_error* e = nullptr;
        rs2_reset_logger(&e);
        error::handle(e);
    }

    // Enable rolling log file when used with rs2_log_to_file:
    // Upon reaching (max_size/2) bytes, the log will be renamed with an ".old" suffix and a new log created. Any
    // previous .old file will be erased.
    // Must have permissions to remove/rename files in log file directory.
    //
    // @param max_size max file size in megabytes
    //
    inline void enable_rolling_log_file( unsigned max_size )
    {
        rs2_error * e = nullptr;
        rs2_enable_rolling_log_file( max_size, &e );
        error::handle( e );
    }
    
    /*
        Interface to the log message data we expose.
    */
    class log_message
    {
        // Only log_callback should be creating us!
        friend class log_callback;

        log_message( rs2_log_message const & msg ) : _msg( msg ) {}

    public:
        /* Returns the line-number in the file where the LOG() comment was issued */
        size_t line_number() const
        {
            rs2_error* e = nullptr;
            size_t ln = rs2_get_log_message_line_number( &_msg, &e );
            error::handle( e );
            return ln;
        }
        /* Returns the file in which the LOG() command was issued */
        const char * filename() const
        {
            rs2_error* e = nullptr;
            const char * path = rs2_get_log_message_filename( &_msg, &e );
            error::handle( e );
            return path;
        }
        /* Returns the raw message, as it was passed to LOG(), before any embelishments like level etc. */
        const char* raw() const
        {
            rs2_error* e = nullptr;
            const char* r = rs2_get_raw_log_message( &_msg, &e );
            error::handle( e );
            return r;
        }
        /*
            Returns a complete log message, as defined by librealsense, with level, timestamp, etc.:
                11/12 13:49:40,153 INFO [10604] (rs.cpp:2271) Framebuffer size changed to 1552 x 919
        */
        const char* full() const
        {
            rs2_error* e = nullptr;
            const char* str = rs2_get_full_log_message( &_msg, &e );
            error::handle( e );
            return str;
        }

    private:
        rs2_log_message const & _msg;
    };

    /*
        Wrapper around any callback function that is given to log_to_callback.
    */
    class log_callback : public rs2_log_callback
    {
    public:
        typedef std::function< void( rs2_log_severity, rs2::log_message const & ) > log_fn;

    private:
        log_fn on_log_function;

    public:
        explicit log_callback( log_fn && on_log )
            : on_log_function( std::move( on_log ) )
        {
        }

        void on_log( rs2_log_severity severity, rs2_log_message const & msg ) noexcept override
        {
            on_log_function( severity, log_message( msg ));
        }

        void release() override { delete this; }
    };

    /*
        Your callback should look like this, for example:
            void callback( rs2_log_severity severity, rs2::log_message const & msg ) noexcept
            {
                std::cout << msg.build() << std::endl;
            }
        and, when initializing rs2:
            rs2::log_to_callback( callback );
        or:
            rs2::log_to_callback(
                []( rs2_log_severity severity, rs2::log_message const & msg ) noexcept
                {
                    std::cout << msg.build() << std::endl;
                })
    */
    inline void log_to_callback( rs2_log_severity min_severity, log_callback::log_fn callback )
    {
        // We wrap the callback with an interface and pass it to librealsense, who will
        // now manage its lifetime. Rather than deleting it, though, it will call its
        // release() function, where (back in our context) it can be safely deleted:
        rs2_error* e = nullptr;
        rs2_log_to_callback_cpp( min_severity, new log_callback( std::move( callback )), &e );
        error::handle( e );
    }

    inline void log(rs2_log_severity severity, const char* message)
    {
        rs2_error* e = nullptr;
        rs2_log(severity, message, &e);
        error::handle(e);
    }

    /*
        Wrapper around any callback function that is given to set_thread_start_callback.
    */
    class thread_start_callback : public rs2_thread_start_callback
    {
    public:
        typedef std::function< void( rs2_thread_category, const char * ) > callback_fn;

    private:
        callback_fn _on_thread_start;

    public:
        explicit thread_start_callback( callback_fn && on_thread_start )
            : _on_thread_start( std::move( on_thread_start ) )
        {
        }

        void on_thread_start( rs2_thread_category category, const char * name ) noexcept override
        {
            _on_thread_start( category, name );
        }

        void release() override { delete this; }
    };

    /*
        Register a callback to be invoked at the start of every library-created thread.
        The callback runs inside the new thread, so the user can call platform APIs to
        adjust thread priority.

        Example:
            rs2::set_thread_start_callback(
                []( rs2_thread_category category, const char * name )
                {
                    // Adjust thread priority based on category...
                });
    */
    inline void set_thread_start_callback( thread_start_callback::callback_fn callback )
    {
        rs2_error * e = nullptr;
        rs2_set_thread_start_callback_cpp( new thread_start_callback( std::move( callback ) ), &e );
        error::handle( e );
    }
}

inline std::ostream & operator << (std::ostream & o, rs2_stream stream) { return o << rs2_stream_to_string(stream); }
inline std::ostream & operator << (std::ostream & o, rs2_format format) { return o << rs2_format_to_string(format); }
inline std::ostream & operator << (std::ostream & o, rs2_distortion distortion) { return o << rs2_distortion_to_string(distortion); }
inline std::ostream & operator << (std::ostream & o, rs2_option option) { return o << rs2_option_to_string(option); } // This function is being deprecated. For existing options it will return option name, but for future API additions the user should call rs2_get_option_name instead.
inline std::ostream & operator << (std::ostream & o, rs2_log_severity severity) { return o << rs2_log_severity_to_string(severity); }
inline std::ostream & operator << (std::ostream & o, rs2::log_message const & msg ) { return o << msg.raw(); }
inline std::ostream & operator << (std::ostream & o, rs2_camera_info camera_info) { return o << rs2_camera_info_to_string(camera_info); }
inline std::ostream & operator << (std::ostream & o, rs2_frame_metadata_value metadata) { return o << rs2_frame_metadata_to_string(metadata); }
inline std::ostream & operator << (std::ostream & o, rs2_timestamp_domain domain) { return o << rs2_timestamp_domain_to_string(domain); }
inline std::ostream & operator << (std::ostream & o, rs2_notification_category notificaton) { return o << rs2_notification_category_to_string(notificaton); }
inline std::ostream & operator << (std::ostream & o, rs2_sr300_visual_preset preset) { return o << rs2_sr300_visual_preset_to_string(preset); }
inline std::ostream & operator << (std::ostream & o, rs2_exception_type exception_type) { return o << rs2_exception_type_to_string(exception_type); }
inline std::ostream & operator << (std::ostream & o, rs2_playback_status status) { return o << rs2_playback_status_to_string(status); }
inline std::ostream & operator << (std::ostream & o, rs2_l500_visual_preset preset) {return o << rs2_l500_visual_preset_to_string(preset);}
inline std::ostream & operator << (std::ostream & o, rs2_sensor_mode mode) { return o << rs2_sensor_mode_to_string(mode); }
inline std::ostream & operator << (std::ostream & o, rs2_calibration_type mode) { return o << rs2_calibration_type_to_string(mode); }
inline std::ostream & operator << (std::ostream & o, rs2_calibration_status mode) { return o << rs2_calibration_status_to_string(mode); }
inline std::ostream & operator << (std::ostream & o, rs2_eth_link_priority priority) { return o << rs2_eth_link_priority_to_string(priority); }
inline std::ostream & operator << (std::ostream & o, rs2_thread_category category) { return o << rs2_thread_category_to_string(category); }

#endif // LIBREALSENSE_RS2_HPP
