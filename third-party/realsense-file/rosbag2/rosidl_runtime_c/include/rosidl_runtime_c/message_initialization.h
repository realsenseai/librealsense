// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_RUNTIME_C__MESSAGE_INITIALIZATION_H_
#define ROSIDL_RUNTIME_C__MESSAGE_INITIALIZATION_H_

enum rosidl_runtime_c__message_initialization
{
  // Initialize all fields of the message, either with the default value
  // (if the field has one), or with an empty value (generally 0 or an
  // empty string).
  ROSIDL_RUNTIME_C_MSG_INIT_ALL,
  // Skip initialization of all fields of the message.  It is up to the user to
  // ensure that all fields are initialized before use.
  ROSIDL_RUNTIME_C_MSG_INIT_SKIP,
  // Initialize all fields of the message to an empty value (generally 0 or an
  // empty string).
  ROSIDL_RUNTIME_C_MSG_INIT_ZERO,
  // Initialize all fields of the message that have defaults; all other fields
  // are left untouched.
  ROSIDL_RUNTIME_C_MSG_INIT_DEFAULTS_ONLY,
};

#endif  // ROSIDL_RUNTIME_C__MESSAGE_INITIALIZATION_H_
