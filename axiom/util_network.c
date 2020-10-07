/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#include "util_errno.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_syscalls.h"

static nr_status_t nr_wait_fd(int fd, int events, nrtime_t deadline) {
  struct pollfd pfd;
  int timeout_msec;

  timeout_msec = 0;

  if (deadline > 0) {
    nrtime_t now;

    now = nr_get_time();
    if (now > deadline) {
      errno = ETIMEDOUT;
      return NR_FAILURE;
    }

    timeout_msec = (deadline - now) / NR_TIME_DIVISOR_MS;
    if (0 == timeout_msec) {
      /* Ensure we do not block if a timeout was requested. */
      timeout_msec = 1;
    }
  }

  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;

  for (;;) {
    int rv;
    int err;

    rv = nr_poll(&pfd, 1, timeout_msec);
    if (rv > 0) {
      return NR_SUCCESS;
    }

    if (0 == rv) {
      errno = ETIMEDOUT;
      return NR_FAILURE;
    }

    err = errno;
    if (EINTR == err) {
      continue;
    }

    if ((EAGAIN == err) || (EWOULDBLOCK == err)) {
      return NR_SUCCESS;
    }

    return NR_FAILURE;
  }
}

nr_status_t nr_write_full(int fd,
                          const void* buf,
                          size_t len,
                          nrtime_t deadline) {
  const char* p;
  ssize_t remaining;
  int err;

  if ((fd < 0) || (0 == buf)) {
    errno = EINVAL;
    return NR_FAILURE;
  }

  p = (const char*)buf;
  remaining = (ssize_t)len;

  while (remaining > 0) {
    ssize_t rv;

    rv = nr_write(fd, p, remaining);
    if (rv >= 0) {
      p += rv;
      remaining -= rv;
      continue;
    }

    err = errno;
    if (EINTR == err) {
      continue;
    }

    if ((EAGAIN != err) && (EWOULDBLOCK != err)) {
      return NR_FAILURE;
    }

    if (NR_FAILURE == nr_wait_fd(fd, POLLOUT, deadline)) {
      return NR_FAILURE;
    }
  }

  return NR_SUCCESS;
}

nr_status_t nr_write_message(int fd,
                             const void* buf,
                             size_t len,
                             nrtime_t deadline) {
  nrbuf_t* header;
  nr_status_t st;

  if ((fd < 0) || (NULL == buf)) {
    errno = EINVAL;
    return NR_FAILURE;
  }
  if (len > NR_PROTOCOL_CMDLEN_MAX_BYTES) {
    errno = EINVAL;
    return NR_FAILURE;
  }

  header = nr_buffer_create(NR_PROCOTOL_PREAMBLE_LENGTH, 0);
  nr_protocol_write_preamble(header, len);

  st = nr_write_full(fd, nr_buffer_cptr(header), nr_buffer_len(header),
                     deadline);
  nr_buffer_destroy(&header);

  if (NR_FAILURE == st) {
    return NR_FAILURE;
  }

  return nr_write_full(fd, buf, len, deadline);
}

static nrbuf_t* nrn_read_internal(int fd,
                                  int nbytes,
                                  nrtime_t deadline,
                                  char* tmp) {
  int remaining;
  ssize_t rv; /* result of the most recent call to read() */
  int err;
  nrbuf_t* buf;

  if (fd <= 0) {
    errno = EINVAL;
    return NULL;
  }

  remaining = nbytes;

  while (remaining > 0) {
    rv = nr_read(fd, tmp + (nbytes - remaining), remaining);

    if (0 == rv) {
      errno = -1; /* our own EEOF */
      return NULL;
    }

    if (rv > 0) {
      remaining -= rv;
      continue;
    }

    err = errno;
    if (EINTR == err) {
      continue;
    }

    if ((EAGAIN != err) && (EWOULDBLOCK != err)) {
      return NULL;
    }

    if (NR_FAILURE == nr_wait_fd(fd, POLLIN, deadline)) {
      return NULL;
    }
  }

  buf = nr_buffer_create(nbytes, 0);
  nr_buffer_add(buf, tmp, nbytes);
  return buf;
}

nrbuf_t* nrn_read(int fd, int nbytes, nrtime_t deadline) {
  char* tmp;
  nrbuf_t* reply;

  if (nbytes <= 0) {
    errno = EINVAL;
    return NULL;
  }

  tmp = (char*)nr_malloc(nbytes);
  reply = nrn_read_internal(fd, nbytes, deadline, tmp);
  nr_free(tmp);

  return reply;
}

void nr_protocol_write_preamble(nrbuf_t* buf, uint32_t datalen) {
  nr_buffer_write_uint32_t_le(buf, datalen);
  nr_buffer_write_uint32_t_le(buf, NR_PREAMBLE_FORMAT);
}

nr_status_t nr_protocol_parse_preamble(nrbuf_t* buf, uint32_t* return_length) {
  uint32_t length = 0;
  uint32_t format = 0;

  if (NULL == return_length) {
    return NR_FAILURE;
  }

  if (NR_SUCCESS != nr_buffer_read_uint32_t_le(buf, &length)) {
    nrl_error(NRL_NETWORK, "parse preamble failure: unable to read length");
    return NR_FAILURE;
  }

  if (NR_SUCCESS != nr_buffer_read_uint32_t_le(buf, &format)) {
    nrl_error(NRL_NETWORK, "parse preamble failure: unable to read format");
    return NR_FAILURE;
  }

  if (NR_PREAMBLE_FORMAT != format) {
    nrl_error(NRL_NETWORK, "parse preamble failure: invalid format: %u",
              format);
    return NR_FAILURE;
  }

  if (length > NR_PROTOCOL_CMDLEN_MAX_BYTES) {
    nrl_error(NRL_NETWORK, "parse preamble failure: invalid length: %d",
              length);
    return NR_FAILURE;
  }

  *return_length = length;

  return NR_SUCCESS;
}

nrbuf_t* nr_network_receive(int fd, nrtime_t deadline) {
  nrbuf_t* preamble;
  nrbuf_t* msg;
  nr_status_t st;
  uint32_t len;

  preamble = nrn_read(fd, NR_PROCOTOL_PREAMBLE_LENGTH, deadline);
  if (NULL == preamble) {
    nrl_error(NRL_NETWORK,
              "failed to read reply preamble: fd=%d errno=" NRP_FMT_UQ, fd,
              NRP_STRERROR(nr_errno(errno)));
    return NULL;
  }

  len = 0;
  st = nr_protocol_parse_preamble(preamble, &len);
  nr_buffer_destroy(&preamble);

  if (NR_SUCCESS != st) {
    return NULL;
  }

  msg = nrn_read(fd, len, deadline);
  if (NULL == msg) {
    nrl_error(NRL_NETWORK,
              "failed to read reply msg: len=%d fd=%d errno=" NRP_FMT_UQ, len,
              fd, NRP_STRERROR(nr_errno(errno)));
    return NULL;
  }
  return msg;
}

nr_status_t nr_network_set_non_blocking(int fd) {
  int flags;

  if (fd < 0) {
    return NR_FAILURE;
  }

  flags = nr_fcntl(fd, F_GETFL, 0);

  if (flags < 0) {
    return NR_FAILURE;
  }

  flags |= O_NONBLOCK;

  if (nr_fcntl(fd, F_SETFL, flags) < 0) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}
