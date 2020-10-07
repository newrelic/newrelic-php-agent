/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_mysqli_metadata.h"
#include "nr_mysqli_metadata_private.h"
#include "util_memory.h"
#include "util_object.h"

static inline void nr_mysqli_metadata_add_property(nrobj_t* link,
                                                   const char* name,
                                                   const char* value) {
  if (value) {
    nro_set_hash_string(link, name, value);
  }
}

nr_mysqli_metadata_t* nr_mysqli_metadata_create(void) {
  nr_mysqli_metadata_t* metadata = NULL;

  metadata = (nr_mysqli_metadata_t*)nr_malloc(sizeof(nr_mysqli_metadata_t));
  metadata->links = nro_new_hash();

  return metadata;
}

void nr_mysqli_metadata_destroy(nr_mysqli_metadata_t** metadata_ptr) {
  nr_mysqli_metadata_t* metadata;

  if ((NULL == metadata_ptr) || (NULL == *metadata_ptr)) {
    return;
  }

  metadata = *metadata_ptr;
  nro_delete(metadata->links);
  nr_realfree((void**)metadata_ptr);
}

nr_status_t nr_mysqli_metadata_get(const nr_mysqli_metadata_t* metadata,
                                   nr_mysqli_metadata_link_handle_t handle,
                                   nr_mysqli_metadata_link_t* link) {
  const nrobj_t* link_obj;
  char id[NR_MYSQLI_METADATA_ID_SIZE];

  if ((NULL == metadata) || (NULL == link)) {
    return NR_FAILURE;
  }

  nr_mysqli_metadata_id(handle, id);
  link_obj = nro_get_hash_hash(metadata->links, id, NULL);
  if (NULL == link_obj) {
    return NR_FAILURE;
  }

  link->host = nro_get_hash_string(link_obj, "host", NULL);
  link->user = nro_get_hash_string(link_obj, "user", NULL);
  link->password = nro_get_hash_string(link_obj, "password", NULL);
  link->database = nro_get_hash_string(link_obj, "database", NULL);
  link->socket = nro_get_hash_string(link_obj, "socket", NULL);
  link->port = (uint16_t)nro_get_hash_int(link_obj, "port", NULL);
  link->flags = (long)nro_get_hash_long(link_obj, "flags", NULL);
  link->options = nro_get_hash_value(link_obj, "options", NULL);

  return NR_SUCCESS;
}

nr_status_t nr_mysqli_metadata_set_connect(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* host,
    const char* user,
    const char* password,
    const char* database,
    uint16_t port,
    const char* socket,
    long flags) {
  nrobj_t* link;

  link = nr_mysqli_metadata_create_or_get(metadata, handle);
  if (NULL == link) {
    return NR_FAILURE;
  }

  nr_mysqli_metadata_add_property(link, "host", host);
  nr_mysqli_metadata_add_property(link, "user", user);
  nr_mysqli_metadata_add_property(link, "password", password);
  nr_mysqli_metadata_add_property(link, "database", database);
  nr_mysqli_metadata_add_property(link, "socket", socket);
  nro_set_hash_int(link, "port", (int)port);
  nro_set_hash_long(link, "flags", (int64_t)flags);

  nr_mysqli_metadata_save(metadata, handle, link);

  nro_delete(link);

  return NR_SUCCESS;
}

nr_status_t nr_mysqli_metadata_set_database(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    const char* database) {
  nrobj_t* link;

  if (NULL == database) {
    return NR_FAILURE;
  }

  link = nr_mysqli_metadata_create_or_get(metadata, handle);
  if (NULL == link) {
    return NR_FAILURE;
  }

  nr_mysqli_metadata_add_property(link, "database", database);
  nr_mysqli_metadata_save(metadata, handle, link);

  nro_delete(link);

  return NR_SUCCESS;
}

nr_status_t nr_mysqli_metadata_set_option(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle,
    long option,
    const char* value) {
  nrobj_t* link;
  nrobj_t* option_hash;
  nrobj_t* options_dup;
  const nrobj_t* options_orig;

  if (NULL == value) {
    return NR_FAILURE;
  }

  link = nr_mysqli_metadata_create_or_get(metadata, handle);
  if (NULL == link) {
    return NR_FAILURE;
  }

  options_orig = nro_get_hash_array(link, "options", NULL);
  if (options_orig) {
    options_dup = nro_copy(options_orig);
  } else {
    options_dup = nro_new_array();
  }

  option_hash = nro_new_hash();
  nro_set_hash_long(option_hash, "option", (int64_t)option);
  nro_set_hash_string(option_hash, "value", value);

  nro_set_array(options_dup, 0, option_hash);
  nro_set_hash(link, "options", options_dup);

  nr_mysqli_metadata_save(metadata, handle, link);

  nro_delete(link);
  nro_delete(option_hash);
  nro_delete(options_dup);

  return NR_SUCCESS;
}

nrobj_t* nr_mysqli_metadata_create_or_get(
    nr_mysqli_metadata_t* metadata,
    nr_mysqli_metadata_link_handle_t handle) {
  const nrobj_t* found;
  char id[NR_MYSQLI_METADATA_ID_SIZE];

  if (NULL == metadata) {
    return NULL;
  }

  nr_mysqli_metadata_id(handle, id);
  found = nro_get_hash_hash(metadata->links, id, NULL);
  if (found) {
    return nro_copy(found);
  }

  return nro_new_hash();
}

void nr_mysqli_metadata_id(nr_mysqli_metadata_link_handle_t handle,
                           char out[NR_MYSQLI_METADATA_ID_SIZE]) {
  if (NULL == out) {
    return;
  }

  snprintf(out, NR_MYSQLI_METADATA_ID_SIZE, NR_UINT64_FMT, handle);
}

void nr_mysqli_metadata_save(nr_mysqli_metadata_t* metadata,
                             nr_mysqli_metadata_link_handle_t handle,
                             const nrobj_t* link) {
  char id[NR_MYSQLI_METADATA_ID_SIZE];

  if ((NULL == metadata) || (NULL == link)
      || (NR_OBJECT_HASH != nro_type(link))) {
    return;
  }

  nr_mysqli_metadata_id(handle, id);
  nro_set_hash(metadata->links, id, link);
}
