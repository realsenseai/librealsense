// Copyright (c) 2019, Open Source Robotics Foundation, Inc.
// All rights reserved.
//
// Software License Agreement (BSD License 2.0)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//  * Neither the name of the copyright holders nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef RCPPUTILS__VISIBILITY_CONTROL_HPP_
#define RCPPUTILS__VISIBILITY_CONTROL_HPP_

/*! \file visibility_control.hpp
  * \brief Macros for controlling visibilty of exported iterfaces.
  *
  * This logic was borrowed (then namespaced) from the examples on the gcc wiki:
  *     https://gcc.gnu.org/wiki/Visibility
  */
/**
  * \def RCPPUTILS_EXPORT
  * \brief Exposes the function with its decorated name in the compiled library object.
  */
/**
  * \def RCPPUTILS_IMPORT
  * \brief On Windows declares a function will be imported from a dll, otherwise it is empty
  */
/**
  * \def RCPPUTILS_PUBLIC
  * \brief Declares symbols and functions will be visible for export.
  */
/**
  * \def RCPPUTILS_PUBLIC_TYPE
  * \brief On Windows, this is a replica of RCPPUTILS_PUBLIC, otherwise it is empty.
  */
/**
  * \def RCPPUTILS_LOCAL
  * \brief Declares symbols cannot be exported from the dll.
  */

#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define RCPPUTILS_EXPORT __attribute__ ((dllexport))
    #define RCPPUTILS_IMPORT __attribute__ ((dllimport))
  #else
    #define RCPPUTILS_EXPORT __declspec(dllexport)
    #define RCPPUTILS_IMPORT __declspec(dllimport)
  #endif
  #ifdef RCPPUTILS_BUILDING_LIBRARY
    #define RCPPUTILS_PUBLIC RCPPUTILS_EXPORT
  #else
    #define RCPPUTILS_PUBLIC RCPPUTILS_IMPORT
  #endif
  #define RCPPUTILS_PUBLIC_TYPE RCPPUTILS_PUBLIC
  #define RCPPUTILS_LOCAL
#else
  #define RCPPUTILS_EXPORT __attribute__ ((visibility("default")))
  #define RCPPUTILS_IMPORT
  #if __GNUC__ >= 4
    #define RCPPUTILS_PUBLIC __attribute__ ((visibility("default")))
    #define RCPPUTILS_LOCAL  __attribute__ ((visibility("hidden")))
  #else
    #define RCPPUTILS_PUBLIC
    #define RCPPUTILS_LOCAL
  #endif
  #define RCPPUTILS_PUBLIC_TYPE
#endif

#endif  // RCPPUTILS__VISIBILITY_CONTROL_HPP_
