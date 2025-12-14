// Copyright 2015-2020 Open Source Robotics Foundation, Inc.
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

#ifndef RCPPUTILS__SCOPE_EXIT_HPP_
#define RCPPUTILS__SCOPE_EXIT_HPP_

#include <utility>

#include "rcutils/macros.h"

namespace rcpputils
{

template<typename CallableT>
struct scope_exit final
{
  explicit scope_exit(CallableT && callable)
  : callable_(std::forward<CallableT>(callable))
  {
  }

  scope_exit(const scope_exit &) = delete;
  scope_exit(scope_exit &&) = default;

  scope_exit & operator=(const scope_exit &) = delete;
  scope_exit & operator=(scope_exit &&) = default;

  ~scope_exit()
  {
    if (!cancelled_) {
      callable_();
    }
  }

  void cancel()
  {
    cancelled_ = true;
  }

private:
  CallableT callable_;
  bool cancelled_{false};
};

template<typename CallableT>
scope_exit<CallableT>
make_scope_exit(CallableT && callable)
{
  return scope_exit<CallableT>(std::forward<CallableT>(callable));
}

}  // namespace rcpputils

#define RCPPUTILS_SCOPE_EXIT(code) \
  auto RCUTILS_JOIN(scope_exit_, __LINE__) = rcpputils::make_scope_exit([&]() {code;})

#endif  // RCPPUTILS__SCOPE_EXIT_HPP_
