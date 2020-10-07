/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_object.h"
#include "util_reply.h"
#include "util_strings.h"

int nr_reply_get_int(const nrobj_t* reply, const char* name, int dflt) {
  const nrobj_t* rp;
  int ret;
  nr_status_t err;

  if (nrunlikely((0 == reply) || (0 == name) || (0 == name[0]))) {
    return dflt;
  }

  rp = nro_get_hash_value(reply, name, 0);
  if (0 == rp) {
    return dflt;
  }

  ret = nro_get_ival(rp, &err);
  if (NR_FAILURE == err) {
    return dflt;
  }

  return ret;
}

int nr_reply_get_bool(const nrobj_t* reply, const char* name, int dflt) {
  const nrobj_t* rp;
  int ret;
  nr_status_t err;
  const char* str;
  char c;

  if (nrunlikely((0 == reply) || (0 == name) || (0 == name[0]))) {
    return dflt;
  }

  rp = nro_get_hash_value(reply, name, 0);
  if (0 == rp) {
    return dflt;
  }

  ret = nro_get_ival(rp, &err);
  if (NR_SUCCESS == err) {
    return ret;
  }

  str = nro_get_string(rp, &err);
  if (NR_FAILURE == err) {
    return dflt;
  }

  c = str[0];

  if (('1' == c) || ('y' == c) || ('Y' == c) || ('t' == c) || ('T' == c)
      || (0 == nr_stricmp(str, "on"))) {
    return 1;
  }

  if (('0' == c) || ('n' == c) || ('N' == c) || ('f' == c) || ('F' == c)
      || (0 == nr_stricmp(str, "off"))) {
    return 0;
  }

  return dflt;
}

double nr_reply_get_double(const nrobj_t* reply,
                           const char* name,
                           double dflt) {
  const nrobj_t* rp;
  nr_status_t err;
  double ret;
  int64_t iret;
  uint64_t uret;

  if (nrunlikely((0 == reply) || (0 == name) || (0 == name[0]))) {
    return dflt;
  }

  rp = nro_get_hash_value(reply, name, 0);
  if (0 == rp) {
    return dflt;
  }

  switch (nro_type(rp)) {
    case NR_OBJECT_INT:
      iret = (int64_t)nro_get_ival(rp, &err);
      if (NR_SUCCESS == err) {
        return (double)iret;
      }
      break;

    case NR_OBJECT_LONG:
      iret = nro_get_long(rp, &err);
      if (NR_SUCCESS == err) {
        return (double)iret;
      }
      break;

    case NR_OBJECT_ULONG:
      uret = nro_get_ulong(rp, &err);
      if (NR_SUCCESS == err) {
        return (double)uret;
      }
      break;

    case NR_OBJECT_DOUBLE:
      ret = nro_get_double(rp, &err);
      if (NR_SUCCESS == err) {
        return ret;
      }
      break;

    case NR_OBJECT_INVALID:
    case NR_OBJECT_NONE:
    case NR_OBJECT_BOOLEAN:
    case NR_OBJECT_STRING:
    case NR_OBJECT_JSTRING:
    case NR_OBJECT_HASH:
    case NR_OBJECT_ARRAY:
      break;
  }

  return dflt;
}
