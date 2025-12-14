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

#ifdef __cplusplus
extern "C"
{
#endif
#include "rcutils/filesystem.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#else
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
#include <windows.h>
#pragma warning(pop)
#include <direct.h>
#endif  // _WIN32

#include "rcutils/format_string.h"
#include "rcutils/repl_str.h"

#ifdef _WIN32
# define RCUTILS_PATH_DELIMITER "\\"
#else
# define RCUTILS_PATH_DELIMITER "/"
#endif  // _WIN32

bool
rcutils_get_cwd(char * buffer, size_t max_length)
{
  if (NULL == buffer || max_length == 0) {
    return false;
  }
#ifdef _WIN32
  if (NULL == _getcwd(buffer, (int)max_length)) {
    return false;
  }
#else
  if (NULL == getcwd(buffer, max_length)) {
    return false;
  }
#endif  // _WIN32
  return true;
}

bool
rcutils_is_directory(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
#ifdef _WIN32
  return (buf.st_mode & S_IFDIR) == S_IFDIR;
#else
  return S_ISDIR(buf.st_mode);
#endif  // _WIN32
}

bool
rcutils_is_file(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
#ifdef _WIN32
  return (buf.st_mode & S_IFREG) == S_IFREG;
#else
  return S_ISREG(buf.st_mode);
#endif  // _WIN32
}

bool
rcutils_exists(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
  return true;
}

bool
rcutils_is_readable(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
#ifdef _WIN32
  if (!(buf.st_mode & _S_IREAD)) {
#else
  if (!(buf.st_mode & S_IRUSR)) {
#endif  // _WIN32
    return false;
  }
  return true;
}

bool
rcutils_is_writable(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
#ifdef _WIN32
  if (!(buf.st_mode & _S_IWRITE)) {
#else
  if (!(buf.st_mode & S_IWUSR)) {
#endif  // _WIN32
    return false;
  }
  return true;
}

bool
rcutils_is_readable_and_writable(const char * abs_path)
{
  struct stat buf;
  if (stat(abs_path, &buf) < 0) {
    return false;
  }
#ifdef _WIN32
  // NOTE(marguedas) on windows all writable files are readable
  // hence the following check is equivalent to "& _S_IWRITE"
  if (!((buf.st_mode & _S_IWRITE) && (buf.st_mode & _S_IREAD))) {
#else
  if (!((buf.st_mode & S_IWUSR) && (buf.st_mode & S_IRUSR))) {
#endif  // _WIN32
    return false;
  }
  return true;
}

char *
rcutils_join_path(
  const char * left_hand_path,
  const char * right_hand_path,
  rcutils_allocator_t allocator)
{
  if (NULL == left_hand_path) {
    return NULL;
  }
  if (NULL == right_hand_path) {
    return NULL;
  }

  return rcutils_format_string(
    allocator,
    "%s%s%s",
    left_hand_path, RCUTILS_PATH_DELIMITER, right_hand_path);
}

char *
rcutils_to_native_path(
  const char * path,
  rcutils_allocator_t allocator)
{
  if (NULL == path) {
    return NULL;
  }

  return rcutils_repl_str(path, "/", RCUTILS_PATH_DELIMITER, &allocator);
}

bool
rcutils_mkdir(const char * abs_path)
{
  if (NULL == abs_path) {
    return false;
  }

  if (abs_path[0] == '\0') {
    return false;
  }

  bool success = false;
#ifdef _WIN32
  // TODO(clalancette): Check to ensure that the path is absolute on Windows.
  // In theory we can use PathRelativeA to do this, but I was unable to make
  // it work.  Needs further investigation.

  int ret = _mkdir(abs_path);
#else
  if (abs_path[0] != '/') {
    return false;
  }

  int ret = mkdir(abs_path, 0775);
#endif
  if (ret == 0 || (errno == EEXIST && rcutils_is_directory(abs_path))) {
    success = true;
  }

  return success;
}

size_t
rcutils_calculate_directory_size(const char * directory_path, rcutils_allocator_t allocator)
{
  size_t dir_size = 0;

  if (!rcutils_is_directory(directory_path)) {
    fprintf(stderr, "Path is not a directory: %s\n", directory_path);
    return dir_size;
  }
#ifdef _WIN32
  char * path = rcutils_join_path(directory_path, "*", allocator);
  WIN32_FIND_DATA data;
  HANDLE handle = FindFirstFile(path, &data);
  if (INVALID_HANDLE_VALUE == handle) {
    fprintf(stderr, "Can't open directory %s. Error code: %lu\n", directory_path, GetLastError());
    return dir_size;
  }
  allocator.deallocate(path, allocator.state);
  if (handle != INVALID_HANDLE_VALUE) {
    do {
      // Skip over local folder handle (`.`) and parent folder (`..`)
      if (strcmp(data.cFileName, ".") != 0 && strcmp(data.cFileName, "..") != 0) {
        char * file_path = rcutils_join_path(directory_path, data.cFileName, allocator);
        dir_size += rcutils_get_file_size(file_path);
        allocator.deallocate(file_path, allocator.state);
      }
    } while (FindNextFile(handle, &data));
    FindClose(handle);
  }
  return dir_size;
#else
  DIR * dir = opendir(directory_path);
  if (NULL == dir) {
    fprintf(stderr, "Can't open directory %s. Error code: %d\n", directory_path, errno);
    return dir_size;
  }
  struct dirent * entry;
  while (NULL != (entry = readdir(dir))) {
    // Skip over local folder handle (`.`) and parent folder (`..`)
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      char * file_path = rcutils_join_path(directory_path, entry->d_name, allocator);
      dir_size += rcutils_get_file_size(file_path);
      allocator.deallocate(file_path, allocator.state);
    }
  }
  closedir(dir);
  return dir_size;
#endif
}

size_t
rcutils_get_file_size(const char * file_path)
{
  if (!rcutils_is_file(file_path)) {
    fprintf(stderr, "Path is not a file: %s\n", file_path);
    return 0;
  }

  struct stat stat_buffer;
  int rc = stat(file_path, &stat_buffer);
  return rc == 0 ? (size_t)(stat_buffer.st_size) : 0;
}

#ifdef __cplusplus
}
#endif
