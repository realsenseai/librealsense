// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "rcpputils/asserts.hpp"

namespace rcpputils
{
AssertionException::AssertionException(const char * msg)
{
  msg_ = msg;
}

const char * AssertionException::what() const throw()
{
  return msg_.c_str();
}

IllegalStateException::IllegalStateException(const char * msg)
{
  msg_ = msg;
}

const char * IllegalStateException::what() const throw()
{
  return msg_.c_str();
}
}  // namespace rcpputils
