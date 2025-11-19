// Copyright 2017 Open Source Robotics Foundation, Inc.
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

#include <string.h>

#include "rcutils/cmdline_parser.h"

bool rcutils_cli_option_exist(char ** begin, char ** end, const char * option)
{
  // return std::find(begin, end, option) != end;
  for (size_t i = 0; i < (size_t)(end - begin); ++i) {
    if (strcmp(begin[i], option) == 0) {
      return true;
    }
  }
  return false;
}

char * rcutils_cli_get_option(char ** begin, char ** end, const char * option)
{
  size_t idx = 0;
  size_t end_idx = (size_t)(end - begin);
  for (; idx < end_idx; ++idx) {
    if (strncmp(begin[idx], option, strlen(option)) == 0) {
      break;
    }
  }

  if (idx < end_idx - 1 && begin[idx++] != NULL) {
    return begin[idx];
  }

  return NULL;
}
