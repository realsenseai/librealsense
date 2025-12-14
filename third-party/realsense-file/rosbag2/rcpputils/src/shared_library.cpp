// Copyright 2020 Open Source Robotics Foundation, Inc.
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

#include <iostream>
#include <string>

#include "rcutils/error_handling.h"

#include "rcpputils/shared_library.hpp"

namespace rcpputils
{
SharedLibrary::SharedLibrary(const std::string & library_path)
{
  lib = rcutils_get_zero_initialized_shared_library();
  rcutils_ret_t ret = rcutils_load_shared_library(
    &lib,
    library_path.c_str(),
    rcutils_get_default_allocator());
  if (ret != RCUTILS_RET_OK) {
    if (ret == RCUTILS_RET_BAD_ALLOC) {
      throw std::bad_alloc();
    } else {
      std::string rcutils_error_str(rcutils_get_error_string().str);
      rcutils_reset_error();
      throw std::runtime_error{rcutils_error_str};
    }
  }
}

SharedLibrary::~SharedLibrary()
{
  if (rcutils_is_shared_library_loaded(&lib)) {
    rcutils_ret_t ret = rcutils_unload_shared_library(&lib);
    if (ret != RCUTILS_RET_OK) {
      std::cerr << rcutils_get_error_string().str << std::endl;
      rcutils_reset_error();
    }
  }
}

void SharedLibrary::unload_library()
{
  rcutils_ret_t ret = rcutils_unload_shared_library(&lib);
  if (ret != RCUTILS_RET_OK) {
    std::string rcutils_error_str(rcutils_get_error_string().str);
    rcutils_reset_error();
    throw std::runtime_error{rcutils_error_str};
  }
}

void * SharedLibrary::get_symbol(const std::string & symbol_name)
{
  void * lib_symbol = rcutils_get_symbol(&lib, symbol_name.c_str());

  if (!lib_symbol) {
    std::string rcutils_error_str(rcutils_get_error_string().str);
    rcutils_reset_error();
    throw std::runtime_error{rcutils_error_str};
  }
  return lib_symbol;
}

bool SharedLibrary::has_symbol(const std::string & symbol_name)
{
  return rcutils_has_symbol(&lib, symbol_name.c_str());
}

std::string SharedLibrary::get_library_path()
{
  if (lib.library_path != nullptr) {
    return std::string(lib.library_path);
  }
  throw std::runtime_error{"Library path is not defined"};
}

std::string get_platform_library_name(std::string library_name, bool debug)
{
  char library_name_platform[1024]{};
  rcutils_ret_t ret = rcutils_get_platform_library_name(
    library_name.c_str(),
    library_name_platform,
    1024,
    debug);
  if (ret != RCUTILS_RET_OK) {
    std::string rcutils_error_str(rcutils_get_error_string().str);
    rcutils_reset_error();
    throw std::runtime_error{rcutils_error_str};
  }
  return std::string(library_name_platform);
}

}  // namespace rcpputils
