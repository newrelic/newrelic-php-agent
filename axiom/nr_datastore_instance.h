/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DATASTORE_INSTANCE_HDR
#define NR_DATASTORE_INSTANCE_HDR

typedef struct _nr_datastore_instance_t {
  char* host;
  char* port_path_or_id;
  char* database_name;
} nr_datastore_instance_t;

/*
 * Purpose : Create a datastore instance struct.
 *
 * Params  : 1. The host
 *           2. The port, path, or id
 *           3. The database name
 *
 * Returns : A pointer to the datastore instance structure.
 */
extern nr_datastore_instance_t* nr_datastore_instance_create(
    const char* host,
    const char* port_path_or_id,
    const char* database_name);

/*
 * Purpose : Destroy a datastore instance struct.
 *
 * Params  : 1. A pointer to the struct
 */
extern void nr_datastore_instance_destroy(
    nr_datastore_instance_t** instance_ptr);

/*
 * Purpose : Destroy just the fields within a datastore instance struct. Useful
 *           for datastore instances that were not created with
 *           nr_datastore_instance_create().
 *
 * Params  : 1. The datastore instance.
 */
extern void nr_datastore_instance_destroy_fields(
    nr_datastore_instance_t* instance);

/*
 * Purpose : Determine whether a host is a known local address.
 *
 * Params  : 1. The host
 *
 * Returns : Non-zero if it is a known local address, zero otherwise.
 */
extern int nr_datastore_instance_is_localhost(const char* host);

/*
 * Purpose : Get the host, port_path_or_id, or database name from a datastore
 *           instance struct.
 *
 * Params  : 1. A pointer to the struct
 *
 * Returns : The field's value or NULL if the instance is NULL.
 */
extern const char* nr_datastore_instance_get_host(
    nr_datastore_instance_t* instance);

extern const char* nr_datastore_instance_get_port_path_or_id(
    nr_datastore_instance_t* instance);

extern const char* nr_datastore_instance_get_database_name(
    nr_datastore_instance_t* instance);

/*
 * Purpose : Set the host, port_path_or_id, or database name for a datastore
 *           instance struct.
 *
 * Params  : 1. A pointer to the struct
 *           2. The string value for the field
 */
extern void nr_datastore_instance_set_host(nr_datastore_instance_t* instance,
                                           const char* host);

extern void nr_datastore_instance_set_port_path_or_id(
    nr_datastore_instance_t* instance,
    const char* port_path_or_id);

extern void nr_datastore_instance_set_database_name(
    nr_datastore_instance_t* instance,
    const char* database_name);

#endif /* NR_DATASTORE_INSTANCE_HDR */
