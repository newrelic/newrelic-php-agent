/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with MySQLi link metadata.
 */
#ifndef NR_MYSQLI_METADATA_HDR
#define NR_MYSQLI_METADATA_HDR

#include <stdint.h>

#include "util_object.h"

/*
 * The type used to represent handles. These are PHP object handles (unsigned
 * int in PHP 5; uint32_t in PHP 7). We'll use a uint64_t, since that
 * encapsulates both possibilities.
 */
typedef uint64_t nr_mysqli_metadata_link_handle_t;

/*
 * Forward declaration of the metadata repository.
 */
typedef struct _nr_mysqli_metadata_t nr_mysqli_metadata_t;

/*
 * A structure to hold the metadata for a single link. All fields (except port
 * and flags) may be NULL if they were omitted.
 *
 * The options field is handled somewhat differently: it will be an array of
 * hashes, each of which will have two elements: "option" (which is the numeric
 * value of the options that was set via mysqli_options() as a long), and
 * "value" (which is the string representation of the value). If the user never
 * called mysqli_options(), this array will exist but be empty.
 */
typedef struct _nr_mysqli_metadata_link_t {
  const char* host;
  const char* user;
  const char* password;
  const char* database;
  uint16_t port;
  const char* socket;
  long flags;
  const nrobj_t* options;
} nr_mysqli_metadata_link_t;

/*
 * Purpose : Create a new metadata repository.
 *
 * Returns : A newly allocated metadata repository.
 */
extern nr_mysqli_metadata_t* nr_mysqli_metadata_create(void);

/*
 * Purpose : Destroy a metadata repository.
 *
 * Params  : 1. A pointer to a metadata repository.
 */
extern void nr_mysqli_metadata_destroy(nr_mysqli_metadata_t** metadata_ptr);

/*
 * Purpose : Retrieve the metadata for a given link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. A pointer to a link structure that will receive the
 *              link metadata.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, then the
 *           structure pointed to by link will not be modified.
 */
extern nr_status_t nr_mysqli_metadata_get(
    const nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    nr_mysqli_metadata_link_t* link);

/*
 * Purpose : Set the link parameters for a given link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The database host, or NULL if omitted.
 *           4. The database user, or NULL if omitted.
 *           5. The database password, or NULL if omitted.
 *           6. The selected database, or NULL if omitted.
 *           7. The database port, or 0 if omitted.
 *           8. The database socket, or NULL if omitted.
 *           9. mysqli_real_connect flags, or 0 if omitted.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_mysqli_metadata_set_connect(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* host,
    const char* user,
    const char* password,
    const char* database,
    uint16_t port,
    const char* socket,
    long flags);

/*
 * Purpose : Set the current database for a given link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The new database name, or NULL if omitted.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_mysqli_metadata_set_database(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* database);

/*
 * Purpose : Set a generic mysqli_options() option.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The option being set.
 *           4. The value of the option being set.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_mysqli_metadata_set_option(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    long option,
    const char* value);

#endif /* NR_MYSQLI_METADATA_HDR */
