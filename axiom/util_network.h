/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a number of small utility functions useful for using the
 * various network interfaces available. It's primary role is to provide a
 * single place in the code base where common tasks are done, so that the code
 * can be more reliably tested, and in case we need to do things slightly
 * differently on various operating systems, that we only have a single place
 * where there is pre-processor conditional code.
 *
 * The usage of the word "network" is slightly misleading. Although an actual
 * network connection is the primary usage of these functions most of them will
 * actually work on any type of file descriptor (for example on a pipe).
 */

#ifndef UTIL_NETWORK_HDR
#define UTIL_NETWORK_HDR

#include <stddef.h>
#include <stdint.h>

#include "nr_axiom.h"
#include "util_buffer.h"
#include "util_time.h"

/*
 * This number allows for future versioning of the protocol with the daemon.
 */
#define NR_PREAMBLE_FORMAT 2

/*
 * In little endian format:
 * 4 bytes for the message length (not including preamble)
 * 4 bytes for the protocol (currently always NR_PREAMBLE_FORMAT)
 */
#define NR_PROCOTOL_PREAMBLE_LENGTH 8

/*
 * This is the maximum length for data sent to and from the daemon.
 * Experimentally, metrics are around 100 bytes each, and the transaction trace
 * can be as large as 1 or 2 megabytes.
 */
#define NR_PROTOCOL_CMDLEN_MAX_BYTES (32 * 1024 * 1024)

/*
 * Purpose : Write a message to a file descriptor.
 *
 * Params  : 1. The destination.
 *           2. The message body.
 *           3. Length of the message body in bytes.
 *           4. The write deadline or zero for none. The deadline should
 *              be expressed as a point in time (i.e. absolute) rather than
 *              a timeout.
 *
 * Returns : NR_SUCCESS if the complete message was sent; otherwise,
 *           NR_FAILURE.
 *
 * Notes   : This function shall set errno to ETIMEDOUT and return NR_FAILURE
 *           if the complete message could not be written prior to the
 *           deadline.
 */
extern nr_status_t nr_write_message(int fd,
                                    const void* buf,
                                    size_t len,
                                    nrtime_t deadline);

/*
 * Purpose : Write to a file descriptor with an optional deadline.
 *
 * Params  : 1. The destination.
 *           2. The data to write.
 *           3. The number of bytes to write.
 *           4. The deadline for the write or zero for none. The deadline
 *              should be a point in time (i.e. absolute) rather than a
 *              timeout.
 *
 * Returns : NR_SUCCESS if all of the data was written; otherwise, NR_FAILURE.
 *
 * Notes   : This function shall set errno to ETIMEDOUT and return NR_FAILURE
 *           if the data could not be written prior to the deadline.
 */
extern nr_status_t nr_write_full(int fd,
                                 const void* buf,
                                 size_t len,
                                 nrtime_t deadline);

typedef enum _nr_network_status_t {
  /*
   * The call resulted in an error other than EAGAIN/EWOULDBLOCK/EINTR.
   * Some data may have been read.  The caller should check the buffer.
   */
  NR_NETWORK_STATUS_ERROR = -1,
  /*
   * The call resulted in an EOF.  Some data may have been read.  The caller
   * should check the buffer.
   */
  NR_NETWORK_STATUS_EOF = 0,
  /*
   * The called returned success or EAGAIN/EWOULDBLOCK.  Data may or may not
   * have been read.  The caller should check the buffer.
   */
  NR_NETWORK_STATUS_SUCCESS = 1
} nr_network_status_t;

/*
 * Purpose : Try to read a certain number of bytes into a buffer from a file
 *           descriptor until success or the timeout has elapsed. If a deadline
 *           is provided, it will be observed, otherwise it will block until all
 *           the data has been read.
 *
 * Params  : 1. The fd of interest.
 *           2. The number of bytes to read.
 *           3. The read deadline or zero for none. The deadline should
 *              be expressed as a point in time (i.e. absolute) rather than
 *              a timeout.
 *
 * Returns : A buffer containing all nbytes on success, and NULL on failure.
 */
extern nrbuf_t* nrn_read(int fd, int nbytes, nrtime_t deadline);

/*
 * Purpose : Verify that the inter-process communication message preamble
 *           is correct.
 *
 * Params  : 1. A buffer containing the complete protocol preamble.
 *           2. Pointer to the location to return message length. Required.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_protocol_parse_preamble(nrbuf_t* buf,
                                              uint32_t* return_length);

/*
 * Purpose : Wait for a reply from the daemon to a worker message previously
 *           sent.
 *
 * Params  : 1. The file descriptor to wait for the reply on.
 *           2. The read deadline or zero for none.  The deadline should
 *              be expressed as a point in time (i.e. absolute) rather than
 *              a timeout.
 *
 * Returns : A buffer on success, and NULL on failure.
 */
extern nrbuf_t* nr_network_receive(int fd, nrtime_t deadline);

/*
 * Purpose : Set a file descriptor to be non-blocking.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_network_set_non_blocking(int fd);

/*
 * Purpose : Write the protocol preamble to a buffer.
 *
 * Notes   : This function should only be called by nr_network_send, it is
 *           exported here for testing.  The preamble will be appended to the
 *           end of the buffer, which will be grown if necessary.
 */
extern void nr_protocol_write_preamble(nrbuf_t* buf, uint32_t datalen);

#endif /* UTIL_NETWORK_HDR */
