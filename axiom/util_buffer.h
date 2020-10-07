/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for a general purpose data buffer.
 *
 * A simple mechanism for buffering data to be transmitted or stored. Allows
 * both writes to and reads from the buffer. The buffer data type is completely
 * opaque. Only access the buffer through the functions provided below.
 */
#ifndef UTIL_BUFFER_HDR
#define UTIL_BUFFER_HDR

#include <stdint.h>

#include "nr_axiom.h"
#include "util_time.h"

typedef struct _nrintbuf_t nrbuf_t;

/*
 * Purpose : Create a general-purpose extensible buffer.
 *
 * Params  : 1. The estimated required size. If <= 0, defaults to 1K.
 *           2. The amount to extend the memory by each time if more is
 *              required. Defaults to 1/3 parameter 1 if <= 0 was passed.
 *
 * Returns : A pointer to the buffer or NULL on any form of error.
 */
extern nrbuf_t* nr_buffer_create(int estsize, int extsize);

/*
 * Purpose : Returns the number of bytes of data currently in a buffer.
 *
 * Params  : 1. The buffer.
 *
 * Returns : The number of characters written to the buffer, or -1 on error.
 */
extern int nr_buffer_len(const nrbuf_t* bufp);

/*
 * Purpose : Return a pointer to the first available byte in a buffer, if any.
 *
 * Params  : 1. The buffer.
 *
 * Returns : A constant pointer into the buffer, or NULL if there is no data in
 *           the buffer OR there is an error.
 */
extern const void* nr_buffer_cptr(const nrbuf_t* bufp);

/*
 * Purpose : Reset a buffer pointer to begin using the buffer anew.
 *
 * Params  : 1. The buffer to reset.
 */
extern void nr_buffer_reset(nrbuf_t* bufp);

/*
 * Purpose : Add data to the end of the buffer. Allocates more space if needed.
 *
 * Params  : 1. The buffer to add to.
 *           2. Pointer to the data to add or NULL if data was added manually.
 *           3. Length of the data to add.
 *
 * Notes   : This will use as much space as possible before reallocating for
 *           more. So, if you had previously used space out of the buffer and
 *           the start pointer isn't the beginning of the buffer, this function
 *           will move the data down to make room for new data. Therefore, you
 *           must call nr_buffer_cptr() after calling this function in order
 *           to get a valid pointer to the start of the data.
 */
extern void nr_buffer_add(nrbuf_t* bp, const void* data, int dlen);

/*
 * Purpose : Add a string to the end of a buffer, escaping it as a JSON string.
 *
 * Params  : 1. The buffer to add to.
 *           2. The raw string that will be JSON escaped and added.
 */
extern void nr_buffer_add_escape_json(nrbuf_t* bufp, const char* raw_string);

/*
 * Purpose : Either copy data out of the buffer to some other area of memory
 *           or adjust the internal pointers of the buffer if you have used
 *           data from the buffer.
 *
 * Params  : 1. The buffer to adjust.
 *           2. Where to store the data. Can be NULL if you just want to
 *              inform the buffer that you have used data directly out of a
 *              pointer into the buffer.
 *           3. The number of bytes to be copied / used.
 *
 * Returns : The actual number of bytes copied / used, or -1 on failure. This
 *           can be less than the third parameter if the buffer does not hold
 *           sufficient bytes to honor the request.
 *
 * Notes   : It is a good idea to inform the buffer when you have used data
 *           from the pointer obtained by nr_buffer_cptr() for example, as it
 *           allows the buffer code to re-use the space and may prevent needing
 *           to allocate more space for the buffer on the heap. Of course, the
 *           downside is that this uses a memmove() which can also be fairly
 *           expensive. However this only happens if you add data after
 *           calling this function.
 */
extern int nr_buffer_use(nrbuf_t* bufp, void* destp, int dlen);

/*
 * Purpose : Dispose of a buffer, releasing all resources.
 *
 * Params  : 1. Address of a buffer.
 */
extern void nr_buffer_destroy(nrbuf_t** bufp);

/*
 * Purpose : Write various data types to a buffer.
 *
 * Params  : 1. The buffer to write to.
 *           2. The value to write.
 */
extern void nr_buffer_write_uint32_t_le(nrbuf_t* bufp, uint32_t val);

extern void nr_buffer_write_uint64_t_as_text(nrbuf_t* bufp, uint64_t val);

/*
 * Purpose : Read various data types from a buffer.
 *
 * Params  : 1. The buffer to read from.
 *           2. Pointer to the data type to store.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_buffer_read_uint32_t_le(nrbuf_t* bufp, uint32_t* val);

/*
 * Purpose : Peek at the last char in the buffer
 *
 * Params : 1. The buffer to read from.
 *
 * Return : The last byte in the buffer cast to a char.
 */
extern char nr_buffer_peek_end(nrbuf_t* bufp);

#endif /* UTIL_BUFFER_HDR */
