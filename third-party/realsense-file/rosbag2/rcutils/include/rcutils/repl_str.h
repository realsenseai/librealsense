// Copyright 2015-2016 Laird Shaw
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

// This function is based on the repl_str() from (on 2017-04-25):
//
//   http://creativeandcritical.net/str-replace-c
//
// It is released under the Public Domain, and has been placed additionally
// under the Apache 2.0 license by me (William Woodall).
//
// It has been modified to take a custom allocator and to fit some of our
// style standards.

#ifndef RCUTILS__REPL_STR_H_
#define RCUTILS__REPL_STR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "rcutils/allocator.h"
#include "rcutils/macros.h"
#include "rcutils/visibility_control.h"

/// Replace all the occurrences of one string for another in the given string.
/**
 * Documentation copied from the source with minor tweaks:
 *
 * ----
 *
 * **Description:**
 *
 * Replaces in the string `str` all the occurrences of the source string `from`
 * with the destination string `to`.
 * The lengths of the strings `from` and `to` may differ.
 * The string `to` may be of any length, but the string `from` must be of
 * non-zero length - the penalty for providing an empty string for the `from`
 * parameter is an infinite loop.
 * In addition, none of the three parameters may be NULL.
 *
 * **Returns:**
 *
 * The post-replacement string, or NULL if memory for the new string could not
 * be allocated.
 * Does not modify the original string.
 * The memory for the returned post-replacement string may be deallocated with
 * given allocator's deallocate function when it is no longer required.
 *
 * **Performance:**
 *
 * In case you are curious enough to want to compare this implementation with
 * alternative implementations, you are welcome to compile and run the code in
 * the benchmarking file,
 * [replacebench.c](http://creativeandcritical.net/downloads/replacebench.c).
 * In that file, the above function is included as `replace_cache`, and the
 * functions originally published on this page as `replace_str2` and
 * `replace_str` are included as `replace_opt2` and `replace_opt`.
 * Code/functions are included from the comp.lang.c thread,
 * [how to replace a substring in a string using C?]
 * (https://groups.google.com/forum/#!msg/comp.lang.c/sgydS2lDgxc/v2MRxRrAQncJ),
 * from answers to the stackoverflow question,
 * [What is the function to replace string in C?]
 * (http://stackoverflow.com/questions/779875/what-is-the-function-to-replace-string-in-c),
 * and also from private correspondence.
 * See the comments top of file for instructions on compiling and running it.
 *
 * In most scenarios, the fastest replacement function, by around 10-20%,
 * is Albert Chan's `replace_smart`, followed by the above function:
 * `repl_str` aka `replace_cache`.
 * There are some scenarios, though, where `repl_str` is faster than
 * `replace_smart`, sometimes by up to 200%.
 * These scenarios involve long strings with few matches.
 * Why, if Albert's function is generally slightly faster than the above
 * `repl_str` function, is it not the focus of this page?
 * Because it generally uses much more memory than `repl_str`.
 *
 * The third fastest implementation is typically `replace_str2` aka
 * `replace_opt2`.
 * For longer strings in the case in which the lengths of the "from" and "to"
 * strings differ, `repl_str` aka `replace_cache` beats it by margins of up to
 * about 80%.
 * For smaller strings, and in the case where the lengths of the "from" and
 * "to" strings are identical, `replace_str2` aka `replace_opt2` is faster,
 * by a maximum margin of about 35%, sometimes in those scenarios beating
 * `replace_smart` too.
 * Some of the other functions are also faster for smaller strings.
 * The even-match point between `replace_str2` and `repl_str`
 * (assuming "from" and "to" string lengths differ) depends on how far into
 * the string the final match occurs, how many matches there are, and the
 * comparative lengths of the old and "to" strings, but roughly it occurs for
 * strings of 700-800 bytes in length.
 *
 * This analysis is based on compiling with [GCC](https://gcc.gnu.org/) and
 * testing on a 64-bit Intel platform running Linux, however brief testing with
 * [Microsoft Visual C++ 2010 Express]
 * (https://www.visualstudio.com/en-US/products/visual-studio-express-vs)
 * (scroll down to "Additional information" at that link) on Windows 7 seemed
 * to produce similar results.
 *
 * ----
 *
 * Here continues additional documentation added by OSRF.
 *
 * The allocator must not be NULL.
 *
 * \param[in] str string to have substrings found and replaced within
 * \param[in] from string to match for replacement
 * \param[in] to string to replace matched strings with
 * \param[in] allocator structure defining functions to be used for allocation
 * \return duplicated `str` with all matches of `from` replaced with `to`
 */
RCUTILS_PUBLIC
RCUTILS_WARN_UNUSED
char *
rcutils_repl_str(
  const char * str,
  const char * from,
  const char * to,
  const rcutils_allocator_t * allocator);

// Implementation copied from above mentioned source continues in repl_str.c.

#ifdef __cplusplus
}
#endif

#endif  // RCUTILS__REPL_STR_H_
