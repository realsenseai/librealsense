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

/*! \file pointer_traits.hpp
 * \brief Additional type traits support for smart pointers
 */

#ifndef RCPPUTILS__POINTER_TRAITS_HPP_
#define RCPPUTILS__POINTER_TRAITS_HPP_

#include <memory>
#include <type_traits>

namespace rcpputils
{

namespace details
{

template<class T>
struct is_smart_pointer_helper : std::false_type
{};

template<class T>
struct is_smart_pointer_helper<std::shared_ptr<T>>: std::true_type
{};

template<class T>
struct is_smart_pointer_helper<std::unique_ptr<T>>: std::true_type
{};

template<class T>
struct is_smart_pointer : is_smart_pointer_helper<typename std::remove_cv<T>::type>
{};

template<
  class T,
  bool is_smart_pointer
>
struct remove_pointer
{
  using type = typename std::remove_pointer<T>::type;
};

template<class T>
struct remove_pointer<T, true>
{
  using type = typename std::remove_pointer<
    decltype(std::declval<typename std::remove_volatile<T>::type>().get())>::type;
};
}  // namespace details

/// Type traits for validating if T is of type pointer or smart pointer
/**
 * In comparison to the existing type trait for pointer in the stdlib `std::is_pointer<T>`
 * https://en.cppreference.com/w/cpp/types/is_pointer this trait is enhancing it for
 * checking of smart pointer types as well.
 * The valid pointer types are T*, std::shared_pointer<T> and std::unique_ptr<T>
 *
 * Potential use cases are for static assert when passing a template parameter requiring this
 * parameter to be of type pointer without specifying which type of pointer (raw, smart).
 *
 * ```
 * class MyType
 * {
 *   template<class T>
 *   MyType(T && arg)
 *   {
 *     static_assert(rcpputils::is_pointer<decltype(arg)>::value, "arg has to be a pointer");
 *
 *     arg->do_stuff();  // with the assert above, this call is guaranteed to work.
 *   }
 * };
 * ```
 */
template<class T>
struct is_pointer
{
  /// Indicates whether this object is a pointer or smart pointer.
  static constexpr bool value = std::is_pointer<typename std::remove_reference<T>::type>::value ||
    details::is_smart_pointer<typename std::remove_reference<T>::type>::value;
};

/// Type traits for deducing the data type of T from a pointer or smart pointer.
/**
 * In comparison to the existing type trait for pointer in the stdlib `std::remove_pointer<T>`
 * https://en.cppreference.com/w/cpp/types/remove_pointer this trait is enhancing it for
 * checking of smart pointer types as well.
 * The valid pointer types are T*, std::shared_pointer<T> and std::unique_ptr<T>
 *
 */
template<class T>
struct remove_pointer
{
  using type = typename details::remove_pointer<
    typename std::remove_reference<T>::type, details::is_smart_pointer<T>::value>::type;
};

}  // namespace rcpputils

#endif  // RCPPUTILS__POINTER_TRAITS_HPP_
