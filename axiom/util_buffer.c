/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "util_buffer.h"
#include "util_json.h"
#include "util_memory.h"
#include "util_strings.h"

struct _nrintbuf_t {
  int avail;   /* Data bytes available in the buffer */
  int bsize;   /* Allocated buffer size */
  int bptr;    /* Index to first unread byte in buffer */
  int extsize; /* Extension size */
  char* buf;   /* The actual buffer */
};

int nr_buffer_len(const nrbuf_t* bufp) {
  if (0 == bufp) {
    return -1;
  }

  return bufp->avail;
}

const void* nr_buffer_cptr(const nrbuf_t* bufp) {
  if ((0 == bufp) || (bufp->avail <= 0) || (0 == bufp->buf)) {
    return 0;
  }

  return (const void*)(bufp->buf + bufp->bptr);
}

void nr_buffer_reset(nrbuf_t* bufp) {
  if (0 == bufp) {
    return;
  }

  bufp->avail = 0;
  bufp->bptr = 0;
}

nrbuf_t* nr_buffer_create(int estsize, int extsize) {
  nrbuf_t* bp;

  if (estsize < 1024) {
    estsize = 1024;
  }
  if (extsize <= 0) {
    extsize = estsize / 3;
  }
  if (extsize < 512) {
    extsize = 512;
  }

  bp = (nrbuf_t*)nr_zalloc(sizeof(nrbuf_t));
  bp->bsize = estsize;
  bp->extsize = extsize;
  bp->buf = (char*)nr_malloc(estsize);
  return (nrbuf_t*)bp;
}

void nr_buffer_add(nrbuf_t* bp, const void* data, int dlen) {
  int bytes_remaining;
  int bytes_needed;

  if (NULL == bp) {
    return;
  }
  if (dlen <= 0) {
    return;
  }

  bytes_remaining = bp->bsize - bp->avail;
  bytes_needed = dlen + bp->avail;

  if (bytes_remaining < bytes_needed) {
    int bytes_extra = bytes_needed - bytes_remaining;

    if (bytes_extra < bp->extsize) {
      bp->bsize += bp->extsize;
    } else {
      nr_clang_assert(bp->extsize > 0);
      bp->bsize += ((bytes_extra / bp->extsize) + 1) * bp->extsize;
    }
    bp->buf = (char*)nr_realloc(bp->buf, bp->bsize);
  }

  if (0 != bp->bptr) {
    nr_memmove(bp->buf, bp->buf + bp->bptr, bp->avail);
    bp->bptr = 0;
  }

  if (data) {
    nr_memcpy(bp->buf + bp->avail, data, dlen);
  }

  bp->avail += dlen;
}

/*
 * Purpose : Ensure there is enough space in a buffer for incoming data.
 *
 * Params  : 1. The buffer to size.
 *           2. How many bytes of input are expected.
 *
 * Returns : A pointer to the exact location where data of the requested size
 *           can be stored. This points to data INSIDE the buffer.
 *
 * Notes   : This function is provided to allow the buffer to have space pre-
 *           allocated for it, so that external code can read into the buffer.
 *           See the notes above for nr_buffer_add() as they apply to this
 *           function too.
 *
 *           IMPORTANT WARNING: this function can end up causing corruption if
 *           it is not immediately followed by an nr_buffer_add(). The expected
 *           calling sequence is:
 *
 *           ptr = nr_buffer_ensure (buf, SIZE);
 *           put_data_into (ptr);
 *           nr_buffer_add (buf, NULL, SIZE);
 */
static void* nr_buffer_ensure(nrbuf_t* bp, size_t reqsize) {
  if (nrunlikely(0 == bp)) {
    return 0;
  } else {
    int bytes_remaining = bp->bsize - bp->avail;
    int bytes_needed = reqsize + bp->avail;

    if (bytes_remaining < bytes_needed) {
      int bytes_extra = bytes_needed - bytes_remaining;

      if (bytes_extra < bp->extsize) {
        bp->bsize += bp->extsize;
      } else {
        bp->bsize += ((bytes_extra / bp->extsize) + 1) * bp->extsize;
      }
      bp->buf = (char*)nr_realloc(bp->buf, bp->bsize);
    }

    if (0 != bp->bptr) {
      nr_memmove(bp->buf, bp->buf + bp->bptr, bp->avail);
      bp->bptr = 0;
    }

    return bp->buf + bp->avail;
  }
}

void nr_buffer_add_escape_json(nrbuf_t* bufp, const char* raw_string) {
  size_t raw_string_len;
  size_t escaped_space_needed;
  size_t escaped_len;
  char* bp;

  if ((0 == bufp) || (0 == raw_string)) {
    return;
  }

  raw_string_len = nr_strlen(raw_string);
  escaped_space_needed = (raw_string_len * 6) + 3;

  bp = (char*)nr_buffer_ensure(bufp, escaped_space_needed);
  if (0 == bp) {
    return;
  }

  escaped_len = nr_json_escape(bp, raw_string);
  nr_buffer_add(bufp, 0, escaped_len);
}

int nr_buffer_use(nrbuf_t* bufp, void* destp, int dlen) {
  if (nrunlikely((0 == bufp) || (dlen < 0))) {
    return -1;
  } else {
    nrbuf_t* bp = (nrbuf_t*)bufp;

    if (dlen > bp->avail) {
      dlen = bp->avail;
    }

    if (0 != destp) {
      nr_memcpy(destp, bp->buf + bp->bptr, dlen);
    }

    bp->avail -= dlen;
    if (0 == bp->avail) {
      bp->bptr = 0;
    } else {
      bp->bptr += dlen;
    }

    return dlen;
  }
}

void nr_buffer_destroy(nrbuf_t** bufp) {
  if (nrunlikely((0 == bufp) || (0 == *bufp))) {
    return;
  } else {
    nrbuf_t* bp = *(nrbuf_t**)bufp;

    nr_free(bp->buf);
    nr_realfree((void**)bufp);
  }
}

void nr_buffer_write_uint32_t_le(nrbuf_t* bufp, uint32_t val) {
  uint8_t bytes[sizeof(uint32_t)];

  nr_memset(bytes, 0, sizeof(bytes));

  bytes[0] = (uint8_t)(val >> 0);
  bytes[1] = (uint8_t)(val >> 8);
  bytes[2] = (uint8_t)(val >> 16);
  bytes[3] = (uint8_t)(val >> 24);

  nr_buffer_add(bufp, &bytes, sizeof(bytes));
}

void nr_buffer_write_uint64_t_as_text(nrbuf_t* bufp, uint64_t val) {
  char tmp[256];
  int sl;

  if (0 == bufp) {
    return;
  }

  tmp[0] = 0;
  sl = snprintf(tmp, sizeof(tmp), NR_UINT64_FMT, val);
  nr_buffer_add(bufp, tmp, sl);
}

nr_status_t nr_buffer_read_uint32_t_le(nrbuf_t* bufp, uint32_t* val) {
  uint32_t x = 0;
  uint8_t bytes[sizeof(x)];

  if (NULL == bufp) {
    return NR_FAILURE;
  }
  if (NULL == val) {
    return NR_FAILURE;
  }

  nr_memset(bytes, 0, sizeof(bytes));
  if (sizeof(bytes) != nr_buffer_use(bufp, (void*)bytes, sizeof(bytes))) {
    return NR_FAILURE;
  }

  x |= (uint32_t)bytes[0] << 0;
  x |= (uint32_t)bytes[1] << 8;
  x |= (uint32_t)bytes[2] << 16;
  x |= (uint32_t)bytes[3] << 24;

  *val = x;

  return NR_SUCCESS;
}

char nr_buffer_peek_end(nrbuf_t* bufp) {
  if (NULL == bufp || NULL == bufp->buf || 0 == bufp->avail) {
    return 0;
  }

  return bufp->buf[bufp->avail - 1];
}
