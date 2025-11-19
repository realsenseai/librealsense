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

#ifndef TYPE_SUPPORT_DISPATCH_HPP_
#define TYPE_SUPPORT_DISPATCH_HPP_

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

#include <string>

#include "rcpputils/find_library.hpp"
#include "rcpputils/shared_library.hpp"
#include "rosidl_typesupport_c/type_support_map.h"

namespace rosidl_typesupport_cpp
{

extern const char * typesupport_identifier;

template<typename TypeSupport>
const TypeSupport *
get_typesupport_handle_function(
  const TypeSupport * handle, const char * identifier)
{
  if (strcmp(handle->typesupport_identifier, identifier) == 0) {
    return handle;
  }

  if (handle->typesupport_identifier == rosidl_typesupport_cpp::typesupport_identifier) {
    const type_support_map_t * map = \
      static_cast<const type_support_map_t *>(handle->data);
    for (size_t i = 0; i < map->size; ++i) {
      if (strcmp(map->typesupport_identifier[i], identifier) != 0) {
        continue;
      }
      rcpputils::SharedLibrary * lib = nullptr;

      if (!map->data[i]) {
        std::string library_name{map->package_name};
        library_name += "__";
        library_name += identifier;

        std::string library_path;
        try {
          library_path = rcpputils::find_library_path(library_name);
        } catch (const std::runtime_error & e) {
          const std::string message =
            "Failed to find library '" + library_name + "' due to " + e.what();
          throw std::runtime_error(message);
        }

        if (library_path.empty()) {
          fprintf(stderr, "Failed to find library '%s'\n", library_name.c_str());
          return nullptr;
        }

        try {
          lib = new rcpputils::SharedLibrary(library_path.c_str());
        } catch (const std::runtime_error & e) {
          throw std::runtime_error(
                  "Could not load library " + library_path + ": " +
                  std::string(e.what()));
        } catch (const std::bad_alloc & e) {
          throw std::runtime_error(
                  "Could not load library " + library_path + ": " +
                  std::string(e.what()));
        }
        map->data[i] = lib;
      }
      auto clib = static_cast<const rcpputils::SharedLibrary *>(map->data[i]);
      lib = const_cast<rcpputils::SharedLibrary *>(clib);
      if (!lib->has_symbol(map->symbol_name[i])) {
        fprintf(stderr, "Failed to find symbol '%s' in library\n", map->symbol_name[i]);
        return nullptr;
      }
      void * sym = lib->get_symbol(map->symbol_name[i]);

      typedef const TypeSupport * (* funcSignature)(void);
      funcSignature func = reinterpret_cast<funcSignature>(sym);
      const TypeSupport * ts = func();
      return ts;
    }
  }
  return nullptr;
}

}  // namespace rosidl_typesupport_cpp

#endif  // TYPE_SUPPORT_DISPATCH_HPP_
