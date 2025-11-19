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

#ifndef RCUTILS__FILESYSTEM_H_
#define RCUTILS__FILESYSTEM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>

#include "rcutils/allocator.h"
#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

/// Return current working directory.
/**
 * \param[in] buffer Allocated string to store current directory path to
 * \param[in] max_length maximum length to be stored in buffer
 * \return `True` if success
 * \return `False` if buffer is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
bool
rcutils_get_cwd(char * buffer, size_t max_length);

/// Check if the provided path points to a directory.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if directory
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_is_directory(const char * abs_path);

/// Check if the provided path points to a file.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if file
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_is_file(const char * abs_path);

/// Check if the provided path points to an existing file/folder.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if the path exists
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_exists(const char * abs_path);

/// Check if the provided path points to a file/folder readable by current user.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if the file is readable
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_is_readable(const char * abs_path);

/// Check if the provided path points to a file/folder writable by current user.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if the file is writable
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_is_writable(const char * abs_path);

/// Check if the provided path points to a file/folder both readable and writable by current user.
/**
 * \param[in] abs_path Absolute path to check.
 * \return `True` if the file is redable and writable False otherwise
 * \return `False` if abs_path is NULL
 * \return `False` on failure
 */
RCUTILS_PUBLIC
bool
rcutils_is_readable_and_writable(const char * abs_path);

/// Return newly allocated string with arguments separated by correct delimiter for the platform.
/**
 * This function allocates memory and returns it to the caller.
 * It is up to the caller to release the memory once it is done with it by
 * calling `deallocate` on the same allocator passed here.
 *
 * \param[in] left_hand_path
 * \param[in] right_hand_path
 * \param[in] allocator
 * \return concatenated path on success
 * \return `NULL` on invalid arguments
 * \return `NULL` on failure
 */
RCUTILS_PUBLIC
char *
rcutils_join_path(
  const char * left_hand_path,
  const char * right_hand_path,
  rcutils_allocator_t allocator);

/// Return newly allocated string with all argument's "/" replaced by platform specific separator.
/**
 * This function allocates memory and returns it to the caller.
 * It is up to the caller to release the memory once it is done with it by
 * calling `deallocate` on the same allocator passed here.
 *
 * \param[in] path
 * \param[in] allocator
 * \return path using platform specific delimiters on success
 * \return `NULL` on invalid arguments
 * \return `NULL` on failure
 */
RCUTILS_PUBLIC
char *
rcutils_to_native_path(
  const char * path,
  rcutils_allocator_t allocator);

/// Create the specified directory.
/**
 * This function creates an absolutely-specified directory.
 * If any of the intermediate directories do not exist, this function will
 * return False.
 * If the abs_path already exists, and is a directory, this function will
 * return True.
 *
 * This function is not thread-safe due to mkdir races as described in the
 * openat(2) documentation.
 *
 * \param[in] abs_path
 * \return `True` if making the directory was successful, False otherwise
 * \return `False` if path is NULL
 * \return `False` if path is empty
 * \return `False` if path is not absolute
 * \return `False` if any intermediate directories don't exist
 */
RCUTILS_PUBLIC
bool
rcutils_mkdir(const char * abs_path);

/// Calculate the size of the specified directory.
/*
 * Calculates the size of a directory by summarizing the file size of all files.
 * \note This operation is not recursive.
 * \param[in] directory_path The directory path to calculate the size of.
 * \param[in] allocator Allocator being used for internal file path composition.
 * \return The size of the directory in bytes.
 */
RCUTILS_PUBLIC
size_t
rcutils_calculate_directory_size(const char * directory_path, rcutils_allocator_t allocator);

/// Calculate the size of the specifed file.
/*
 * \param[in] file_path The path of the file to obtain its size of.
 * \return The size of the file in bytes.
 */
RCUTILS_PUBLIC
size_t
rcutils_get_file_size(const char * file_path);

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__FILESYSTEM_H_
