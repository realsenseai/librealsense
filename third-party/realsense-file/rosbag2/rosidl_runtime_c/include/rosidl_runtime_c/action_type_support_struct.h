// Copyright 2018 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_RUNTIME_C__ACTION_TYPE_SUPPORT_STRUCT_H_
#define ROSIDL_RUNTIME_C__ACTION_TYPE_SUPPORT_STRUCT_H_

#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_runtime_c/service_type_support_struct.h"
#include "rosidl_runtime_c/visibility_control.h"
#include "rosidl_typesupport_interface/macros.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct rosidl_action_type_support_t rosidl_action_type_support_t;

/// Contains rosidl action type support data.
/*
 * Actions are built based on services(goal, result and cancel) and message (feedback and status).
 */
struct rosidl_action_type_support_t
{
  const rosidl_service_type_support_t * goal_service_type_support;
  const rosidl_service_type_support_t * result_service_type_support;
  const rosidl_service_type_support_t * cancel_service_type_support;
  const rosidl_message_type_support_t * feedback_message_type_support;
  const rosidl_message_type_support_t * status_message_type_support;
};

/// Get the action type support given a provided action and package.
/*
 * \param PkgName name of the package that contains the action
 * \param Name action name
 * \return a rosidl_action_type_support_t struct if found, otherwise NULL.
 */
#define ROSIDL_GET_ACTION_TYPE_SUPPORT(PkgName, Name) \
  ROSIDL_TYPESUPPORT_INTERFACE__ACTION_SYMBOL_NAME( \
    rosidl_typesupport_c, PkgName, action, Name)()

#ifdef __cplusplus
}
#endif

#endif  // ROSIDL_RUNTIME_C__ACTION_TYPE_SUPPORT_STRUCT_H_
