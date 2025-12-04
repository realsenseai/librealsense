// Copyright 2016 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_TYPESUPPORT_CPP__MESSAGE_TYPE_SUPPORT_DISPATCH_HPP_
#define ROSIDL_TYPESUPPORT_CPP__MESSAGE_TYPE_SUPPORT_DISPATCH_HPP_

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_runtime_c/visibility_control.h"

#include "rosidl_typesupport_cpp/visibility_control.h"

namespace rosidl_typesupport_cpp
{

/// Get the message type support handle specific to this identifier.
/**
 * If the identifier is the same as this handle's typesupport_identifier, then the handle is
 * simply returned, otherwise it's loaded from a shared library.
 *
 * \param handle Handle to message type support
 * \param identifier The typesupport identifier to get the handle function for
 * \return The associated message typesupport handle if found, otherwise NULL
 */
ROSIDL_TYPESUPPORT_CPP_PUBLIC
const rosidl_message_type_support_t *
get_message_typesupport_handle_function(
  const rosidl_message_type_support_t * handle, const char * identifier);

}  // namespace rosidl_typesupport_cpp

#endif  // ROSIDL_TYPESUPPORT_CPP__MESSAGE_TYPE_SUPPORT_DISPATCH_HPP_
