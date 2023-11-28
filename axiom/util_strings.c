/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains string utility functions.
 */
#include "nr_axiom.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

char* nr_string_to_lowercase(const char* str) {
  int i;
  char* low;

  if (0 == str) {
    return 0;
  }

  low = nr_strdup(str);
  if (0 == low) {
    return 0;
  }

  for (i = 0; low[i]; i++) {
    low[i] = nr_tolower(low[i]);
  }
  return low;
}

char* nr_formatf(const char* fmt, ...) {
  int rv;
  va_list ap;
  char* ret = NULL;

  if (NULL == fmt) {
    return NULL;
  }

  va_start(ap, fmt);
  rv = vasprintf(&ret, fmt, ap);
  va_end(ap);

  if (rv < 0) {
    nr_free(ret);
    return NULL;
  }

  return ret;
}

nrobj_t* nr_strsplit(const char* orig, const char* delim, int use_empty) {
  nrobj_t* arr;
  const char* s;

  if (NULL == orig) {
    return NULL;
  }

  if (NULL == delim) {
    return NULL;
  }

  arr = nro_new_array();

  if (0 == orig[0]) {
    nro_set_array_string(arr, 0, "");
    return arr;
  }

  if (0 == delim[0]) {
    nro_set_array_string(arr, 0, orig);
    /* This return (unlike the code below) does not not strip whitespace */
    return arr;
  }

  s = orig;
  while (s) {
    char* dup;
    const char* end = (const char*)strpbrk(s, delim);
    const char* next;
    int len;

    if (NULL == end) {
      end = s + nr_strlen(s);
      next = NULL;
    } else {
      next = end + 1;
    }

    len = (int)(end - s);

    /* Skip trailing whitespace */
    while ((len > 0) && nr_isspace(s[len - 1])) {
      len--;
    }

    /* Skip leading whitespace */
    while ((len > 0) && nr_isspace(*s)) {
      s++;
      len--;
    }

    if ((len > 0) || use_empty) {
      dup = nr_strndup(s, len);
      nro_set_array_string(arr, 0, dup);
      nr_free(dup);
    }

    s = next;
  }

  return arr;
}

char* nr_strxcpy(char* dest, const char* src, int len) {
  if (nrlikely((0 != dest) && (0 != src) && (len > 0))) {
    nr_memcpy(dest, src, len);
    dest[len] = 0;
    return dest + len;
  }

  if (nrunlikely((0 == dest))) {
    return 0;
  }

  dest[0] = 0;
  return dest;
}

int nr_strnlen(const char* str, int maxlen) {
  if (nrlikely((0 != str) && (maxlen > 0))) {
    const char* np = (const char*)nr_memchr(str, 0, (size_t)maxlen);

    if (nrlikely(0 != np)) {
      return np - str;
    }
    return maxlen;
  }

  return 0;
}

char* nr_strcpy(char* dest, const char* src) {
  if (nrlikely((0 != dest) && (0 != src))) {
    int slen = (int)(strlen)(src);
    nr_memcpy(dest, src, (size_t)(slen + 1));
    return dest + slen;
  }

  if (nrunlikely((0 == dest))) {
    return 0;
  }

  dest[0] = 0;
  return dest;
}

char* nr_strlcpy(char* dest, const char* src, int len) {
  if (nrlikely((0 != dest) && (0 != src) && (len > 0))) {
    int slen = (int)(strlen)(src);

    if (nrunlikely(slen >= len)) {
      slen = len - 1;
    }

    nr_memcpy(dest, src, (size_t)slen);
    dest[slen] = 0;
    return dest + slen;
  }

  if (nrunlikely((0 == dest) || (len <= 0))) {
    return 0;
  }

  dest[0] = 0;
  return dest;
}

char* nr_strcat(char* dest, const char* src) {
  int dl;
  int sl;

  if (nrlikely((0 != dest) && (0 != src))) {
    dl = (int)(strlen)(dest);
    sl = (int)(strlen)(src);

    nr_memcpy(dest + dl, src, (size_t)(sl + 1));
    return dest + dl + sl;
  }

  if (nrunlikely((0 == dest))) {
    return 0;
  }

  dl = (int)(strlen)(dest);
  return dest + dl;
}

char* nr_strncat(char* dest, const char* src, int len) {
  int dl;
  int sl;

  if (nrlikely((0 != dest) && (0 != src) && (len > 0))) {
    dl = (int)(strlen)(dest);
    sl = (int)(strlen)(src);
    if (sl > len) {
      sl = len;
    }

    nr_memcpy(dest + dl, src, (size_t)sl);
    dest[dl + sl] = 0;
    return dest + dl + sl;
  }

  if (nrunlikely((0 == dest))) {
    return 0;
  }

  dl = (int)(strlen)(dest);
  return dest + dl;
}

int nr_stridx(const char* str, const char* needle) {
  if (nrlikely((0 != str) && (0 != needle))) {
    const char* found = (strstr)(str, needle);

    if (found) {
      return (int)(found - str);
    }
  }
  return -1;
}

int nr_strcaseidx(const char* str, const char* needle) {
  char c;
  int i;
  int needle_len;

  if ((0 == str) || (0 == needle)) {
    return -1;
  }

  c = needle[0];
  needle_len = nr_strlen(needle);
  if (0 == needle_len) {
    return 0;
  }

  for (i = 0; str[i]; i++) {
    if (tolower(c) == tolower(str[i])) {
      int rv = (strncasecmp)(str + i, needle, needle_len);

      if (0 == rv) {
        return i;
      }
    }
  }

  return -1;
}

int nr_strncaseidx(const char* str, const char* needle, int len) {
  char c;
  int i;
  int needle_len;

  if ((0 == str) || (0 == needle) || (len <= 0)) {
    return -1;
  }

  c = needle[0];
  needle_len = nr_strlen(needle);
  if (0 == needle_len) {
    return -1;
  }
  if (needle_len > len) {
    return -1;
  }

  for (i = 0; i < len - needle_len + 1; i++) {
    if ('\0' == str[i]) {
      return -1;
    }
    if (tolower(c) == tolower(str[i])) {
      int rv = (strncasecmp)(str + i, needle, needle_len);

      if (0 == rv) {
        return i;
      }
    }
  }

  return -1;
}

int nr_strncaseidx_last_match(const char* str, const char* needle, int len) {
  char c;
  int i;
  int needle_len;

  if ((0 == str) || (0 == needle) || (len <= 0)) {
    return -1;
  }

  /* Look for an early NUL terminator */
  for (i = 0; i < len; i++) {
    if ('\0' == str[i]) {
      len = i;
      break;
    }
  }

  c = needle[0];
  needle_len = nr_strlen(needle);
  if (0 == needle_len) {
    return -1;
  }
  if (needle_len > len) {
    return -1;
  }

  for (i = len - needle_len; i >= 0; i--) {
    if (tolower(c) == tolower(str[i])) {
      int rv = (strncasecmp)(str + i, needle, needle_len);

      if (0 == rv) {
        return i;
      }
    }
  }

  return -1;
}

int nr_strnidx(const char* str, const char* needle, int str_len) {
  if (nrlikely((0 != str) && (0 != needle) && (str_len >= 0))) {
    return nr_strnidx_impl(str, needle, str_len);
  }
  return -1;
}

int nr_strnspn(const char* s1, int s1len, const char* s2, int s2len) {
  const char* es1;
  const char* es2;
  const char* wp = s1;
  const char* spans;
  int ch;

  if (nrunlikely((0 == s1) || (0 == s2) || (s1len <= 0) || (s2len <= 0))) {
    return 0;
  }

  es1 = s1 + s1len;
  es2 = s2 + s2len;

  ch = s1[0];

next_span:
  for (spans = s2; (wp != es1) && (spans != es2);) {
    int cc = *spans;
    spans++;
    if (cc == ch) {
      wp++;
      ch = *wp;
      goto next_span;
    }
  }
  return (int)(wp - s1);
}

int nr_strncspn(const char* s1, int s1len, const char* s2, int s2len) {
  const char* es1;
  const char* es2;
  const char* wp;
  const char* spans;
  int ch;

  if (nrunlikely((0 == s1) || (0 == s2) || (s1len <= 0) || (s2len <= 0))) {
    return 0;
  }

  es1 = s1 + s1len;
  es2 = s2 + s2len;

  ch = s1[0];

  for (wp = s1;;) {
    spans = s2;
    do {
      if ((ch == spans[0]) || (wp == es1)) {
        return (int)(wp - s1);
      }
    } while (spans++ < (es2 - 1));
    wp++;
    ch = wp[0];
  }
  /*NOTREACHED*/
}

int nr_str_char_count(const char* s, char c) {
  int count = 0;
  const char* ptr;

  if (NULL == s) {
    return 0;
  }

  for (ptr = s; '\0' != *ptr; ptr++) {
    if (c == *ptr) {
      count++;
    }
  }

  return count;
}

char* nr_str_append(char* dest, const char* src, const char* delimiter) {
  char* tmp = NULL;
  const char* delim
      = (NULL != delimiter) ? delimiter : "";  // Treat NULL delimiter as no delimiter

  if (NULL == src) {
    return dest;
  }

  if (NULL == dest) {
    dest = nr_strdup(src);
  } else {
    tmp = dest;
    dest = nr_formatf("%s%s%s", dest, delim, src);
    nr_free(tmp);
  }

  return dest;
}

char* nr_file_basename(char* filename) {
  char* retval = NULL;
  size_t filename_len = 0;

  if (NULL == filename) {
    return NULL;
  }

  filename_len = nr_strlen(filename);

  if (!nr_striendswith(filename, filename_len, NR_PSTR(".php"))) {
    return filename;
  }

  retval = nr_strndup(filename, filename_len - (sizeof(".php") - 1));
  nr_free(filename);
  return retval;
}
