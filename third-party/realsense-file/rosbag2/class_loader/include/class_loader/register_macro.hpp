/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CLASS_LOADER__REGISTER_MACRO_HPP_
#define CLASS_LOADER__REGISTER_MACRO_HPP_

#include <string>

// TODO(mikaelarguedas) remove this once console_bridge complies with this
// see https://github.com/ros/console_bridge/issues/55
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
#include "console_bridge/console.h"
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#include "class_loader/class_loader_core.hpp"

#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Base, UniqueID, Message) \
  namespace \
  { \
  struct ProxyExec ## UniqueID \
  { \
    typedef  Derived _derived; \
    typedef  Base _base; \
    ProxyExec ## UniqueID() \
    { \
      if (!std::string(Message).empty()) { \
        CONSOLE_BRIDGE_logInform("%s", Message); \
      } \
      class_loader::impl::registerPlugin<_derived, _base>(#Derived, #Base); \
    } \
  }; \
  static ProxyExec ## UniqueID g_register_plugin_ ## UniqueID; \
  }  // namespace

#define CLASS_LOADER_REGISTER_CLASS_INTERNAL_HOP1_WITH_MESSAGE(Derived, Base, UniqueID, Message) \
  CLASS_LOADER_REGISTER_CLASS_INTERNAL_WITH_MESSAGE(Derived, Base, UniqueID, Message)

/**
* @macro This macro is same as CLASS_LOADER_REGISTER_CLASS, but will spit out a message when the plugin is registered
* at library load time
*/
#define CLASS_LOADER_REGISTER_CLASS_WITH_MESSAGE(Derived, Base, Message) \
  CLASS_LOADER_REGISTER_CLASS_INTERNAL_HOP1_WITH_MESSAGE(Derived, Base, __COUNTER__, Message)

/**
* @macro This is the macro which must be declared within the source (.cpp) file for each class that is to be exported as plugin.
* The macro utilizes a trick where a new struct is generated along with a declaration of static global variable of same type after it. The struct's constructor invokes a registration function with the plugin system. When the plugin system loads a library with registered classes in it, the initialization of static variables forces the invocation of the struct constructors, and all exported classes are automatically registerd.
*/
#define CLASS_LOADER_REGISTER_CLASS(Derived, Base) \
  CLASS_LOADER_REGISTER_CLASS_WITH_MESSAGE(Derived, Base, "")

#endif  // CLASS_LOADER__REGISTER_MACRO_HPP_
