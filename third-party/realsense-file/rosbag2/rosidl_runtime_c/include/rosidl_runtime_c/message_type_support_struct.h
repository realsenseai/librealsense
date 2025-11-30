// Copyright 2015-2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSIDL_RUNTIME_C__MESSAGE_TYPE_SUPPORT_STRUCT_H_
#define ROSIDL_RUNTIME_C__MESSAGE_TYPE_SUPPORT_STRUCT_H_

#include "rosidl_runtime_c/visibility_control.h"
#include "rosidl_typesupport_interface/macros.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct rosidl_message_type_support_t rosidl_message_type_support_t;

typedef const rosidl_message_type_support_t * (* rosidl_message_typesupport_handle_function)(
  const rosidl_message_type_support_t *, const char *);

/// Contains rosidl message type support data
struct rosidl_message_type_support_t
{
  /// String identifier for the type_support.
  const char * typesupport_identifier;
  /// Pointer to the message type support library
  const void * data;
  /// Pointer to the message type support handler function
  rosidl_message_typesupport_handle_function func;
};

/// Get the message type support handle specific to this identifier.
/**
 * The handle's message typesupport identifier function is returned or if the parameters are NULL
 * then an assert will happen.
 *
 * \param handle Handle to message type support
 * \param identifier The typesupport identifier to get the handle function for
 * \return The associated message typesupport handle function.
 */
ROSIDL_GENERATOR_C_PUBLIC
const rosidl_message_type_support_t * get_message_typesupport_handle(
  const rosidl_message_type_support_t * handle, const char * identifier);

// Get the message type support handle function specific to this identifier.
/**
 * If the identifier is the same as this handle's typesupport_identifier the handle is simply
 * returned or if the parameters are NULL then an assert will happen.
 *
 * \param handle Handle to message type support
 * \param identifier The typesupport identifier to get the handle function for
 * \return if the identifier match's the handle's identifier then the handle's function
 *   is returned.
 */
ROSIDL_GENERATOR_C_PUBLIC
const rosidl_message_type_support_t * get_message_typesupport_handle_function(
  const rosidl_message_type_support_t * handle, const char * identifier);

/// Get the message type support given a provided action and package.
/*
 * \param PkgName Name of the package that contains the message
 * \param MsgSubfolder name of the subfolder (for example: msg)
 * \param MsgName message name
 * \return a rosidl_message_type_support_t struct if founded, otherwise NULL.
 */
#define ROSIDL_GET_MSG_TYPE_SUPPORT(PkgName, MsgSubfolder, MsgName) \
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME( \
    rosidl_typesupport_c, PkgName, MsgSubfolder, MsgName)()

#ifdef __cplusplus
}
#endif

#endif  // ROSIDL_RUNTIME_C__MESSAGE_TYPE_SUPPORT_STRUCT_H_
