// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_types.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include "basics.h"  // LRS_EXTENSION_API

#include <exception>
#include <string>
#include <cstring>


namespace librealsense {


class librealsense_exception : public std::exception
{
public:
    const char * get_message() const noexcept { return _msg.c_str(); }

    rs2_exception_type get_exception_type() const noexcept { return _exception_type; }

    const char * what() const noexcept override { return _msg.c_str(); }

protected:
    librealsense_exception( const std::string & msg, rs2_exception_type exception_type ) noexcept
        : _msg( msg )
        , _exception_type( exception_type )
    {
    }

private:
    std::string _msg;
    rs2_exception_type _exception_type;
};


class LRS_EXTENSION_API recoverable_exception : public librealsense_exception
{
public:
    recoverable_exception( const std::string & msg, rs2_exception_type exception_type ) noexcept;
};


class unrecoverable_exception : public librealsense_exception
{
public:
    unrecoverable_exception( const std::string & msg, rs2_exception_type exception_type ) noexcept
        : librealsense_exception( msg, exception_type )
    {
        LOG_ERROR( msg );
    }
};


class io_exception : public unrecoverable_exception
{
public:
    io_exception( const std::string & msg ) noexcept
        : unrecoverable_exception( msg, RS2_EXCEPTION_TYPE_IO )
    {
    }
};


class camera_disconnected_exception : public unrecoverable_exception
{
public:
    camera_disconnected_exception( const std::string & msg ) noexcept
        : unrecoverable_exception( msg, RS2_EXCEPTION_TYPE_CAMERA_DISCONNECTED )
    {
    }
};


class backend_exception : public unrecoverable_exception
{
public:
    backend_exception( const std::string & msg, rs2_exception_type exception_type ) noexcept
        : unrecoverable_exception( msg, exception_type )
    {
    }
};


class linux_backend_exception : public backend_exception
{
public:
    linux_backend_exception( const std::string & msg ) noexcept
        : backend_exception( generate_last_error_message( msg ), RS2_EXCEPTION_TYPE_BACKEND )
    {
    }

private:
    std::string generate_last_error_message( const std::string & msg ) const
    {
        return msg + " Last Error: " + strerror( errno );
    }
};


class windows_backend_exception : public backend_exception
{
public:
    // TODO: get last error
    windows_backend_exception( const std::string & msg ) noexcept
        : backend_exception( msg, RS2_EXCEPTION_TYPE_BACKEND )
    {
    }
};


class invalid_value_exception : public recoverable_exception
{
public:
    invalid_value_exception( const std::string & msg ) noexcept
        : recoverable_exception( msg, RS2_EXCEPTION_TYPE_INVALID_VALUE )
    {
    }
};


class wrong_api_call_sequence_exception : public recoverable_exception
{
public:
    wrong_api_call_sequence_exception( const std::string & msg ) noexcept
        : recoverable_exception( msg, RS2_EXCEPTION_TYPE_WRONG_API_CALL_SEQUENCE )
    {
    }
};


class not_implemented_exception : public recoverable_exception
{
public:
    not_implemented_exception( const std::string & msg ) noexcept
        : recoverable_exception( msg, RS2_EXCEPTION_TYPE_NOT_IMPLEMENTED )
    {
    }
};


}  // namespace librealsense
