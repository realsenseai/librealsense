// Copyright 2016 Open Source Robotics Foundation, Inc.
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

#ifndef ROSIDL_RUNTIME_CPP__BOUNDED_VECTOR_HPP_
#define ROSIDL_RUNTIME_CPP__BOUNDED_VECTOR_HPP_

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rosidl_runtime_cpp
{

/// A container based on std::vector but with an upper bound.
/**
 * Meets the same requirements as std::vector.
 *
 * \param Tp Type of element
 * \param UpperBound The upper bound for the number of elements
 * \param Alloc Allocator type, defaults to std::allocator<Tp>
 */
template<typename Tp, std::size_t UpperBound, typename Alloc = std::allocator<Tp>>
class BoundedVector
  : protected std::vector<Tp, Alloc>
{
  using Base = std::vector<Tp, Alloc>;

public:
  using typename Base::value_type;
  using typename Base::pointer;
  using typename Base::const_pointer;
  using typename Base::reference;
  using typename Base::const_reference;
  using typename Base::iterator;
  using typename Base::const_iterator;
  using typename Base::const_reverse_iterator;
  using typename Base::reverse_iterator;
  using typename Base::size_type;
  using typename Base::difference_type;
  using typename Base::allocator_type;

  /// Create a %BoundedVector with no elements.
  BoundedVector()
  noexcept (std::is_nothrow_default_constructible<Alloc>::value)
  : Base()
  {}

  /// Creates a %BoundedVector with no elements.
  /**
   * \param a An allocator object
   */
  explicit
  BoundedVector(
    const typename Base::allocator_type & a)
  noexcept
  : Base(a)
  {}

  /// Create a %BoundedVector with default constructed elements.
  /**
   * This constructor fills the %BoundedVector with @a n default
   * constructed elements.
   *
   * \param n The number of elements to initially create
   * \param a An allocator
   */
  explicit
  BoundedVector(
    typename Base::size_type n,
    const typename Base::allocator_type & a = allocator_type())
  : Base(n, a)
  {
    if (n > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
  }

  /// Create a %BoundedVector with copies of an exemplar element.
  /**
   * This constructor fills the %BoundedVector with @a n copies of @a value.
   *
   * \param n The number of elements to initially create
   * \param value An element to copy
   * \param a An allocator
   */
  BoundedVector(
    typename Base::size_type n,
    const typename Base::value_type & value,
    const typename Base::allocator_type & a = allocator_type())
  : Base(n, value, a)
  {
    if (n > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
  }

  /// %BoundedVector copy constructor.
  /**
   * The newly-created %BoundedVector uses a copy of the allocation
   * object used by @a x.
   * All the elements of @a x are copied, but any extra memory in
   * @a x (for fast expansion) will not be copied.
   *
   * \param x A %BoundedVector of identical element and allocator types
   */
  BoundedVector(
    const BoundedVector & x)
  : Base(x)
  {}

  /// %BoundedVector move constructor.
  /**
   * The newly-created %BoundedVector contains the exact contents of @a x.
   * The contents of @a x are a valid, but unspecified %BoundedVector.
   *
   * \param x A %BoundedVector of identical element and allocator types
   */
  BoundedVector(BoundedVector && x) noexcept
  : Base(std::move(x))
  {}

  /// Copy constructor with alternative allocator
  BoundedVector(const BoundedVector & x, const typename Base::allocator_type & a)
  : Base(x, a)
  {}

  /// Build a %BoundedVector from an initializer list.
  /**
   * Create a %BoundedVector consisting of copies of the elements in the
   * initializer_list @a l.
   *
   * This will call the element type's copy constructor N times
   * (where N is @a l.size()) and do no memory reallocation.
   *
   * \param l An initializer_list
   * \param a An allocator
   */
  BoundedVector(
    std::initializer_list<typename Base::value_type> l,
    const typename Base::allocator_type & a = typename Base::allocator_type())
  : Base(l, a)
  {
    if (l.size() > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
  }

  /// Build a %BoundedVector from a range.
  /**
   * Create a %BoundedVector consisting of copies of the elements from
   * [first,last).
   *
   * If the iterators are forward, bidirectional, or random-access, then
   * this will call the elements' copy constructor N times (where N is
   * distance(first,last)) and do no memory reallocation.
   * But if only input iterators are used, then this will do at most 2N
   * calls to the copy constructor, and logN memory reallocations.
   *
   * \param first An input iterator
   * \param last An input iterator
   * \param a An allocator
   */
  template<
    typename InputIterator
  >
  BoundedVector(
    InputIterator first,
    InputIterator last,
    const typename Base::allocator_type & a = allocator_type())
  : Base(first, last, a)
  {
    if (size() > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
  }

  /// The dtor only erases the elements.
  /**
   * Note that if the elements themselves are pointers, the pointed-to
   * memory is not touched in any way.
   * Managing the pointer is the user's responsibility.
   */
  ~BoundedVector() noexcept
  {}

  /// %BoundedVector assignment operator.
  /**
   * All the elements of @a x are copied, but any extra memory in
   * @a x (for fast expansion) will not be copied.
   * Unlike the copy constructor, the allocator object is not copied.
   *
   * \param x A %BoundedVector of identical element and allocator types
   */
  BoundedVector &
  operator=(const BoundedVector & x)
  {
    (void)Base::operator=(x);
    return *this;
  }

  /// %BoundedVector move assignment operator
  /**
   * \param x A %BoundedVector of identical element and allocator types.
   */
  BoundedVector &
  operator=(BoundedVector && x)
  {
    (void)Base::operator=(std::move(x));
    return *this;
  }

  /// %BoundedVector list assignment operator.
  /**
   * This function fills a %BoundedVector with copies of the elements in
   * the initializer list @a l.
   *
   * Note that the assignment completely changes the %BoundedVector and
   * that the resulting %BoundedVector's size is the same as the number
   * of elements assigned.
   * Old data may be lost.
   *
   * \param l An initializer_list
   */
  BoundedVector &
  operator=(std::initializer_list<typename Base::value_type> l)
  {
    if (l.size() > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::operator=(l);
    return *this;
  }

  /// Assign a given value to a %BoundedVector.
  /**
   * This function fills a %BoundedVector with @a n copies of the
   * given value.
   * Note that the assignment completely changes the %BoundedVector and
   * that the resulting %BoundedVector's size is the same as the number
   * of elements assigned.
   * Old data may be lost.
   *
   * \param n Number of elements to be assigned
   * \param val Value to be assigned
   */
  void
  assign(
    typename Base::size_type n,
    const typename Base::value_type & val)
  {
    if (n > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::assign(n, val);
  }

  /// Assign a range to a %BoundedVector.
  /**
   * This function fills a %BoundedVector with copies of the elements in
   * the range [first,last).
   *
   * Note that the assignment completely changes the %BoundedVector and
   * that the resulting %BoundedVector's size is the same as the number
   * of elements assigned.
   * Old data may be lost.
   *
   * \param first An input iterator
   * \param last   An input iterator
   */
  template<
    typename InputIterator
  >
  void
  assign(InputIterator first, InputIterator last)
  {
    using cat = typename std::iterator_traits<InputIterator>::iterator_category;
    do_assign(first, last, cat());
  }

  /// Assign an initializer list to a %BoundedVector.
  /**
   * This function fills a %BoundedVector with copies of the elements in
   * the initializer list @a l.
   *
   * Note that the assignment completely changes the %BoundedVector and
   * that the resulting %BoundedVector's size is the same as the number
   * of elements assigned.
   * Old data may be lost.
   *
   * \param l An initializer_list
   */
  void
  assign(std::initializer_list<typename Base::value_type> l)
  {
    if (l.size() > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::assign(l);
  }

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;
  using Base::cbegin;
  using Base::cend;
  using Base::crbegin;
  using Base::crend;
  using Base::size;

  /** Returns the size() of the largest possible %BoundedVector.  */
  typename Base::size_type
  max_size() const noexcept
  {
    return std::min(UpperBound, Base::max_size());
  }

  /// Resize the %BoundedVector to the specified number of elements.
  /**
   * This function will %resize the %BoundedVector to the specified
   * number of elements.
   * If the number is smaller than the %BoundedVector's current size the
   * %BoundedVector is truncated, otherwise default constructed elements
   * are appended.
   *
   * \param new_size Number of elements the %BoundedVector should contain
   */
  void
  resize(typename Base::size_type new_size)
  {
    if (new_size > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::resize(new_size);
  }

  /// Resize the %BoundedVector to the specified number of elements.
  /**
   * This function will %resize the %BoundedVector to the specified
   * number of elements.
   * If the number is smaller than the %BoundedVector's current size the
   * %BoundedVector is truncated, otherwise the %BoundedVector is
   * extended and new elements are populated with given data.
   *
   * \param new_size Number of elements the %BoundedVector should contain
   * \param x Data with which new elements should be populated
   */
  void
  resize(
    typename Base::size_type new_size,
    const typename Base::value_type & x)
  {
    if (new_size > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::resize(new_size, x);
  }

  using Base::shrink_to_fit;
  using Base::capacity;
  using Base::empty;

  /// Attempt to preallocate enough memory for specified number of elements.
  /**
   * This function attempts to reserve enough memory for the
   * %BoundedVector to hold the specified number of elements.
   * If the number requested is more than max_size(), length_error is
   * thrown.
   *
   * The advantage of this function is that if optimal code is a
   * necessity and the user can determine the number of elements that
   * will be required, the user can reserve the memory in %advance, and
   * thus prevent a possible reallocation of memory and copying of
   * %BoundedVector data.
   *
   * \param n Number of elements required
   * @throw std::length_error If @a n exceeds @c max_size()
   */
  void
  reserve(typename Base::size_type n)
  {
    if (n > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::reserve(n);
  }

  using Base::operator[];
  using Base::at;
  using Base::front;
  using Base::back;

  /// Return a pointer such that [data(), data() + size()) is a valid range.
  /**
   * For a non-empty %BoundedVector, data() == &front().
   */
  template<
    typename T,
    typename std::enable_if<
      !std::is_same<T, Tp>::value &&
      !std::is_same<T, bool>::value
    >::type * = nullptr
  >
  T *
  data() noexcept
  {
    return Base::data();
  }

  template<
    typename T,
    typename std::enable_if<
      !std::is_same<T, Tp>::value &&
      !std::is_same<T, bool>::value
    >::type * = nullptr
  >
  const T *
  data() const noexcept
  {
    return Base::data();
  }

  /// Add data to the end of the %BoundedVector.
  /**
   * This is a typical stack operation.
   * The function creates an element at the end of the %BoundedVector
   * and assigns the given data to it.
   * Due to the nature of a %BoundedVector this operation can be done in
   * constant time if the %BoundedVector has preallocated space
   * available.
   *
   * \param x Data to be added
   */
  void
  push_back(const typename Base::value_type & x)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::push_back(x);
  }

  void
  push_back(typename Base::value_type && x)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::push_back(x);
  }

  /// Add data to the end of the %BoundedVector.
  /**
   * This is a typical stack operation.
   * The function creates an element at the end of the %BoundedVector
   * and assigns the given data to it.
   * Due to the nature of a %BoundedVector this operation can be done in
   * constant time if the %BoundedVector has preallocated space
   * available.
   *
   * \param args Arguments to be forwarded to the constructor of Tp
   */
  template<typename ... Args>
  auto
  emplace_back(Args && ... args)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::emplace_back(std::forward<Args>(args)...);
  }

  /// Insert an object in %BoundedVector before specified iterator.
  /**
   * This function will insert an object of type T constructed with
   * T(std::forward<Args>(args)...) before the specified location.
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position A const_iterator into the %BoundedVector
   * \param args Arguments
   * \return An iterator that points to the inserted data
   */
  template<typename ... Args>
  typename Base::iterator
  emplace(
    typename Base::const_iterator position,
    Args && ... args)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::emplace(position, std::forward<Args>(args) ...);
  }

  /// Insert given value into %BoundedVector before specified iterator.
  /**
   * This function will insert a copy of the given value before the
   * specified location.
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position A const_iterator into the %BoundedVector
   * \param x Data to be inserted
   * \return An iterator that points to the inserted data
   */
  typename Base::iterator
  insert(
    typename Base::const_iterator position,
    const typename Base::value_type & x)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::insert(position, x);
  }

  /// Insert given rvalue into %BoundedVector before specified iterator.
  /**
   * This function will insert a copy of the given rvalue before the
   * specified location.
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position A const_iterator into the %BoundedVector
   * \param x Data to be inserted
   * \return An iterator that points to the inserted data
   */
  typename Base::iterator
  insert(
    typename Base::const_iterator position,
    typename Base::value_type && x)
  {
    if (size() >= UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::insert(position, x);
  }

  /// Insert an initializer_list into the %BoundedVector.
  /**
   * This function will insert copies of the data in the
   * initializer_list @a l into the %BoundedVector before the location
   * specified by @a position.
   *
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position An iterator into the %BoundedVector
   * \param l An initializer_list
   */
  typename Base::iterator
  insert(
    typename Base::const_iterator position,
    std::initializer_list<typename Base::value_type> l)
  {
    if (size() + l.size() > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::insert(position, l);
  }

  /// Insert a number of copies of given data into the %BoundedVector.
  /**
   * This function will insert a specified number of copies of the given
   * data before the location specified by @a position.
   *
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position A const_iterator into the %BoundedVector
   * \param n Number of elements to be inserted
   * \param x Data to be inserted
   * \return An iterator that points to the inserted data
   */
  typename Base::iterator
  insert(
    typename Base::const_iterator position,
    typename Base::size_type n,
    const typename Base::value_type & x)
  {
    if (size() + n > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::insert(position, n, x);
  }

  /// Insert a range into the %BoundedVector.
  /**
   * This function will insert copies of the data in the range
   * [first,last) into the %BoundedVector before the location
   * specified by @a pos.
   *
   * Note that this kind of operation could be expensive for a
   * %BoundedVector and if it is frequently used the user should
   * consider using std::list.
   *
   * \param position A const_iterator into the %BoundedVector
   * \param first An input iterator
   * \param last   An input iterator
   * \return An iterator that points to the inserted data
   */
  template<
    typename InputIterator
  >
  typename Base::iterator
  insert(
    typename Base::const_iterator position,
    InputIterator first,
    InputIterator last)
  {
    using cat = typename std::iterator_traits<InputIterator>::iterator_category;
    return do_insert(position, first, last, cat());
  }

  using Base::erase;
  using Base::pop_back;
  using Base::clear;

private:
  /// Assign elements from an input range.
  template<
    typename InputIterator
  >
  void
  do_assign(InputIterator first, InputIterator last, std::input_iterator_tag)
  {
    BoundedVector(first, last).swap(*this);
  }

  /// Assign elements from a forward range.
  template<
    typename FwdIterator
  >
  void
  do_assign(FwdIterator first, FwdIterator last, std::forward_iterator_tag)
  {
    if (static_cast<std::size_t>(std::distance(first, last)) > UpperBound) {
      throw std::length_error("Exceeded upper bound");
    }
    Base::assign(first, last);
  }

  // Insert each value at the end and then rotate them to the desired position.
  // If the bound is exceeded, the inserted elements are removed again.
  template<
    typename InputIterator
  >
  typename Base::iterator
  do_insert(
    typename Base::const_iterator position,
    InputIterator first,
    InputIterator last,
    std::input_iterator_tag)
  {
    const auto orig_size = size();
    const auto idx = position - cbegin();
    try {
      while (first != last) {
        push_back(*first++);
      }
    } catch (const std::length_error &) {
      Base::resize(orig_size);
      throw;
    }
    auto pos = begin() + idx;
    std::rotate(pos, begin() + orig_size, end());
    return begin() + idx;
  }

  template<
    typename FwdIterator
  >
  typename Base::iterator
  do_insert(
    typename Base::const_iterator position,
    FwdIterator first,
    FwdIterator last,
    std::forward_iterator_tag)
  {
    auto dist = std::distance(first, last);
    if ((dist < 0) || (size() + static_cast<size_t>(dist) > UpperBound)) {
      throw std::length_error("Exceeded upper bound");
    }
    return Base::insert(position, first, last);
  }

  /// Vector equality comparison.
  /**
   * This is an equivalence relation.
   * It is linear in the size of the vectors.
   * Vectors are considered equivalent if their sizes are equal, and if
   * corresponding elements compare equal.
   *
   * \param x A %BoundedVector
   * \param y A %BoundedVector of the same type as @a x
   * \return True if the size and elements of the vectors are equal
  */
  friend bool
  operator==(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) == static_cast<const Base &>(y);
  }

  /// Vector ordering relation.
  /**
   * This is a total ordering relation.
   * It is linear in the size of the vectors.
   * The elements must be comparable with @c <.
   *
   * See std::lexicographical_compare() for how the determination is made.
   *
   * \param x A %BoundedVector
   * \param y A %BoundedVector of the same type as @a x
   * @return True if @a x is lexicographically less than @a y
  */
  friend bool
  operator<(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) < static_cast<const Base &>(y);
  }

  /// Based on operator==
  friend bool
  operator!=(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) != static_cast<const Base &>(y);
  }

  /// Based on operator<
  friend bool
  operator>(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) > static_cast<const Base &>(y);
  }

  /// Based on operator<
  friend bool
  operator<=(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) <= static_cast<const Base &>(y);
  }

  /// Based on operator<
  friend bool
  operator>=(
    const BoundedVector & x,
    const BoundedVector & y)
  {
    return static_cast<const Base &>(x) >= static_cast<const Base &>(y);
  }
};

/// See rosidl_runtime_cpp::BoundedVector::swap().
template<typename Tp, std::size_t UpperBound, typename Alloc>
inline void
swap(BoundedVector<Tp, UpperBound, Alloc> & x, BoundedVector<Tp, UpperBound, Alloc> & y)
{
  x.swap(y);
}

}  // namespace rosidl_runtime_cpp

#endif  // ROSIDL_RUNTIME_CPP__BOUNDED_VECTOR_HPP_
