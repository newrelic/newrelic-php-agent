/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with MySQLi link metadata.
 */
#ifndef NR_MYSQLI_METADATA_PRIVATE_HDR
#define NR_MYSQLI_METADATA_PRIVATE_HDR

#include "nr_mysqli_metadata.h"
#include "util_object.h"

/*
 * The required size of the metadata ID string, defined as the number of
 * characters required to represent a uint64_t (20) plus a null terminator.
 */
#define NR_MYSQLI_METADATA_ID_SIZE 21

/*
 * The metadata repository structure. At present, all metadata is stored within
 * an nrobj_t.
 */
struct _nr_mysqli_metadata_t {
  nrobj_t* links; /* a hash, keyed by link ID, with values that are
                     themselves hashes of metadata */
};

/*
 * Purpose : Create or get the metadata for a MySQLi link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *
 * Returns : A mutable metadata object, or NULL if an error occurred. This
 *           object will need to deleted once freed.
 */
extern nrobj_t* nr_mysqli_metadata_create_or_get(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle);

/*
 * Purpose : Generate an ID for a MySQLi link. The ID will be a NULL terminated
 * string that can be provided as a key to nrobj_t functions.
 *
 * Params  : 1. The MySQLi link handle.
 *           2. The string to place the ID in. This needs to have been
 *              allocated by the caller, and is owned by the caller, and must
 *              be of size NR_MYSQLI_METADATA_ID_SIZE or larger.
 */
extern void nr_mysqli_metadata_id(nr_mysqli_metadata_link_handle_t handle,
                                  char out[NR_MYSQLI_METADATA_ID_SIZE]);

/*
 * Purpose : Save the metadata for a MySQLi link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The metadata.
 */
extern void nr_mysqli_metadata_save(nr_mysqli_metadata_t* metadata,
                                    nr_mysqli_metadata_link_handle_t handle,
                                    const nrobj_t* link);

#endif /* NR_MYSQLI_METADATA_PRIVATE_HDR */
