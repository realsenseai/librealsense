// Copyright 2015 Open Source Robotics Foundation, Inc.
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

#ifndef RCUTILS__VISIBILITY_CONTROL_H_
#define RCUTILS__VISIBILITY_CONTROL_H_

#include "rcutils/visibility_control_macros.h"

#ifdef RCUTILS_BUILDING_DLL
# define RCUTILS_PUBLIC RCUTILS_EXPORT
#else
# define RCUTILS_PUBLIC RCUTILS_IMPORT
#endif  // !RCUTILS_BUILDING_DLL

#if defined(_MSC_VER) || defined(__CYGWIN__)
# define RCUTILS_PUBLIC_TYPE RCUTILS_PUBLIC
#else  // defined(_MSC_VER) || defined(__CYGWIN__)
# define RCUTILS_PUBLIC_TYPE
#endif  // !defined(_MSC_VER) && !defined(__CYGWIN__)

#endif  // RCUTILS__VISIBILITY_CONTROL_H_
