// Copyright 2015-2018 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_RUNTIME_C__SEQUENCE_BOUND_H_
#define ROSIDL_RUNTIME_C__SEQUENCE_BOUND_H_

#include "rosidl_runtime_c/visibility_control.h"
#include "rosidl_typesupport_interface/macros.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct rosidl_runtime_c__Sequence__bound rosidl_runtime_c__Sequence__bound;

typedef const rosidl_runtime_c__Sequence__bound * (* rosidl_runtime_c__bound_handle_function)(
  const rosidl_runtime_c__Sequence__bound *, const char *);

struct rosidl_runtime_c__Sequence__bound
{
  /// String identifier for the type_support.
  const char * typesupport_identifier;
  /// Pointer to type support handle function
  const void * data;
  rosidl_runtime_c__bound_handle_function func;
};

/// Get the message bounds handle specific to this identifier.
/**
 * The handle's sequence bound is simply returned or if the parameters are NULL then an assert
 * will happen.
 *
 * \param handle Handle to service type support
 * \param identifier The identifier to get the handle for
 * \return The associated sequence bound handle function.
 */
ROSIDL_GENERATOR_C_PUBLIC
const rosidl_runtime_c__Sequence__bound * get_sequence_bound_handle(
  const rosidl_runtime_c__Sequence__bound * handle, const char * identifier);

/// Get the message bounds handle function specific to this identifier.
/**
 * If the identifier is the same as this handle function's identifier the handle is simply returned
 * or if the parameters are NULL then an assert will happen.
 *
 * \param handle Handle to service type support
 * \param identifier The identifier to get the handle function for
 * \return if the identifier match's the handle's identifier then the handle's function
 *   is returned.
 */
ROSIDL_GENERATOR_C_PUBLIC
const rosidl_runtime_c__Sequence__bound * get_sequence_bound_handle_function(
  const rosidl_runtime_c__Sequence__bound * handle, const char * identifier);

/// Get the sequence bounds given a provided action and package.
/*
 * \param PkgName Name of the package that contains the message
 * \param MsgSubfolder name of the subfolder (foe example: msg)
 * \param MsgName message name
 * \return a rosidl_message_bounds_t struct if founded, otherwise NULL.
 */
#define ROSIDL_GET_SEQUENCE_BOUNDS(PkgName, MsgSubfolder, MsgName) \
  ROSIDL_BOUNDS_INTERFACE__MESSAGE_SYMBOL_NAME( \
    rosidl_typesupport_c, PkgName, MsgSubfolder, MsgName)()

#ifdef __cplusplus
}
#endif

#endif  // ROSIDL_RUNTIME_C__SEQUENCE_BOUND_H_
