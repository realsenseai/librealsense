// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef RCPPUTILS__THREAD_SAFETY_ANNOTATIONS_HPP_
#define RCPPUTILS__THREAD_SAFETY_ANNOTATIONS_HPP_

#include <mutex>

/*! \file thread_safety_annotations.hpp
* \brief Enable thread safety attributes only with clang+libcxx.
*
* Technically they would work with clang without libcxx, on manually-annotated thread safety
* primitives, but this use case causes the error of annotating against non-annotated libstdc++
* types. Users that wish to annotate their threading library will need to define these macros
* separately for that case.
*
* The attributes can be safely erased when compiling with other compilers.
*
*
* Macro descriptions were obtained from llvm's thread safety annotation documentation, see
* [their documentation](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html) for more info
*/

// Prefixing all macros to avoid potential conflict with other projects.
#if defined(__clang__) && defined(_LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS) && (!defined(SWIG))
#define RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

/**
  * \brief Defined for negation functionality
  *
  * libcxx does not define this operator, needed for negative capabilities
  * TODO Here until someone has a better idea
  */
inline const std::mutex & operator!(const std::mutex & a)
{
  return a;
}

/**
  * \def RCPPUTILS_TSA_CAPABILITY
  * \brief Attribute on classes, which specifies that objects of the class can be used as a
  * capability.
  *
  * The string argument specifies the kind of capability in error
  * messages, e.g. "mutex". See the Container example given above, or the Mutex class in mutex.h at:
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#capability-string
  */
#define RCPPUTILS_TSA_CAPABILITY(x) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

/**
  * \def RCPPUTILS_TSA_SCOPED_CAPABILITY
  * \brief Attribute on classes that implement RAII-style locking, in which a capability is acquired
  * in the constructor, and released in the destructor
  *
  * Such classes require special handling because the constructor and destructor refer to the
  * capability via different names; see the MutexLocker class in mutex.h, at:
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#scoped-capability
  */
#define RCPPUTILS_TSA_SCOPED_CAPABILITY \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

/**
  * \def RCPPUTILS_TSA_GUARDED_BY
  * \brief Attribute on data members, which declares that the data member is protected by the given
  * capability.
  *
  * Read operations on the data require shared access, while write operations require exclusive
  * access. https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#guarded-by-c-and-pt-guarded-by-c
  */
#define RCPPUTILS_TSA_GUARDED_BY(x) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

/**
  * \def RCPPUTILS_TSA_PT_GUARDED_BY
  * \brief Similar to RCPPUTILS_TSA_GUARDED_BY, but is intended for use on pointers and smart
  * pointers.
  *
  * There is no constraint on the data member itself, but the data that it points to is protected by
  * the given capability.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#guarded-by-c-and-pt-guarded-by-c
  */
#define RCPPUTILS_TSA_PT_GUARDED_BY(x) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

/**
  * \def RCPPUTILS_TSA_ACQUIRED_BEFORE
  * \brief Attribute on member declarations, specifically declarations of mutexes or other
  * capabilities.
  *
  * These declarations enforce a particular order in which the mutexes must be acquired, in order to
  * prevent deadlock.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquired-before-acquired-after
  */
#define RCPPUTILS_TSA_ACQUIRED_BEFORE(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_ACQUIRED_AFTER
  * \brief Attribute on member declarations, specifically declarations of mutexes or other
  * capabilities.
  *
  * These declarations enforce a particular order in which the mutexes must be acquired, in order
  * to prevent deadlock.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquired-before-acquired-after
  */
#define RCPPUTILS_TSA_ACQUIRED_AFTER(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_REQUIRES
  * \brief Attribute on functions or methods, which declares that the calling thread must have
  * exclusive access to the given capabilities.
  *
  * More than one capability may be specified. The capabilities must be held on entry to the
  * function, and must still be held on exit.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#requires-requires-shared
  */
#define RCPPUTILS_TSA_REQUIRES(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_REQUIRES_SHARED
  * \brief Attribute on functions or methods, which declares that the calling thread must have
  * shared access to the given capabilities.
  *
  * More than one capability may be specified. The capabilities must be held on entry to the
  * function, and must still be held on exit.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#requires-requires-shared
  */
#define RCPPUTILS_TSA_REQUIRES_SHARED(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_ACQUIRE
  * \brief Attribute on functions or methods, which declares that the function acquires a
  * capability, but does not release it.
  *
  * The caller must not hold the given capability on entry, and it will hold the capability on exit.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquire-acquire-shared-release-release-shared
  */
#define RCPPUTILS_TSA_ACQUIRE(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_ACQUIRE_SHARED
  * \brief Attribute on functions or methods, which declares that the function acquires a shared
  * capability, but does not release it.
  *
  * The caller must not hold the given capability on entry, and it will hold the capability on exit.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquire-acquire-shared-release-release-shared
  */
#define RCPPUTILS_TSA_ACQUIRE_SHARED(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_RELEASE
  * \brief Declare that the function releases the given capability.
  *
  * The caller must hold the capability on entry, and will no longer hold it on exit. It does not
  * matter whether the given capability is shared or exclusive.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquire-acquire-shared-release-release-shared
  */
#define RCPPUTILS_TSA_RELEASE(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_RELEASE_SHARED
  * \brief Declare that the function releases the given shared capability.
  *
  * The caller must hold the shared capability on entry, and will no longer hold it on exit. It does
  * not matter whether the given capability is shared or exclusive.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#acquire-acquire-shared-release-release-shared
  */
#define RCPPUTILS_TSA_RELEASE_SHARED(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_TRY_ACQUIRE
  * \brief Attempts to RCPPUTILS_TSA_ACQUIRE, but its return value will indicate whether it
  * succeeded or failed.
  *
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#try-acquire-bool-try-acquire-shared-bool
  */
#define RCPPUTILS_TSA_TRY_ACQUIRE(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_TRY_ACQUIRE_SHARED
  * \brief Attempts to RCPPUTILS_TSA_ACQUIRE_SHARED, but its return value will indicate whether it
  * succeeded or failed.
  *
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#try-acquire-bool-try-acquire-shared-bool
  */
#define RCPPUTILS_TSA_TRY_ACQUIRE_SHARED(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_EXCLUDES
  * \brief attribute prevents deadlock, by making sure that a mutex is not held.
  *
  * However, EXCLUDES is an optional attribute, and does not provide the same safety guarantee as
  * RCPPUTILS_TSA_REQUIRES. In particular:
  *   - A function which acquires a capability does not have to exclude it.
  *   - A function which calls a function that excludes a capability does not have transitively
  *   - exclude that capability.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#negative-capabilities
  */
#define RCPPUTILS_TSA_EXCLUDES(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_ASSERT_CAPABILITY
  * \brief Perform a run-time test to see whether the calling thread holds the given capability.
  *
  * The function is assumed to fail (no return) if the capability is not held
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#assert-capability-and-assert-shared-capability
  */
#define RCPPUTILS_TSA_ASSERT_CAPABILITY(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_ASSERT_SHARED_CAPABILITY
  * \brief Perform a run-time test to see whether the calling thread holds the given shared
  * capability.
  *
  * The function is assumed to fail (no return) if the capability is not held.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#assert-capability-and-assert-shared-capability
  */
#define RCPPUTILS_TSA_ASSERT_SHARED_CAPABILITY(...) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(__VA_ARGS__))

/**
  * \def RCPPUTILS_TSA_RETURN_CAPABILITY
  * \brief Attribute on functions or methods, which declares that the function returns a reference
  * to the given capability.
  *
  * It is used to annotate getter methods that return mutexes.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#return-capability-c
  */
#define RCPPUTILS_TSA_RETURN_CAPABILITY(x) \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

/**
  * \def RCPPUTILS_TSA_NO_THREAD_SAFETY_ANALYSIS
  * \brief Attribute on functions or methods, which turns off thread safety checking for that
  * method.
  *
  * RCPPUTILS_TSA_NO_THREAD_SAFETY_ANALYSIS provides an escape hatch for functions which are
  * either (1) deliberately thread-unsafe, or (2) are thread-safe, but too complicated for the
  * analysis to understand. Reasons for (2) will be described in the Known Limitations, below.
  * https://clang.llvm.org/docs/ThreadSafetyAnalysis.html#return-capability-c
  */
#define RCPPUTILS_TSA_NO_THREAD_SAFETY_ANALYSIS \
  RCPPUTILS_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#endif  // RCPPUTILS__THREAD_SAFETY_ANNOTATIONS_HPP_
