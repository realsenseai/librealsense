// Copyright 2015-2016 Laird Shaw
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

// This function is based on the repl_str() from (on 2017-04-25):
//
//   http://creativeandcritical.net/str-replace-c
//
// It is released under the Public Domain, and has been placed additionally
// under the Apache 2.0 license by me (William Woodall).
//
// It has been modified to take a custom allocator and to fit some of our
// style standards.

// Note: the "tuning" values at the beginning of the function have been left as-is.

#ifdef __cplusplus
extern "C"
{
#endif

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
#endif

#include "rcutils/repl_str.h"

// *INDENT-OFF* (prevent uncrustify from messing with the original style)

char *
rcutils_repl_str(
  const char * str,
  const char * from,
  const char * to,
  const rcutils_allocator_t * allocator)
{
  /* Adjust each of the below values to suit your needs. */

  /* Increment positions cache size initially by this number. */
  size_t cache_sz_inc = 16;
  /* Thereafter, each time capacity needs to be increased,
   * multiply the increment by this factor. */
  const size_t cache_sz_inc_factor = 3;
  /* But never increment capacity by more than this number. */
  const size_t cache_sz_inc_max = 1048576;

  char *pret, *ret = NULL;
  const char *pstr2, *pstr = str;
  size_t i, count = 0;
  #if (__STDC_VERSION__ >= 199901L)
  uintptr_t *pos_cache_tmp, *pos_cache = NULL;
  #else
  ptrdiff_t *pos_cache_tmp, *pos_cache = NULL;
  #endif
  size_t cache_sz = 0;
  size_t cpylen, orglen, retlen, tolen, fromlen = strlen(from);

  /* Find all matches and cache their positions. */
  while ((pstr2 = strstr(pstr, from)) != NULL) {
    count++;

    /* Increase the cache size when necessary. */
    if (cache_sz < count) {
      cache_sz += cache_sz_inc;
      pos_cache_tmp =
        allocator->reallocate(pos_cache, sizeof(*pos_cache) * cache_sz, allocator->state);
      if (pos_cache_tmp == NULL) {
        goto end_repl_str;
      } else {
        pos_cache = pos_cache_tmp;
      }
      cache_sz_inc *= cache_sz_inc_factor;
      if (cache_sz_inc > cache_sz_inc_max) {
        cache_sz_inc = cache_sz_inc_max;
      }
    }

    pos_cache[count-1] = (size_t)(pstr2 - str);
    pstr = pstr2 + fromlen;
  }

  orglen = (size_t)(pstr - str) + strlen(pstr);

  /* Allocate memory for the post-replacement string. */
  if (count > 0) {
    tolen = strlen(to);
    retlen = orglen + (tolen - fromlen) * count;
  } else {
    retlen = orglen;
  }
  ret = allocator->allocate(retlen + 1, allocator->state);
  if (ret == NULL) {
    goto end_repl_str;
  }

  if (count == 0) {
    /* If no matches, then just duplicate the string. */
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4996)  // strcpy may be unsafe
#endif
    strcpy(ret, str);  // NOLINT
#if defined(_MSC_VER)
# pragma warning(pop)
#endif
  } else {
    /* Otherwise, duplicate the string whilst performing
     * the replacements using the position cache. */
    pret = ret;
    memcpy(pret, str, pos_cache[0]);
    pret += pos_cache[0];
    for (i = 0; i < count; i++) {
      memcpy(pret, to, tolen);
      pret += tolen;
      pstr = str + pos_cache[i] + fromlen;
      cpylen = (i == count-1 ? orglen : pos_cache[i+1]) - pos_cache[i] - fromlen;
      memcpy(pret, pstr, cpylen);
      pret += cpylen;
    }
    ret[retlen] = '\0';
  }

end_repl_str:
  /* Free the cache and return the post-replacement string,
   * which will be NULL in the event of an error. */
  allocator->deallocate(pos_cache, allocator->state);
  return ret;
}

// *INDENT-ON*

#ifdef __cplusplus
}
#endif
