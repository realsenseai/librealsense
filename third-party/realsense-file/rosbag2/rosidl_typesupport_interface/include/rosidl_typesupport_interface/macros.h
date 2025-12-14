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

#ifndef ROSIDL_TYPESUPPORT_INTERFACE__MACROS_H_
#define ROSIDL_TYPESUPPORT_INTERFACE__MACROS_H_

/// Declare the typesupport symbol name. Note: this should not be called directly.
#define ROSIDL_TYPESUPPORT_INTERFACE__SYMBOL_NAME( \
    typesupport_name, function_name, package_name, interface_type, interface_name) \
  typesupport_name ## __ ## function_name ## __ ## \
  package_name ## __ ## interface_type ## __ ## interface_name

/// Declare a typesupport symbol name for a rosidl message definition.
#define ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME( \
    typesupport_name, package_name, interface_type, message_name) \
  ROSIDL_TYPESUPPORT_INTERFACE__SYMBOL_NAME( \
    typesupport_name, get_message_type_support_handle, \
    package_name, interface_type, message_name)

/// Declare a typesupport symbol name for a rosidl service definition.
#define ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME( \
    typesupport_name, package_name, interface_type, service_name) \
  ROSIDL_TYPESUPPORT_INTERFACE__SYMBOL_NAME( \
    typesupport_name, get_service_type_support_handle, \
    package_name, interface_type, service_name)

/// Declare a typesupport symbol name for a rosidl action definition.
#define ROSIDL_TYPESUPPORT_INTERFACE__ACTION_SYMBOL_NAME( \
    typesupport_name, package_name, interface_type, action_name) \
  ROSIDL_TYPESUPPORT_INTERFACE__SYMBOL_NAME( \
    typesupport_name, get_action_type_support_handle, \
    package_name, interface_type, action_name)

#endif  // ROSIDL_TYPESUPPORT_INTERFACE__MACROS_H_
