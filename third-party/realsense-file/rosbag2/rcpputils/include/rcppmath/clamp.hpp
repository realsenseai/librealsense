// Copyright 2020 PAL Robotics S.L.
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

/*! \file clamp.hpp
 * \brief Restrict a value between two bounds
 */

#ifndef RCPPMATH__CLAMP_HPP_
#define RCPPMATH__CLAMP_HPP_

#include <cassert>

namespace rcppmath
{
/// If v compares less than lo, returns lo; otherwise if hi compares less
//  than v, returns hi; otherwise returns v. Uses operator< to compare the values
/**
 * \param[in] v the value to clamp
 * \param[in] lo the lower boundary
 * \param[in] hi the higher boundary
 * \return Reference to lo if v is less than lo, reference to hi if hi is less than v, otherwise
 * reference to v.
 * \note Implementation from https://en.cppreference.com/w/cpp/algorithm/clamp
 * \warning Capturing the result of clamp by reference if one of the parameters is rvalue produces
 *  a dangling reference if that parameter is returned
 **/
template<class T>
constexpr const T & clamp(const T & v, const T & lo, const T & hi)
{
  assert(!(hi < lo) );
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

/// Like the function above, but uses comp to compare the values.
/**
 * \param[in] comp Comparison object that returns true if the first argument is
 * less than the second
 **/
template<class T, class Compare>
constexpr const T & clamp(const T & v, const T & lo, const T & hi, Compare comp)
{
  assert(!comp(hi, lo) );
  return comp(v, lo) ? lo : comp(hi, v) ? hi : v;
}

}  // namespace rcppmath

#endif  // RCPPMATH__CLAMP_HPP_
