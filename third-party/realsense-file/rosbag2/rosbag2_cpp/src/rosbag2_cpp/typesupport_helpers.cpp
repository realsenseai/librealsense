// Copyright 2018, Bosch Software Innovations GmbH.
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

#include "rosbag2_cpp/typesupport_helpers.hpp"

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include "ament_index_cpp/get_resources.hpp"
#include "ament_index_cpp/get_package_prefix.hpp"

#include "rcpputils/shared_library.hpp"

#include "rosidl_runtime_cpp/message_type_support_decl.hpp"

namespace rosbag2_cpp
{

std::string get_typesupport_library_path(
  const std::string & package_name, const std::string & typesupport_identifier)
{
  const char * filename_prefix;
  const char * filename_extension;
  const char * dynamic_library_folder;
#ifdef _WIN32
  filename_prefix = "";
  filename_extension = ".dll";
  dynamic_library_folder = "/bin/";
#elif __APPLE__
  filename_prefix = "lib";
  filename_extension = ".dylib";
  dynamic_library_folder = "/lib/";
#else
  filename_prefix = "lib";
  filename_extension = ".so";
  dynamic_library_folder = "/lib/";
#endif

  std::string package_prefix;
  try {
    package_prefix = ament_index_cpp::get_package_prefix(package_name);
  } catch (ament_index_cpp::PackageNotFoundError & e) {
    throw std::runtime_error(e.what());
  }

  auto library_path = package_prefix + dynamic_library_folder + filename_prefix +
    package_name + "__" + typesupport_identifier + filename_extension;
  return library_path;
}

const std::pair<std::string, std::string> extract_type_and_package(const std::string & full_type)
{
  std::string package_name;
  std::string type_name;

  std::tie(package_name, std::ignore, type_name) = extract_type_identifier(full_type);

  return {package_name, type_name};
}

const std::tuple<std::string, std::string, std::string>
extract_type_identifier(const std::string & full_type)
{
  char type_separator = '/';
  auto sep_position_back = full_type.find_last_of(type_separator);
  auto sep_position_front = full_type.find_first_of(type_separator);
  if (sep_position_back == std::string::npos ||
    sep_position_back == 0 ||
    sep_position_back == full_type.length() - 1)
  {
    throw std::runtime_error(
            "Message type is not of the form package/type and cannot be processed");
  }

  std::string package_name = full_type.substr(0, sep_position_front);
  std::string middle_module = "";
  if (sep_position_back - sep_position_front > 0) {
    middle_module =
      full_type.substr(sep_position_front + 1, sep_position_back - sep_position_front - 1);
  }
  std::string type_name = full_type.substr(sep_position_back + 1);

  return std::make_tuple(package_name, middle_module, type_name);
}

std::shared_ptr<rcpputils::SharedLibrary>
get_typesupport_library(const std::string & type, const std::string & typesupport_identifier)
{
  auto package_name = std::get<0>(extract_type_identifier(type));
  auto library_path = get_typesupport_library_path(package_name, typesupport_identifier);
  return std::make_shared<rcpputils::SharedLibrary>(library_path);
}

const rosidl_message_type_support_t *
get_typesupport_handle(
  const std::string & type,
  const std::string & typesupport_identifier,
  std::shared_ptr<rcpputils::SharedLibrary> library)
{
  if (nullptr == library) {
    throw std::runtime_error(
            "rcpputils::SharedLibrary not initialized. Call get_typesupport_library first.");
  }

  std::string package_name;
  std::string middle_module;
  std::string type_name;
  std::tie(package_name, middle_module, type_name) = extract_type_identifier(type);

  std::stringstream rcutils_dynamic_loading_error;
  rcutils_dynamic_loading_error <<
    "Something went wrong loading the typesupport library for message type " << package_name <<
    "/" << type_name << ".";

  try {
    auto symbol_name = typesupport_identifier + "__get_message_type_support_handle__" +
      package_name + "__" + (middle_module.empty() ? "msg" : middle_module) + "__" + type_name;

    if (!library->get_symbol(symbol_name)) {
      throw std::runtime_error{
              rcutils_dynamic_loading_error.str() +
              std::string(" Symbol not found.")};
    }

    const rosidl_message_type_support_t * (* get_ts)() = nullptr;
    get_ts = (decltype(get_ts))library->get_symbol(symbol_name);

    if (!get_ts) {
      throw std::runtime_error{
              rcutils_dynamic_loading_error.str() +
              std::string(" Symbol of wrong type.")};
    }
    auto type_support = get_ts();
    return type_support;
  } catch (std::runtime_error &) {
    throw std::runtime_error(rcutils_dynamic_loading_error.str() + " Library could not be found.");
  }
}

}  // namespace rosbag2_cpp
