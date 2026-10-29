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
 *
 * The ssl_* fields are the arguments of the last mysqli_ssl_set() call. Each
 * may be NULL if it was omitted or null.
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
  const char* ssl_key;
  const char* ssl_cert;
  const char* ssl_ca;
  const char* ssl_capath;
  const char* ssl_cipher;

  /*
   * Non-zero once nr_mysqli_metadata_set_connect() has run for the link. The
   * other setters can create an entry for a link the agent did not see
   * connect (one opened while not recording): such an entry lacks the host and
   * the user and must not be used to open a connection.
   */
  int complete;
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
 * Purpose : Forget everything saved for a link.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *
 * Notes   : PHP reuses the handle of a freed object, so a new link must not
 *           find what was saved for an earlier one. Call this when a link is
 *           created or closed.
 */
extern void nr_mysqli_metadata_remove(nr_mysqli_metadata_t* metadata,
                                      nr_mysqli_metadata_link_handle_t handle);

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
 * Purpose : Set the user, password and database for a given link, as
 *           mysqli_change_user() does. The database is replaced even when it
 *           is omitted, as the new session has none selected.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The new user.
 *           4. The new password, or NULL if omitted.
 *           5. The new database, or NULL if omitted.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_mysqli_metadata_set_user(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* user,
    const char* password,
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

/*
 * Purpose : Set the mysqli_ssl_set() arguments for a given link. A later call
 *           replaces all of the earlier arguments, as it does in PHP.
 *
 * Params  : 1. The metadata repository.
 *           2. The MySQLi link handle.
 *           3. The key path, or NULL if omitted.
 *           4. The certificate path, or NULL if omitted.
 *           5. The CA certificate path, or NULL if omitted.
 *           6. The CA directory path, or NULL if omitted.
 *           7. The cipher list, or NULL if omitted.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_mysqli_metadata_set_ssl(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* key,
    const char* cert,
    const char* ca,
    const char* capath,
    const char* cipher);

/*
 * Purpose : Copy an existing metadata object.
 *
 * Params  : 1. The metadata object to be copied
 *
 * Returns : A copy of the metadata object. This object will need to be freed
 *           when no longer needed.
 */
extern nr_mysqli_metadata_t* nr_mysqli_metadata_copy(
    nr_mysqli_metadata_t* metadata);

#endif /* NR_MYSQLI_METADATA_HDR */
