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

/*! \file join.hpp
 * \brief Join container of objects into single string
 */

#ifndef RCPPUTILS__JOIN_HPP_
#define RCPPUTILS__JOIN_HPP_

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>

namespace rcpputils
{

/// Join values in a container turned into strings by a given delimiter
/**
 * \param[in] container is a collection of values to be turned into string and joined.
 * \param[in] delim is a delimiter to join values turned into strings.
 * \tparam CharT is the string character type.
 * \tparam ValueT is the container value type.
 * \tparam AllocatorT is the container allocator type.
 * \tparam ContainerT is the container template type.
 * \return joined string
 */
template<
  typename CharT, typename ValueT, typename AllocatorT,
  template<typename T, class A> class ContainerT>
std::basic_string<CharT>
join(const ContainerT<ValueT, AllocatorT> & container, const CharT * delim)
{
  std::basic_ostringstream<CharT> s;
  std::copy(container.begin(), container.end(), std::ostream_iterator<ValueT, CharT>(s, delim));
  std::basic_string<CharT> result = s.str();
  if (delim) {
    result = result.substr(0, result.length() - strlen(delim));
  }
  return result;
}

}  // namespace rcpputils

#endif  // RCPPUTILS__JOIN_HPP_
