/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "util_number_converter.h"
#include "util_strings.h"

void nr_itoa(char* buf, size_t len, int x) {
  char scratch[24];
  char* p;
  char sign;
  unsigned y;

  /*
   * Build the string from right to left.
   * Invariant: p points to the last character written.
   */

  p = scratch + sizeof(scratch) - 1;
  *p = '\0';

  sign = '\0';
  if (x >= 0) {
    y = x;
  } else {
    sign = '-';
    y = -x;
  }

  do {
    *--p = (y % 10) + '0';
    y /= 10;
  } while (y > 0);

  if (sign) {
    *--p = sign;
  }

  nr_strlcpy(buf, p, len);
}

int nr_double_to_str(char* buf, int buf_len, double input) {
  int i;
  int natural_width;
  int actually_written;

  if (NULL == buf) {
    return -1;
  }
  if (buf_len <= 0) {
    return -1;
  }

  /*
   * We used to use %.5f, but that prints out too much for really big numbers.
   * %.17g seems to work quite well for IEEE-754 64-bit numbers; %g prints the
   * shortest representation possible.  However, %g on a number like 123456.789
   * merely prints "123456".  %.17g on 0.333 prints something like
   * "0.3330000000005
   */
  natural_width = snprintf(buf, buf_len, "%.5f", input);

  actually_written
      = (natural_width < (buf_len - 1)) ? natural_width : (buf_len - 1);

  /*
   * Replace LC_NUMERIC non-C locale separator(s) with C-locale
   * separators. The only reader of the JSON we write is either the
   * collector, which needs C-locale '.' decimal radix point, or the
   * daemon (reading from the agent), which starts up in C-locale, and so
   * needs C-locale '.' decimal radix point. Consequently, we can just
   * assume that any ',' is a locale specific decimal radix point, and
   * just simply replace that ',' with a '.'.
   *
   * In the locales tested on macosx snprintf uses either '.' or
   * ',' for the decimal radix point There is no extra insertion of
   * characters to block into 1000's (or 10000s).
   *
   * The story isn't so clean for nr_strtod, which is scanning numbers from a
   * variety of sources.
   */

  for (i = 0; i < actually_written; i++) {
    if (',' == buf[i]) {
      buf[i] = '.';
      break;
    }
  }

  return actually_written;
}

/*
 * nr_strtod will only be used to scan numbers that obey JSON
 * conventions from http://json.org/
 *
 * Because of the JSON spec, we only expect to see numbers written by a
 * "C"-locale printf which uses '.' as the decimal radix.
 *
 * We scan what we are given using the C library strtod, which will use whatever
 * locale is in force.  It may stop on a '.' which we intend it to scan,
 * or it may greedily overscan a ',' which we intend it to not scan.
 *
 * In both cases we make a local copy of the incoming number, fiddle with
 * characters, and rescan with the locale sensitive strtod.
 *
 * For the common case of "C" locale, or "en_EN", we'll only call srtod on the
 * string once, which is the most expensive part.
 */
double nr_strtod(const char* buf, char** endptr) {
  typedef enum { OK, UNDERSHOT, OVERSHOT } scan_status_t;
  scan_status_t work_code;
  char copy[BUFSIZ];
  char* myendptr = 0;
  size_t i;
  size_t position_radix;
  double converted_value;

  /*
   * strtod will segfault when given a null pointer.
   */
  if (0 == buf) {
    if (endptr) {
      *endptr = 0;
    }
    return 0.0;
  }

  converted_value = strtod(buf, &myendptr);

  work_code = OK;
  for (i = 0; &buf[i] != myendptr; i++) {
    if ('.' == buf[i]) {
      /*
       * strtod scanned through a '.', so that must have been the current
       * locale's decimal radix character.
       */
      if (endptr) {
        *endptr = myendptr;
      }
      return converted_value;
    }
    if (',' == buf[i]) {
      /*
       * strtod scanned through a ',', so that must have been the current
       * locale's decimal radix character, which is not what the writer
       * intended.
       */
      work_code = OVERSHOT;
      position_radix = i;
      break;
    }
  }

  if (OK == work_code) {
    if ('.' != *myendptr) {
      /*
       * strtod didn't see any '.' or ',', and came to a stop at a non '.',
       * which is a legitimate termination for either kind of radix character.
       */
      if (endptr) {
        *endptr = myendptr;
      }
      return converted_value;
    } else {
      position_radix = myendptr - buf;
    }
    work_code = UNDERSHOT;
  }

  /*
   * Copy into a local buffer, so we can try to convert the fixed incoming
   * decimal radix '.' to the locale's presumed radix character of ','.
   */
  nr_strlcpy(copy, buf, sizeof(copy));

  if (UNDERSHOT == work_code) {
    copy[position_radix] = ','; /* strtod refused to scan a '.', and it
                                   presumably would scan a ','.  */
  } else {
    copy[position_radix] = 0; /* terminate scan at the overscanned ',' */
  }

  converted_value = strtod(copy, &myendptr);
  if (endptr) {
#if defined(__clang__) \
    || ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5)))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
    *endptr = (char*)buf + (myendptr - copy);
#if defined(__clang_) \
    || ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5)))
#pragma GCC diagnostic pop
#endif
  }

  return converted_value;
}
