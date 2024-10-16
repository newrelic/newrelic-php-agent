/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions that are useful when tracking and instrumenting
 * MySQLi connections and queries. Although created as part of the explain plan
 * project, these functions are explicitly not explain plan related: those are
 * in agent/php_explain_mysqli.[ch].
 *
 * These functions are not currently unit tested.
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_datastore.h"
#include "php_hash.h"
#include "php_mysqli.h"
#include "php_mysqli_private.h"
#include "fw_support.h"
#include "nr_mysqli_metadata.h"
#include "util_memory.h"

/*
 * Purpose : Issue a mysqli::real_connect() call based on the given metadata.
 *
 * Params  : 1. The mysqli object to connect.
 *           2. The connection metadata.
 *
 * Returns : NR_SUCCESS if connection succeeded; NR_FAILURE otherwise.
 */
static nr_status_t nr_php_mysqli_link_real_connect(
    zval* link,
    const nr_mysqli_metadata_link_t* metadata TSRMLS_DC);

/*
 * Purpose : Issue a mysqli::options() call based on the given option.
 *
 * Params  : 1. The mysqli object to set the option on.
 *           2. The option constant.
 *           3. The option value.
 *
 * Returns : NR_SUCCESS if the option was set successfully; NR_FAILURE
 *           otherwise.
 */
static nr_status_t nr_php_mysqli_link_set_option(zval* link,
                                                 long option,
                                                 const char* value TSRMLS_DC);

/*
 * Purpose : Create a blank query metadata array.
 *
 * Returns : A new array.
 */
static zval* nr_php_mysqli_query_create(void);

/*
 * Purpose : Find the metadata for the given mysqli_stmt object handle.
 *
 * Params  : 1. The mysqli_stmt object handle to search for.
 *
 * Returns : The metadata array, or NULL if no metadata exists.
 */
static zval* nr_php_mysqli_query_find(nr_php_object_handle_t handle TSRMLS_DC);

/*
 * Purpose : Find the metadata for the given mysqli_stmt object handle. If no
 *           metadata exists, create a blank array and return that.
 *
 * Params  : 1. The mysqli_stmt object handle to search for.
 *
 * Returns : The metadata array.
 */
static zval* nr_php_mysqli_query_find_or_create(
    nr_php_object_handle_t handle TSRMLS_DC);

zval* nr_php_mysqli_link_duplicate(zval* orig TSRMLS_DC) {
  zval* dup = NULL;
  int i;
  nr_mysqli_metadata_link_t metadata;

  if (!nr_php_mysqli_zval_is_link(orig TSRMLS_CC)) {
    return NULL;
  }

  if (NR_FAILURE
      == nr_mysqli_metadata_get(NRTXNGLOBAL(mysqli_links), Z_OBJ_HANDLE_P(orig),
                                &metadata)) {
    return NULL;
  }

  /*
   * We have to use mysqli_init, mysqli::options and mysqli::real_connect, as
   * that's the only way to provide every possible parameter.
   */
  dup = nr_php_call(NULL, "mysqli_init");
  if (NULL == dup) {
    return NULL;
  }

  if (metadata.options) {
    for (i = 1; i <= nro_getsize(metadata.options); i++) {
      long option;
      const nrobj_t* option_meta = NULL;
      const char* value = NULL;

      option_meta = nro_get_array_hash(metadata.options, i, NULL);
      if (NULL == option_meta) {
        nr_php_zval_free(&dup);
        return NULL;
      }

      option = (long)nro_get_hash_long(option_meta, "option", 0);
      value = nro_get_hash_string(option_meta, "value", NULL);

      if (NR_FAILURE
          == nr_php_mysqli_link_set_option(dup, option, value TSRMLS_CC)) {
        nr_php_zval_free(&dup);
        return NULL;
      }
    }
  }

  if (NR_FAILURE == nr_php_mysqli_link_real_connect(dup, &metadata TSRMLS_CC)) {
    nr_php_zval_free(&dup);
    return NULL;
  }

  return dup;
}

zval* nr_php_mysqli_query_get_link(nr_php_object_handle_t handle TSRMLS_DC) {
  zval* link;
  zval* metadata;

  metadata = nr_php_mysqli_query_find(handle TSRMLS_CC);
  if (NULL == metadata) {
    return NULL;
  }

  link = nr_php_zend_hash_find(Z_ARRVAL_P(metadata), "link");
  if (!nr_php_mysqli_zval_is_link(link TSRMLS_CC)) {
    return NULL;
  }

  return link;
}

char* nr_php_mysqli_query_get_query(nr_php_object_handle_t handle TSRMLS_DC) {
  const zval* metadata;
  const zval* query;

  metadata = nr_php_mysqli_query_find(handle TSRMLS_CC);
  if (NULL == metadata) {
    return NULL;
  }

  query = nr_php_zend_hash_find(Z_ARRVAL_P(metadata), "query");
  if (!nr_php_is_zval_non_empty_string(query)) {
    return NULL;
  }

  return nr_strndup(Z_STRVAL_P(query), Z_STRLEN_P(query));
}

nr_status_t nr_php_mysqli_query_rebind(nr_php_object_handle_t handle,
                                       zval* dest TSRMLS_DC) {
  zval* args = NULL;
  zval** bind_args = NULL;
  zval* format = NULL;
  zend_ulong i;
  zval* metadata = NULL;
  zend_ulong num_args = 0;
  nr_status_t result = NR_FAILURE;
  zval* retval = NULL;

  if (0 == nr_php_mysqli_zval_is_stmt(dest TSRMLS_CC)) {
    goto leave;
  }

  /*
   * In effect, this function will pull the format and arguments given to the
   * original mysqli_stmt::bind_param call in our metadata hash (identified by
   * the mysqli_stmt object handle), and then call mysqli_stmt::bind_param on
   * the destination mysqli_stmt in a manner that should be identical.
   */

  metadata = nr_php_mysqli_query_find(handle TSRMLS_CC);
  if (NULL == metadata) {
    goto leave;
  }

  format = nr_php_zend_hash_find(Z_ARRVAL_P(metadata), "bind_format");
  if (NULL == format) {
    /*
     * The format not existing is a success case: it just means we have nothing
     * to rebind.
     */
    result = NR_SUCCESS;
    goto leave;
  }

  if (!nr_php_is_zval_non_empty_string(format)) {
    /*
     * On the other hand, if zend_hash_find succeeded but we didn't get a valid
     * format, that's a problem.
     */
    goto leave;
  }

  args = nr_php_zend_hash_find(Z_ARRVAL_P(metadata), "bind_args");
  if (!nr_php_is_zval_valid_array(args)) {
    /*
     * Unlike the above, if bind_format exists but bind_args doesn't, then
     * something went wrong in nr_php_mysqli_query_set_bind_params, and we're
     * in an unsafe state to continue.
     */
    goto leave;
  }

  /*
   * We have to build up bind_args, which contains the format for
   * mysqli_stmt::bind_param and then a variable number of arguments.
   */
  num_args = zend_hash_num_elements(Z_ARRVAL_P(args));
  bind_args = (zval**)nr_calloc(num_args + 1, sizeof(zval*));
  bind_args[0] = format;
  for (i = 0; i < num_args; i++) {
    zval* arg = nr_php_zend_hash_index_find(Z_ARRVAL_P(args), i);

    if (NULL == arg) {
      goto leave;
    }

    bind_args[i + 1] = arg;
  }

  /*
   * Actually call mysqli_stmt::bind_param to bind the parameters.
   */
  retval = nr_php_call_user_func(dest, "bind_param", num_args + 1,
                                 bind_args TSRMLS_CC);
  if (!nr_php_is_zval_true(retval)) {
    goto leave;
  }

  result = NR_SUCCESS;

leave:
  nr_free(bind_args);
  nr_php_zval_free(&retval);

  return result;
}

nr_status_t nr_php_mysqli_query_set_bind_params(nr_php_object_handle_t handle,
                                                const char* format,
                                                size_t format_len,
                                                size_t args_len,
                                                zval** args TSRMLS_DC) {
  char* format_dup = NULL;
  size_t i;
  zval* metadata = NULL;
  zval* saved_args = NULL;

  if ((NULL == format) || (0 == args_len) || (NULL == args)) {
    return NR_FAILURE;
  }

  /*
   * We won't implicitly create metadata here: if there isn't already a link
   * and query persisted, then the query is inexplicable regardless.
   */
  metadata = nr_php_mysqli_query_find(handle TSRMLS_CC);
  if (NULL == metadata) {
    return NR_FAILURE;
  }

  saved_args = nr_php_zval_alloc();
  array_init(saved_args);
  for (i = 0; i < args_len; i++) {
    zval* arg = args[i];

    if (NULL == arg) {
      nr_php_zval_free(&saved_args);
      return NR_FAILURE;
    }

    /*
     * MySQLi binds arguments by reference, so we shall do the exact same. We
     * have to increment the refcount: while this means that we'll cling onto
     * the zval until the end of the request, not doing so results in segfaults
     * if the argument is an object, since the destruction of the metadata zval
     * at RSHUTDOWN will cause the Zend Engine to dereference a previously
     * destroyed pointer in the object store.
     */
    Z_ADDREF_P(arg);

    add_next_index_zval(saved_args, arg);
  }

  nr_php_add_assoc_zval(metadata, "bind_args", saved_args);
  nr_php_zval_free(&saved_args);

  format_dup = nr_strndup(format, format_len);
  nr_php_add_assoc_stringl(metadata, "bind_format", format_dup, format_len);
  nr_free(format_dup);

  return NR_SUCCESS;
}

nr_status_t nr_php_mysqli_query_set_link(nr_php_object_handle_t query_handle,
                                         zval* link TSRMLS_DC) {
  zval* metadata = NULL;

  metadata = nr_php_mysqli_query_find_or_create(query_handle TSRMLS_CC);
  if ((NULL == metadata) || !nr_php_mysqli_zval_is_link(link TSRMLS_CC)) {
    return NR_FAILURE;
  }

  nr_php_add_assoc_zval(metadata, "link", link);

  return NR_SUCCESS;
}

nr_status_t nr_php_mysqli_query_set_query(nr_php_object_handle_t handle,
                                          const char* query,
                                          int query_len TSRMLS_DC) {
  char* query_dup = NULL;
  zval* metadata = NULL;

  if (NULL == query) {
    return NR_FAILURE;
  }

  metadata = nr_php_mysqli_query_find_or_create(handle TSRMLS_CC);
  if (NULL == metadata) {
    return NR_FAILURE;
  }

  query_dup = nr_strndup(query, query_len);
  nr_php_add_assoc_stringl(metadata, "query", query_dup, query_len);
  nr_free(query_dup);

  /*
   * A new query means new bind parameters, so let's get rid of whatever's here.
   * We'll ignore the return values, since if the keys don't already exist no
   * harm is done.
   */
  nr_php_zend_hash_del(Z_ARRVAL_P(metadata), "bind_args");
  nr_php_zend_hash_del(Z_ARRVAL_P(metadata), "bind_format");

  return NR_SUCCESS;
}

nr_status_t nr_php_mysqli_query_clear_query(nr_php_object_handle_t handle) {
  zval* metadata = NULL;

  /* If a metadata entry exists then clear the "query" tag from it.
   *  If an entry does not exist then nothing needs to be done.
   */
  metadata = nr_php_mysqli_query_find(handle);
  if (NULL == metadata) {
    return NR_FAILURE;
  }

  /* Clear the "query" element */
  nr_php_zend_hash_del(Z_ARRVAL_P(metadata), "query");

  /*
   * Since the query is cleared so must the bind parameters, so let's get rid of
   * whatever's here. We'll ignore the return values, since if the keys don't
   * already exist no harm is done.
   */
  nr_php_zend_hash_del(Z_ARRVAL_P(metadata), "bind_args");
  nr_php_zend_hash_del(Z_ARRVAL_P(metadata), "bind_format");

  return NR_SUCCESS;
}

int nr_php_mysqli_zval_is_link(const zval* zv TSRMLS_DC) {
  if (NULL == zv) {
    return 0;
  }

  return nr_php_object_instanceof_class(zv, "mysqli" TSRMLS_CC);
}

int nr_php_mysqli_zval_is_stmt(const zval* zv TSRMLS_DC) {
  if (NULL == zv) {
    return 0;
  }

  return nr_php_object_instanceof_class(zv, "mysqli_stmt" TSRMLS_CC);
}

static nr_status_t nr_php_mysqli_link_real_connect(
    zval* link,
    const nr_mysqli_metadata_link_t* metadata TSRMLS_DC) {
  zend_ulong argc = 0;
  zend_ulong arg_required = 0;
  zval* argv[7] = {0};
  zend_ulong i;
  zval* retval = NULL;

#define ADD_IF_INT_SET(null_ok, args, argc, value) \
  if (value) {                                    \
    args[argc] = nr_php_zval_alloc();             \
    ZVAL_LONG(args[argc], value);                 \
    argc++;                                       \
  } else if (true == null_ok) {                   \
    args[argc] = nr_php_zval_alloc();             \
    ZVAL_NULL(args[argc]);                        \
    argc++;                                       \
  }

#define ADD_IF_STR_SET(null_ok, args, argc, value) \
  if (value) {                                    \
    args[argc] = nr_php_zval_alloc();             \
    nr_php_zval_str(args[argc], value);           \
    argc++;                                       \
  } else if (true == null_ok) {                   \
    args[argc] = nr_php_zval_alloc();             \
    ZVAL_NULL(args[argc]);                        \
    argc++;                                       \
  }

  ADD_IF_STR_SET(false, argv, argc,
                 nr_php_mysqli_strip_persistent_prefix(metadata->host));
  ADD_IF_STR_SET(false, argv, argc, metadata->user);
  ADD_IF_STR_SET(false, argv, argc, metadata->password);

  /*
   * We can only add the remaining metadata fields if we already have three
   * arguments (host, user and password) above, lest we accidentally set the
   * wrong positional argument to something it doesn't mean.  Note, prior
   * to 7.4 not all args are nullable.
   */
  arg_required = argc;
  if (argc == 3) {
    ADD_IF_STR_SET(true, argv, argc, metadata->database);
    ADD_IF_INT_SET(true, argv, argc, metadata->port);
    ADD_IF_STR_SET(true, argv, argc, metadata->socket);
    ADD_IF_INT_SET(false, argv, argc, metadata->flags);
  }  retval = nr_php_call_user_func(link, "real_connect", argc, argv TSRMLS_CC);

  for (i = 0; i < argc; i++) {
    nr_php_zval_free(&argv[i]);
  }

  if (!nr_php_is_zval_true(retval)) {
    nr_php_zval_free(&retval);
    return NR_FAILURE;
  }

  nr_php_zval_free(&retval);

  /*
   * If we didn't specify the database in the connection parameters, we need to
   * call mysqli::select_db here.
   */
  if (metadata->database && (arg_required < 3)) {
    zval* database = nr_php_zval_alloc();

    nr_php_zval_str(database, metadata->database);

    retval = nr_php_call(link, "select_db", database);

    nr_php_zval_free(&database);
    if (!nr_php_is_zval_true(retval)) {
      nr_php_zval_free(&retval);
      return NR_FAILURE;
    }

    nr_php_zval_free(&retval);
  }

  return NR_SUCCESS;
}

static nr_status_t nr_php_mysqli_link_set_option(zval* link,
                                                 long option,
                                                 const char* value TSRMLS_DC) {
  zval* option_zv = nr_php_zval_alloc();
  zval* retval = NULL;
  nr_status_t status = NR_SUCCESS;
  zval* value_zv = nr_php_zval_alloc();

  ZVAL_LONG(option_zv, option);
  nr_php_zval_str(value_zv, value);

  retval = nr_php_call(link, "options", option_zv, value_zv);
  if (!nr_php_is_zval_true(retval)) {
    status = NR_FAILURE;
  }

  nr_php_zval_free(&option_zv);
  nr_php_zval_free(&retval);
  nr_php_zval_free(&value_zv);

  return status;
}

static zval* nr_php_mysqli_query_create(void) {
  zval* metadata = nr_php_zval_alloc();

  /*
   * The query metadata is stored as a native PHP array, rather than in a C
   * struct. This has been done for two primary reasons:
   *
   * 1. It means that we can use the Zend Engine's own implementations of zval
   *    destruction and reference counting, since we have to keep a reference
   *    to any bound parameters rather than copying them (as they may change
   *    between being bound and the query being executed).
   *
   * 2. PHP 7 is likely to remove the ability to store arbitrary structures in
   *    HashTable instances, so we might as well start preparing for that.
   *
   * In a perfect world, this would be an axiom module, but since axiom can't
   * deal with PHP specific types (such as zvals), we'll make do with what we
   * have and write this in reasonably idiomatic PHP extension code instead.
   */

  array_init(metadata);
  return metadata;
}

static zval* nr_php_mysqli_query_find(nr_php_object_handle_t handle TSRMLS_DC) {
  zval* metadata = (zval*)nr_hashmap_index_get(NRTXNGLOBAL(mysqli_queries),
                                               (uint64_t)handle);

  if (!nr_php_is_zval_valid_array(metadata)) {
    return NULL;
  }

  return metadata;
}

static void nr_php_mysqli_query_destroy(zval* query) {
  nr_php_zval_free(&query);
}

static zval* nr_php_mysqli_query_find_or_create(
    nr_php_object_handle_t handle TSRMLS_DC) {
  zval* metadata = NULL;

  if (NULL == NRTXNGLOBAL(mysqli_queries)) {
    NRTXNGLOBAL(mysqli_queries) = nr_hashmap_create(
        (nr_hashmap_dtor_func_t)nr_php_mysqli_query_destroy);
  } else {
    /*
     * See if we already have metadata for this handle.
     */
    metadata = nr_php_mysqli_query_find(handle TSRMLS_CC);
    if (metadata) {
      return metadata;
    }
  }

  /*
   * We don't, so let's create it.
   */
  metadata = nr_php_mysqli_query_create();
  nr_hashmap_index_update(NRTXNGLOBAL(mysqli_queries), (uint64_t)handle,
                          metadata);

  return metadata;
}

char* nr_php_mysqli_default_host() {
  char* host = nr_php_zend_ini_string(NR_PSTR("mysqli.default_host"), 0);

  if (nr_strempty(host)) {
    host = "localhost";
  }

  return host;
}

void nr_php_mysqli_get_host_and_port_path_or_id(const char* host_param,
                                                const zend_long port,
                                                const char* socket,
                                                char** host,
                                                char** port_path_or_id) {
  if ((NULL != *host) || (NULL != *port_path_or_id)) {
    return;
  }

  if (nr_strempty(host_param)) {
    *host = nr_strdup(nr_php_mysqli_default_host());
  } else {
    *host = nr_strdup(host_param);
  }

  if (0 == port) {
    /*
     * See:
     * https://github.com/php/php-src/blob/PHP-5.6/ext/mysqlnd/mysqlnd.c#L978-L979
     */
    *port_path_or_id = nr_strdup(nr_php_mysqli_default_port());
  } else {
    *port_path_or_id = nr_formatf(NR_INT64_FMT, (int64_t)port);
  }

  /*
   * Host, port, and socket are all passed to the mysql/mysqlnd driver.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysql/php_mysql.c#L888-L891
   *
   * If the host is exactly "localhost" use a socket.
   * https://github.com/php/php-src/blob/PHP-5.6/ext/mysqlnd/mysqlnd.c#L961-L967
   */
  if (0 == nr_stricmp(*host, "localhost")) {
    nr_free(*port_path_or_id);
    if (nr_strempty(socket)) {
      *port_path_or_id = nr_strdup(nr_php_mysqli_default_socket());
    } else {
      *port_path_or_id = nr_strdup(socket);
    }
  }
}

nr_datastore_instance_t* nr_php_mysqli_create_datastore_instance(
    const char* host,
    const zend_long port,
    const char* socket,
    const char* database_name) {
  nr_datastore_instance_t* instance = NULL;
  char* actual_host = NULL;
  char* actual_port_path_or_id = NULL;

  nr_php_mysqli_get_host_and_port_path_or_id(host, port, socket, &actual_host,
                                             &actual_port_path_or_id);

  instance = nr_datastore_instance_create(actual_host, actual_port_path_or_id,
                                          database_name);

  nr_free(actual_host);
  nr_free(actual_port_path_or_id);

  return instance;
}

void nr_php_mysqli_save_datastore_instance(const zval* mysqli_obj,
                                           const char* host,
                                           const zend_long port,
                                           const char* socket,
                                           const char* database_name
                                               TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = nr_php_datastore_make_key(mysqli_obj, "mysqli");

  /*
   * We don't check whether we've seen this connection before like we do with
   * the mysql extension. Unlike resources, objects can be reused, so we need to
   * update the hashmap each time we see a connection.
   */
  instance = nr_php_mysqli_create_datastore_instance(host, port, socket,
                                                     database_name);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(key);
}

nr_datastore_instance_t* nr_php_mysqli_retrieve_datastore_instance(
    const zval* mysqli_obj TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = nr_php_datastore_make_key(mysqli_obj, "mysqli");

  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);

  nr_free(key);

  return instance;
}

void nr_php_mysqli_remove_datastore_instance(const zval* mysqli_obj TSRMLS_DC) {
  char* key = nr_php_datastore_make_key(mysqli_obj, "mysqli");

  nr_php_datastore_instance_remove(key TSRMLS_CC);

  nr_free(key);
}

const char* nr_php_mysqli_strip_persistent_prefix(const char* host) {
  if (host && 'p' == host[0] && ':' == host[1]) {
    return host + 2;
  }

  return host;
}
