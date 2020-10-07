/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdlib.h>

#include "nr_configstrings.h"
#include "util_number_converter.h"
#include "util_strings.h"
#include "util_time.h"

nrtime_t nr_parse_time(const char* str) {
  char* ep;
  int suffix_seen = 0;
  nrtime_t usec = 0;
  nrtime_t msec = 0;
  nrtime_t secs = 0;
  nrtime_t mins = 0;
  nrtime_t hours = 0;
  nrtime_t days = 0;
  nrtime_t weeks = 0;
  nrtime_t val;
  nrtime_t mult;

  if ((0 == str) || (0 == str[0])) {
    return 0;
  }

  while (*str) {
    val = (nrtime_t)strtoll(str, &ep, 10);
    if (0 == val) {
      if ((const char*)ep == str) {
        break;
      }
      if (0 == *ep) {
        msec = 0;
        suffix_seen = 0;
        break;
      }
    }

    if ((0 == *ep) || (' ' == *ep) || ('\t' == *ep)) {
      msec = val;
      suffix_seen = 0;
      if (0 == *ep) {
        break;
      }
    } else if (('w' == *ep) || ('W' == *ep)) {
      weeks = val;
      suffix_seen = 1;
    } else if (('d' == *ep) || ('D' == *ep)) {
      days = val;
      suffix_seen = 1;
    } else if (('h' == *ep) || ('H' == *ep)) {
      hours = val;
      suffix_seen = 1;
    } else if (('m' == *ep) || ('M' == *ep)) {
      if (('s' == ep[1]) || ('S' == ep[1])) {
        msec = val;
        suffix_seen = 2;
      } else {
        mins = val;
        suffix_seen = 1;
      }
    } else if (('s' == *ep) || ('S' == *ep)) {
      secs = val;
      suffix_seen = 1;
    } else if ((('u' == *ep) || ('U' == *ep))
               && (('s' == ep[1]) || ('S' == ep[1]))) {
      usec = val;
      suffix_seen = 2;
    } else {
      break;
    }

    str = ep + suffix_seen;
  }

  if (0 == suffix_seen) {
    return msec * 1000;
  }

  mult = 1000;
  usec += (msec * mult);
  mult *= 1000;
  usec += (secs * mult);
  mult *= 60;
  usec += (mins * mult);
  mult *= 60;
  usec += (hours * mult);
  mult *= 24;
  usec += (days * mult);
  mult *= 7;
  usec += (weeks * mult);

  return usec;
}

int nr_bool_from_str(const char* str) {
  if ((0 == str) || (0 == str[0])) {
    return 0;
  }

  if (0 == str[1]) {
    if ('1' == str[0]) {
      return 1;
    } else if ('0' == str[0]) {
      return 0;
    } else if (('y' == str[0]) || ('Y' == str[0])) {
      return 1;
    } else if (('n' == str[0]) || ('N' == str[0])) {
      return 0;
    } else if (('t' == str[0]) || ('T' == str[0])) {
      return 1;
    } else if (('f' == str[0]) || ('F' == str[0])) {
      return 0;
    }
  } else if (0 == nr_stricmp(str, "on")) {
    return 1;
  } else if (0 == nr_stricmp(str, "off")) {
    return 0;
  } else if (0 == nr_stricmp(str, "yes")) {
    return 1;
  } else if (0 == nr_stricmp(str, "no")) {
    return 0;
  } else if (0 == nr_stricmp(str, "true")) {
    return 1;
  } else if (0 == nr_stricmp(str, "false")) {
    return 0;
  } else if (0 == nr_stricmp(str, "enabled")) {
    return 1;
  } else if (0 == nr_stricmp(str, "disabled")) {
    return 0;
  } else if (0 == nr_stricmp(str, "enable")) {
    return 1;
  } else if (0 == nr_stricmp(str, "disable")) {
    return 0;
  }

  return -1;
}
