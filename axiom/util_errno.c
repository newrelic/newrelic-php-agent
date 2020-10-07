/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>

#include "util_errno.h"
#include "util_logging.h"

const char* nr_errno(int errnum) {
  switch (errnum) {
    case -1:
      return "EEOF";
    case EPERM:
      return "EPERM";
    case ENOENT:
      return "ENOENT";
    case ESRCH:
      return "ESRCH";
    case EINTR:
      return "EINTR";
    case EBADF:
      return "EBADF";
    case ECHILD:
      return "ECHILD";
    case EDEADLK:
      return "EDEADLK";
    case ENOMEM:
      return "ENOMEM";
    case EACCES:
      return "EACCES";
    case EFAULT:
      return "EFAULT";
    case EBUSY:
      return "EBUSY";
    case EEXIST:
      return "EEXIST";
    case ENODEV:
      return "ENODEV";
    case ENOTDIR:
      return "ENOTDIR";
    case EISDIR:
      return "EISDIR";
    case EINVAL:
      return "EINVAL";
    case ENFILE:
      return "ENFILE";
    case EMFILE:
      return "EMFILE";
    case ENOTTY:
      return "ENOTTY";
    case ENOSPC:
      return "ENOSPC";
    case EPIPE:
      return "EPIPE";
    case EAGAIN:
      return "EAGAIN";
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
      return "EWOULDBLOCK";
#endif
    case EINPROGRESS:
      return "EINPROGRESS";
    case EALREADY:
      return "EALREADY";
    case ENOTSOCK:
      return "ENOTSOCK";
    case ENOTSUP:
      return "ENOTSUP";
    case ENETDOWN:
      return "ENETDOWN";
    case ENETUNREACH:
      return "ENETUNREACH";
    case ECONNABORTED:
      return "ECONNABORTED";
    case ECONNRESET:
      return "ECONNRESET";
    case ENOTCONN:
      return "ENOTCONN";
    case ESHUTDOWN:
      return "ESHUTDOWN";
    case ETIMEDOUT:
      return "ETIMEDOUT";
    case ECONNREFUSED:
      return "ECONNREFUSED";
    case EHOSTUNREACH:
      return "EHOSTUNREACH";
    case EIDRM:
      return "EIDRM";
    case ENOMSG:
      return "ENOMSG";
    case EILSEQ:
      return "EILSEQ";
    case EBADMSG:
      return "EBADMSG";
    case EADDRINUSE:
      return "EADDRINUSE";
    case EAFNOSUPPORT:
      return "EAFNOSUPPORT";
    case EISCONN:
      return "EISCONN";
    default:
      nrl_verbosedebug(NRL_MISC, "unsupported errno=%d", errnum);
      return "NRUNKNOWN";
  }
}
