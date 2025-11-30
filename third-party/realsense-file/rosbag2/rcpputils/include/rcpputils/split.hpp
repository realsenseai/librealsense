// Copyright (c) 2019, Open Source Robotics Foundation, Inc.
// All rights reserved.
//
// Software License Agreement (BSD License 2.0)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//  * Neither the name of the copyright holders nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This file is originally from:
// https://github.com/ros/pluginlib/blob/1a4de29fa55173e9b897ca8ff57ebc88c047e0b3/pluginlib/include/pluginlib/impl/split.hpp

/*! \file split.hpp
 * \brief Split string by provided delimiter.
 */

#ifndef RCPPUTILS__SPLIT_HPP_
#define RCPPUTILS__SPLIT_HPP_

#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace rcpputils
{

/// Split a specified input into tokens using a delimiter and a type erased insert iterator.
/**
 * The returned vector will contain the tokens split from the input
 *
 * \param[in] input the input string to be split
 * \param[in] delim the delimiter used to split the input string
 * \param[in] it iterator pointing to a storage container
 * \param[in] skip_empty remove empty strings from the return vector
 */
template<
  class InsertIterator,
  typename std::enable_if<
    std::is_same<
      InsertIterator &,
      decltype(std::declval<InsertIterator>().operator=(std::declval<std::string>()))>::value
  >::type * = nullptr>
void
split(const std::string & input, char delim, InsertIterator & it, bool skip_empty = false)
{
  std::stringstream ss;
  ss.str(input);
  std::string item;
  while (std::getline(ss, item, delim)) {
    if (skip_empty && item == "") {
      continue;
    }
    it = item;
  }
}

/// Split a specified input into tokens using a delimiter.
/**
 * The returned vector will contain the tokens split from the input
 *
 * \param[in] input the input string to be split
 * \param[in] delim the delimiter used to split the input string
 * \param[in] skip_empty remove empty strings from the return vector
 * \return A vector of tokens.
 */
inline std::vector<std::string>
split(const std::string & input, char delim, bool skip_empty = false)
{
  std::vector<std::string> result;
  auto it = std::back_inserter(result);
  split(input, delim, it, skip_empty);
  return result;
}
}  // namespace rcpputils

#endif  // RCPPUTILS__SPLIT_HPP_
