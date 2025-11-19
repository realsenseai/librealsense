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

/*! \file find_and_replace.hpp
 * \brief Find and replace functionality for std::strings
 */

#ifndef RCPPUTILS__FIND_AND_REPLACE_HPP_
#define RCPPUTILS__FIND_AND_REPLACE_HPP_

#include <memory>
#include <string>

namespace rcpputils
{

/// Find and replace all instances of a string with another string.
/**
 * \param[in] input The input string.
 * \param[in] find The substring to replace.
 * \param[in] replace The string to substitute for each occurrence of `find`.
 * \return A copy of the input string with all instances of the string `find` replaced with the
 *   string `replace`.
 */
template<
  class CharT,
  class Traits = std::char_traits<CharT>,
  class Allocator = std::allocator<CharT>
>
std::basic_string<CharT, Traits, Allocator>
find_and_replace(
  const std::basic_string<CharT, Traits, Allocator> & input,
  const std::basic_string<CharT, Traits, Allocator> & find,
  const std::basic_string<CharT, Traits, Allocator> & replace)
{
  std::basic_string<CharT, Traits, Allocator> output = input;
  const std::size_t find_len = find.length();
  const std::size_t replace_len = replace.length();
  if (find == replace) {
    return output;
  }
  if (0u == find_len) {
    return output;
  }
  std::size_t pos = output.find(find);
  while (pos != std::basic_string<CharT, Traits, Allocator>::npos) {
    output.replace(pos, find_len, replace);
    pos = output.find(find, pos + replace_len);
  }
  return output;
}

namespace detail
{
template<typename CharT, std::size_t Length>
std::basic_string<CharT>
normalize_to_basic_string(const CharT (& char_string)[Length])
{
  return std::basic_string<CharT>(char_string);
}

template<typename StringLikeT>
StringLikeT &&
normalize_to_basic_string(StringLikeT && string_like)
{
  return string_like;
}
}  // namespace detail

/// Find and replace all instances of a string with another string.
/**
 * \param[in] input The input string.
 * \param[in] find The substring to replace.
 * \param[in] replace The string to substitute for each occurrence of `find`.
 * \return A copy of the input string with all instances of the string `find` replaced with the
 *   string `replace`.
 */
template<typename InputT, typename FindT, typename ReplaceT>
auto
find_and_replace(
  InputT && input,
  FindT && find,
  ReplaceT && replace)
{
  auto input_str = detail::normalize_to_basic_string(input);
  return find_and_replace<typename decltype(input_str)::value_type>(
    input_str,
    detail::normalize_to_basic_string(find),
    detail::normalize_to_basic_string(replace));
}

}  // namespace rcpputils

#endif  // RCPPUTILS__FIND_AND_REPLACE_HPP_
