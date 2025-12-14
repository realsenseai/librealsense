// Copyright 2014-2015 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_TYPESUPPORT_INTROSPECTION_C__FIELD_TYPES_H_
#define ROSIDL_TYPESUPPORT_INTROSPECTION_C__FIELD_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum
{
  rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT = 1,
  rosidl_typesupport_introspection_c__ROS_TYPE_DOUBLE = 2,
  rosidl_typesupport_introspection_c__ROS_TYPE_LONG_DOUBLE = 3,
  rosidl_typesupport_introspection_c__ROS_TYPE_CHAR = 4,
  rosidl_typesupport_introspection_c__ROS_TYPE_WCHAR = 5,
  rosidl_typesupport_introspection_c__ROS_TYPE_BOOLEAN = 6,
  rosidl_typesupport_introspection_c__ROS_TYPE_OCTET = 7,
  rosidl_typesupport_introspection_c__ROS_TYPE_UINT8 = 8,
  rosidl_typesupport_introspection_c__ROS_TYPE_INT8 = 9,
  rosidl_typesupport_introspection_c__ROS_TYPE_UINT16 = 10,
  rosidl_typesupport_introspection_c__ROS_TYPE_INT16 = 11,
  rosidl_typesupport_introspection_c__ROS_TYPE_UINT32 = 12,
  rosidl_typesupport_introspection_c__ROS_TYPE_INT32 = 13,
  rosidl_typesupport_introspection_c__ROS_TYPE_UINT64 = 14,
  rosidl_typesupport_introspection_c__ROS_TYPE_INT64 = 15,
  rosidl_typesupport_introspection_c__ROS_TYPE_STRING = 16,
  rosidl_typesupport_introspection_c__ROS_TYPE_WSTRING = 17,

  rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE = 18,

  // for backward compatibility only
  rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT32 = 1,
  rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT64 = 2,
  rosidl_typesupport_introspection_c__ROS_TYPE_BOOL = 6,
  rosidl_typesupport_introspection_c__ROS_TYPE_BYTE = 7
};

#ifdef __cplusplus
}
#endif

#endif  // ROSIDL_TYPESUPPORT_INTROSPECTION_C__FIELD_TYPES_H_
