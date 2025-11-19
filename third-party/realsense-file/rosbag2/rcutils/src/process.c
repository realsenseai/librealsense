// Copyright 2019 Open Source Robotics Foundation, Inc.
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

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined _WIN32 || defined __CYGWIN__
// When building with MSVC 19.28.29333.0 on Windows 10 (as of 2020-11-11),
// there appears to be a problem with winbase.h (which is included by
// Windows.h).  In particular, warnings of the form:
//
// warning C5105: macro expansion producing 'defined' has undefined behavior
//
// See https://developercommunity.visualstudio.com/content/problem/695656/wdk-and-sdk-are-not-compatible-with-experimentalpr.html
// for more information.  For now disable that warning when including windows.h
#pragma warning(push)
#pragma warning(disable : 5105)
#include <Windows.h>
#pragma warning(pop)
#else
#include <libgen.h>
#include <unistd.h>
#endif

#include "rcutils/allocator.h"
#include "rcutils/error_handling.h"
#include "rcutils/process.h"

int rcutils_get_pid(void)
{
#if defined _WIN32 || defined __CYGWIN__
  return (int)GetCurrentProcessId();
#else
  return (int)getpid();
#endif
}

char * rcutils_get_executable_name(rcutils_allocator_t allocator)
{
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    &allocator, "invalid allocator", return NULL);

#if defined __APPLE__ || defined __FreeBSD__ || (defined __ANDROID__ && __ANDROID_API__ >= 21)
  const char * appname = getprogname();
#elif defined __GNUC__ && !defined(__QNXNTO__)
  const char * appname = program_invocation_name;
#elif defined _WIN32 || defined __CYGWIN__
  char appname[MAX_PATH];
  int32_t size = GetModuleFileNameA(NULL, appname, MAX_PATH);
  if (size == 0) {
    return NULL;
  }
#elif defined __QNXNTO__
  extern char * __progname;
  const char * appname = __progname;
#else
#error "Unsupported OS"
#endif

  size_t applen = strlen(appname);

  // Since the above memory may be static, and the caller may want to modify
  // the argument, make and return a copy here.
  char * executable_name = allocator.allocate(applen + 1, allocator.state);
  if (NULL == executable_name) {
    return NULL;
  }

  // Get just the executable name (Unix may return the absolute path)
#if defined __APPLE__ || defined __FreeBSD__ || defined __GNUC__
  // We need an intermediate copy because basename may modify its arguments
  char * intermediate = allocator.allocate(applen + 1, allocator.state);
  if (NULL == intermediate) {
    allocator.deallocate(executable_name, allocator.state);
    return NULL;
  }
  memcpy(intermediate, appname, applen);
  intermediate[applen] = '\0';

  char * bname = basename(intermediate);
  size_t baselen = strlen(bname);
  memcpy(executable_name, bname, baselen);
  executable_name[baselen] = '\0';
  allocator.deallocate(intermediate, allocator.state);
#elif defined _WIN32 || defined __CYGWIN__
  errno_t err = _splitpath_s(appname, NULL, 0, NULL, 0, executable_name, applen, NULL, 0);
  if (err != 0) {
    allocator.deallocate(executable_name, allocator.state);
    return NULL;
  }
#else
#error "Unsupported OS"
#endif

  return executable_name;
}

#ifdef __cplusplus
}
#endif
