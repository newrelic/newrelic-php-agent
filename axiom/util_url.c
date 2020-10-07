/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_memory.h"
#include "util_strings.h"
#include "util_url.h"

#define NR_URL_QUERY_CHARS "#?;"

/*
 * Using 4 '*' characters as the user and password mask to align with the
 * implementation of other agents (i.e., Node).
 *
 * Per http://curl.haxx.se/libcurl/c/CURLOPT_PROXY.html, the expected input (for
 * proxy's) can have format: [scheme://][user][:password][@host][:port]
 */
#define NR_PROXY_CREDS_MASK "****"
#define NR_PROXY_CREDS_MASK_LENGTH (sizeof(NR_PROXY_CREDS_MASK) - 1)

static char* nr_url_clean_internal(const char* url,
                                   int urllen,
                                   int mask_creds) {
  int ui = 0; /* Index into url */
  int ci = 0; /* Index into clean */
  int len;    /* Index of url params or early null terminator */
  char* clean;

  if ((urllen <= 0) || (0 == url) || (0 == url[0])) {
    return 0;
  }

  /*
   * Scan the string for parameters or early null terminator to find where
   * iteration can stop.
   */
  len = nr_strncspn(url, urllen, NR_URL_QUERY_CHARS "\0",
                    sizeof(NR_URL_QUERY_CHARS));
  if (len <= 0) {
    return 0;
  }

  clean = (char*)nr_malloc(len + (2 * NR_PROXY_CREDS_MASK_LENGTH) + 1);

  while (ui < len) {
    if ('@' == url[ui]) {
      int has_password = 0;
      /*
       * Back up over the username:password to scheme:// or the start
       * of the string.
       */
      while (ci) {
        ci--;
        if (':' == clean[ci]) {
          has_password = 1;
        } else if ('/' == clean[ci]) {
          ci++;
          break;
        }
      }

      if (mask_creds) {
        nr_memcpy(clean + ci, NR_PROXY_CREDS_MASK, NR_PROXY_CREDS_MASK_LENGTH);
        ci += NR_PROXY_CREDS_MASK_LENGTH;

        if (has_password) {
          clean[ci] = ':';
          ci++;

          nr_memcpy(clean + ci, NR_PROXY_CREDS_MASK,
                    NR_PROXY_CREDS_MASK_LENGTH);
          ci += NR_PROXY_CREDS_MASK_LENGTH;
        }

        clean[ci] = '@';
        ci++;
      }
    } else {
      clean[ci] = url[ui];
      ci++;
    }

    ui++;
  }

  clean[ci] = '\0';
  return clean;
}

char* nr_url_clean(const char* url, int urllen) {
  return nr_url_clean_internal(url, urllen, 0);
}

char* nr_url_proxy_clean(const char* url) {
  return nr_url_clean_internal(url, nr_strlen(url), 1);
}

const char* nr_url_extract_domain(const char* url, int urllen, int* dnlen) {
  int i;
  int start = 0; /* Index of beginning of domain */
  int stop = -1; /* Index of end of domain */
  int after_at = 0;
  int after_colon_slash_slash = 0;

  if (nrunlikely(0 == dnlen)) {
    return 0;
  }

  *dnlen = -1;

  if (nrunlikely((0 == url) || (0 == url[0]) || (urllen <= 0))) {
    return 0;
  }

  for (i = 0; i < urllen; i++) {
    if ('@' == url[i]) {
      if (after_at) {
        return 0;
      }
      after_at = i + 1;
      start = after_at;
      stop = -1;
    } else if (':' == url[i]) {
      if ((i < urllen - 2) && ('/' == url[i + 1]) && ('/' == url[i + 2])) {
        if ((0 != after_at) || (0 != after_colon_slash_slash)) {
          return 0;
        }
        after_colon_slash_slash = i + 3;
        start = after_colon_slash_slash;
        stop = -1;
        i += 2;
      } else if (-1 == stop) {
        stop = i;
      }
    } else if ((0 == url[i])
               || (0 != nr_strchr("/" NR_URL_QUERY_CHARS, url[i]))) {
      break;
    }
  }

  if (-1 == stop) {
    stop = i;
  }

  if ((start >= urllen) || (start >= stop)) {
    return 0;
  }

  *dnlen = stop - start;
  return &url[start];
}
