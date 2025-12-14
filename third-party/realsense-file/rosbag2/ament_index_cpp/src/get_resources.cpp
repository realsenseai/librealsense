// Copyright 2015 Open Source Robotics Foundation, Inc.
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

#include "ament_index_cpp/get_resources.hpp"

#ifndef _WIN32
#include <dirent.h>
#include <errno.h>
#else
#include <windows.h>
#endif
#include <map>
#include <stdexcept>
#include <string>

#include "ament_index_cpp/get_search_paths.hpp"

namespace ament_index_cpp
{

std::map<std::string, std::string>
get_resources(const std::string & resource_type)
{
  if (resource_type.empty()) {
    throw std::runtime_error("ament_index_cpp::get_resources() resource type must not be empty");
  }
  std::map<std::string, std::string> resources;
  auto paths = get_search_paths();
  for (auto base_path : paths) {
    auto path = base_path + "/share/ament_index/resource_index/" + resource_type;

#ifndef _WIN32
    auto dir = opendir(path.c_str());
    if (!dir) {
      continue;
    }
    dirent * entry;
    while ((entry = readdir(dir)) != NULL) {
      // ignore directories
      auto subdir = opendir((path + "/" + entry->d_name).c_str());
      if (subdir) {
        closedir(subdir);
        continue;
      }
      if (errno != ENOTDIR) {
        continue;
      }

      // ignore files starting with a dot
      if (entry->d_name[0] == '.') {
        continue;
      }

      if (resources.find(entry->d_name) == resources.end()) {
        resources[entry->d_name] = base_path;
      }
    }
    closedir(dir);

#else
    std::string pattern = path + "/*";
    WIN32_FIND_DATAA find_data;  // originally was WIN32_FIND_DATA and didn't support unicode, also FindFirstFile and FindNextFile
    HANDLE find_handle = FindFirstFileA(pattern.c_str(), &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
      continue;
    }
    do {
      // ignore directories
      if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        continue;
      }

      // ignore files starting with a dot
      if (find_data.cFileName[0] == '.') {
        continue;
      }

      if (resources.find(find_data.cFileName) == resources.end()) {
        resources[find_data.cFileName] = base_path;
      }
    } while (FindNextFileA(find_handle, &find_data));
    FindClose(find_handle);
#endif
  }
  return resources;
}

}  // namespace ament_index_cpp
