// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.
#pragma once

#include "core/enum-helpers.h"
#include <librealsense2/hpp/rs_types.hpp>

#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/os/ensure-console.h>

#include <stdexcept>
#include <fstream>


namespace librealsense
{
    void log_to_console( rs2_log_severity min_severity );
    void log_to_file( rs2_log_severity min_severity, const char * file_path );
    void log_to_callback( rs2_log_severity min_severity, rs2_log_callback_sptr callback );
    void reset_logger();
    void enable_rolling_log_file( unsigned max_size );

#if BUILD_EASYLOGGINGPP
    struct log_message
    {
        el::LogMessage const& el_msg;
        std::string built_msg;

        log_message( el::LogMessage const& el_msg ) : el_msg( el_msg ) {}

        unsigned get_log_message_line_number() const {
            return el_msg.line();
        }

        const char* get_log_message_filename() const {
            return el_msg.file().c_str();
        }

        const char* get_raw_log_message() const {
            return el_msg.message().c_str();
        }

        const char* get_full_log_message() {
            if (built_msg.empty())
            {
                bool const append_new_line = false;
                built_msg = el_msg.logger()->logBuilder()->build(&el_msg, append_new_line);
            }
            return built_msg.c_str();
        }
    };

    template<char const * NAME>
    class logger_type
    {
        rs2_log_severity minimum_console_severity = RS2_LOG_SEVERITY_NONE;
        rs2_log_severity minimum_file_severity = RS2_LOG_SEVERITY_NONE;
        rs2_log_severity minimum_callback_severity = RS2_LOG_SEVERITY_NONE;

        std::ofstream log_file;
        std::vector< std::string > callback_dispatchers;

        std::string filename;
        const std::string log_id = NAME;

        // el::Loggers::reconfigureLogger() ends up in Logger::configure(), which mutates state before taking
        // its own lock (a bug present in every released version of this now-archived, unmaintained library --
        // see RSDSO-21284). We can't patch third-party code, so we serialize externally instead, using the
        // same (recursive) el::Logger lock that LIBRS_LOG_/LIBRS_LOG_STR_ already hold while reading.
        template< typename Fn >
        void reconfigure_locked( Fn && fn ) const
        {
            auto * logger = el::Loggers::getLogger( log_id );
            if( ! logger )
                return;
            el::base::threading::ScopedLock lock( logger->lock() );
            fn();
        }

    public:
        static el::Level severity_to_level(rs2_log_severity severity)
        {
            switch (severity)
            {
            case RS2_LOG_SEVERITY_DEBUG: return el::Level::Debug;
            case RS2_LOG_SEVERITY_INFO: return el::Level::Info;
            case RS2_LOG_SEVERITY_WARN: return el::Level::Warning;
            case RS2_LOG_SEVERITY_ERROR: return el::Level::Error;
            case RS2_LOG_SEVERITY_FATAL: return el::Level::Fatal;
            default: return el::Level::Unknown;
            }
        }

        static rs2_log_severity level_to_severity( el::Level level )
        {
            switch( level )
            {
            case el::Level::Debug:   return RS2_LOG_SEVERITY_DEBUG;  // LOG(DEBUG)    - useful for developers to debug application
            case el::Level::Trace:   return RS2_LOG_SEVERITY_DEBUG;  // LOG(TRACE)    - useful to back-trace certain events
            case el::Level::Info:    return RS2_LOG_SEVERITY_INFO;   // LOG(INFO)     - show current progress of application
            case el::Level::Verbose: return RS2_LOG_SEVERITY_INFO;   // LOG(VERBOSE)  - can be highly useful but varies with verbose logging level
            case el::Level::Warning: return RS2_LOG_SEVERITY_WARN;   // LOG(WARNING)  - potentially harmful situtaions
            case el::Level::Error:   return RS2_LOG_SEVERITY_ERROR;  // LOG(ERROR)    - errors in application but application will keep running
            case el::Level::Fatal:   return RS2_LOG_SEVERITY_FATAL;  // LOG(FATAL)    - severe errors that will presumably abort application
            default: 
                // Unknown/Global, or otherwise
                // Global should never occur (no CGLOBAL macro); same for Unknown...
                return RS2_LOG_SEVERITY_ERROR;  // treat as error, since we don't know what it is...
            }
        }

        void open() const
        {
            el::Configurations defaultConf;
            defaultConf.setToDefault();
            // To set GLOBAL configurations you may use

            defaultConf.setGlobally( el::ConfigurationType::Enabled, "false" );
            defaultConf.setGlobally( el::ConfigurationType::ToFile, "false" );
            defaultConf.setGlobally( el::ConfigurationType::ToStandardOutput, "false" );
            defaultConf.setGlobally( el::ConfigurationType::LogFlushThreshold, "10" );
            defaultConf.setGlobally( el::ConfigurationType::Format,
                                     " %datetime{%d/%M %H:%m:%s,%g} %level [%thread] (%fbase:%line) %msg" );

            // NOTE: you can only log to one file, so successive calls to log_to_file
            // will override one another!
            if (minimum_file_severity != RS2_LOG_SEVERITY_NONE)
            {
                // Configure filename for logging only if the severity requires it, which
                // prevents creation of empty log files that are not required.
                defaultConf.setGlobally(el::ConfigurationType::Filename, filename);
            }

            auto min_severity
                = std::min( minimum_file_severity, std::min( minimum_callback_severity, minimum_console_severity ) );

            for( int i = RS2_LOG_SEVERITY_DEBUG; i < RS2_LOG_SEVERITY_NONE; ++i )
            {
                auto severity = static_cast< rs2_log_severity >( i );
                auto level = severity_to_level( severity );
                defaultConf.set( level, el::ConfigurationType::Enabled, severity >= min_severity ? "true" : "false" );
                defaultConf.set( level,
                                 el::ConfigurationType::ToStandardOutput,
                                 severity >= minimum_console_severity ? "true" : "false" );
                defaultConf.set( level,
                                 el::ConfigurationType::ToFile,
                                 severity >= minimum_file_severity ? "true" : "false" );
            }

            reconfigure_locked( [&] { el::Loggers::reconfigureLogger( log_id, defaultConf ); } );
        }

        void open_def() const
        {
            el::Configurations defaultConf;
            defaultConf.setToDefault();
            // To set GLOBAL configurations you may use

            defaultConf.setGlobally( el::ConfigurationType::Enabled, "false" );
            defaultConf.setGlobally( el::ConfigurationType::ToFile, "false" );
            defaultConf.setGlobally( el::ConfigurationType::ToStandardOutput, "false" );

            reconfigure_locked( [&] { el::Loggers::reconfigureLogger( log_id, defaultConf ); } );
        }


        logger_type()
            : filename( rsutils::string::from::datetime() + ".log" )
        {
            if( try_get_log_severity( minimum_file_severity ) )
                open();
            else
                open_def();
        }

        static bool try_get_log_severity(rs2_log_severity& severity)
        {
            static const char* severity_var_name = "LRS_LOG_LEVEL";
            auto content = getenv(severity_var_name);

            if (content)
            {
                std::string content_str(content);
                std::transform(content_str.begin(), content_str.end(), content_str.begin(), ::tolower);

                for (uint32_t i = 0; i < RS2_LOG_SEVERITY_COUNT; i++)
                {
                    auto current = (rs2_log_severity)i;
                    std::string name = librealsense::get_string(current);
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                    if (content_str == name)
                    {
                        severity = current;
                        return true;
                    }
                }
            }

            return false;
        }

        void log_to_console(rs2_log_severity min_severity)
        {
            if( min_severity != RS2_LOG_SEVERITY_NONE )
                rsutils::os::ensure_console( false );  // don't create if none available
            minimum_console_severity = min_severity;
            open();
        }

        void log_to_file(rs2_log_severity min_severity, const char * file_path)
        {
            if (!try_get_log_severity(minimum_file_severity))
                minimum_file_severity = min_severity;

            if (file_path)
                filename = file_path;

            open();
        }

    protected:
        // Create a dispatch sink which will get any messages logged to EasyLogging, which will then
        // post the messages on the viewer's notification window.
        class elpp_dispatcher : public el::LogDispatchCallback
        {
        public:
            rs2_log_callback_sptr callback;
            rs2_log_severity min_severity = RS2_LOG_SEVERITY_NONE;

        protected:
            void handle( el::LogDispatchData const* data ) noexcept override
            {
                // NOTE: once a callback is set, it gets ALL messages -- even from other (non-"librealsense") loggers!
                el::LogMessage const& msg = *data->logMessage();
                auto severity = level_to_severity( msg.level() );
                if( callback && severity >= min_severity )
                {
                    log_message msg_wrapper( msg );
                    callback->on_log( severity, reinterpret_cast<rs2_log_message const&>(msg_wrapper) );  // noexcept!
                }
            }
        };

    public:
        void remove_callbacks()
        {
            for( auto const& dispatch : callback_dispatchers )
                el::Helpers::uninstallLogDispatchCallback< elpp_dispatcher >( dispatch );
            callback_dispatchers.clear();
            minimum_callback_severity = RS2_LOG_SEVERITY_NONE;
        }

        void log_to_callback( rs2_log_severity min_severity, rs2_log_callback_sptr callback )
        {
            try_get_log_severity( min_severity );
            if( callback  &&  min_severity != RS2_LOG_SEVERITY_NONE )
            {
                // Each callback we install must have a unique name
                std::ostringstream ss;
                ss << "elpp_dispatcher_" << callback_dispatchers.size();
                std::string dispatch_name = ss.str();

                // Record all the dispatchers we have so we can erase them
                callback_dispatchers.push_back( dispatch_name );

                // The only way to install the callback in ELPP is with a default ctor...
                el::Helpers::installLogDispatchCallback< elpp_dispatcher >( dispatch_name );
                auto dispatcher = el::Helpers::logDispatchCallback< elpp_dispatcher >( dispatch_name );
                dispatcher->callback = callback;
                dispatcher->min_severity = min_severity;
                if( min_severity < minimum_callback_severity )
                    minimum_callback_severity = min_severity;
                
                // Remove the default logger (which will log to standard out/err) or it'll still be active
                //el::Helpers::uninstallLogDispatchCallback< el::base::DefaultLogDispatchCallback >( "DefaultLogDispatchCallback" );

                open();
            }
        }

        // Stop logging and reset logger to initial configurations
        void reset_logger()
        {
            // One lock for all four, so a concurrent reader can't observe a partially-reset logger
            // (e.g. Enabled=false but ToFile still true).
            reconfigure_locked( [&] {
                el::Loggers::reconfigureLogger( log_id, el::ConfigurationType::Enabled, "false" );
                el::Loggers::reconfigureLogger( log_id, el::ConfigurationType::ToFile, "false" );
                el::Loggers::reconfigureLogger( log_id, el::ConfigurationType::ToStandardOutput, "false" );
                el::Loggers::reconfigureLogger( log_id, el::ConfigurationType::MaxLogFileSize, "0" );
            } );
            remove_callbacks();

            minimum_console_severity = RS2_LOG_SEVERITY_NONE;
            minimum_file_severity = RS2_LOG_SEVERITY_NONE;
        }

        // Callback: called by EL++ when the current log file has reached a certain maximum size.
        // When the callback returns, the log-file will be truncated and reinitialized.
        // We rename the current log file to "<filename>.old" so as not to lose the contents.
        static void rolloutHandler( const char * filename, std::size_t size )
        {
            std::string file_str( filename );
            std::string old_file = file_str + ".old";

            // Remove any existing .old
            const char* old_filename = old_file.c_str();
            std::ifstream exists( old_filename );
            if( exists.is_open() )
            {
                exists.close();
                std::remove( old_filename );
            }

            std::rename( filename, old_filename );
        }

        // Enable rolling log file:
        // Upon reaching (max_size/2) bytes, the log will be renamed with an ".old" suffix and a new log created. Any
        // previous .old file will be erased.
        // Must have permissions to remove/rename files in log file directory.
        //
        // @param max_size max file size in megabytes
        //
        void enable_rolling_log_file( unsigned max_size )
        {
            auto max_size_in_bytes = max_size * 1024 * 1024;
            std::string size = std::to_string(max_size_in_bytes / 2 );

            // Rollout checking happens when Easylogging++ flushes the log file!
            // Or, with the flag el::LoggingFlags::StrictLogFileSizeCheck, at each log output...
            //el::Loggers::addFlag( el::LoggingFlag::StrictLogFileSizeCheck );

            reconfigure_locked( [&] { el::Loggers::reconfigureLogger( log_id, el::ConfigurationType::MaxLogFileSize, size.c_str() ); } );
            el::Helpers::installPreRollOutCallback( rolloutHandler );
        }
    };
#else //BUILD_EASYLOGGINGPP
    struct log_message
    {

        unsigned get_log_message_line_number() const {
            throw std::runtime_error("rs2_get_log_message_line_number is not supported without BUILD_EASYLOGGINGPP");
        }

        const char* get_log_message_filename() const {
            throw std::runtime_error("rs2_get_log_message_filename is not supported without BUILD_EASYLOGGINGPP");
        }

        const char* get_raw_log_message() const {
            throw std::runtime_error("rs2_get_raw_log_message is not supported without BUILD_EASYLOGGINGPP");
        }

        const char* get_full_log_message() {
            throw std::runtime_error("rs2_get_full_log_message is not supported without BUILD_EASYLOGGINGPP");
        }
    };
#endif //BUILD_EASYLOGGINGPP
}
