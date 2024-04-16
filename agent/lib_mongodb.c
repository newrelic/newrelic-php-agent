/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_execute.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_datastore_instance.h"
#include "nr_segment_datastore.h"
#include "util_logging.h"
#include "util_strings.h"
#include "util_system.h"

#include "lib_mongodb_private.h"

static int nr_mongodb_is_server(const zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(obj,
                                        "MongoDB\\Driver\\Server" TSRMLS_CC);
}

char* nr_mongodb_get_host(zval* server TSRMLS_DC) {
  char* host = NULL;
  zval* host_zval = NULL;

  if (!nr_mongodb_is_server(server TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: MongoDB server does not seem to be a server",
                     __func__);
    return NULL;
  }

  host_zval = nr_php_call(server, "getHost");
  if (nr_php_is_zval_valid_string(host_zval)) {
    /*
     * We got a valid string. We check whether it's empty or localhost;
     * otherwise, we'll just copy it.
     */
    host = Z_STRVAL_P(host_zval);
    if (nr_strempty(host)) {
      host = nr_strdup("unknown");
    } else if (nr_datastore_instance_is_localhost(host)) {
      host = nr_system_get_hostname();
    } else {
      host = nr_strdup(host);
    }
  } else {
    host = nr_strdup("unknown");
  }

  nr_php_zval_free(&host_zval);

  return host;
}

char* nr_mongodb_get_port(zval* server TSRMLS_DC) {
  char* port = NULL;
  zval* port_zval;

  if (!nr_mongodb_is_server(server TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: MongoDB server does not seem to be a server",
                     __func__);
    return NULL;
  }

  port_zval = nr_php_call(server, "getPort");
  if (nr_php_is_zval_valid_integer(port_zval)) {
    port = nr_formatf(NR_INT64_FMT, (int64_t)Z_LVAL_P(port_zval));
  } else {
    port = nr_strdup("unknown");
  }

  nr_php_zval_free(&port_zval);

  return port;
}

void nr_mongodb_get_host_and_port_path_or_id(zval* server,
                                             char** host,
                                             char** port_path_or_id TSRMLS_DC) {
  if (!nr_mongodb_is_server(server TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: MongoDB server does not seem to be a server",
                     __func__);
    return;
  }

  if ((NULL != *host) || (NULL != *port_path_or_id)) {
    return;
  }

  *host = nr_mongodb_get_host(server TSRMLS_CC);

  /*
   * Mongo reports socket files as the host, e.g. /tmp/mongodb-27017.sock, so we
   * just switch it to the port.
   */
  if (*host[0] == '/') {
    *port_path_or_id = *host;
    *host = nr_system_get_hostname();
  } else {
    *port_path_or_id = nr_mongodb_get_port(server TSRMLS_CC);
  }
}

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
  || defined OVERWRITE_ZEND_EXECUTE_DATA
NR_PHP_WRAPPER(nr_mongodb_operation) {
  const char* this_klass = "MongoDB\\Operation\\Executable";
  zval* collection = NULL;
  zval* database = NULL;
  zval* server = NULL;
  zval* this_var = NULL;
  nr_segment_t* segment = NULL;
  nr_datastore_instance_t instance = {
      .host = NULL,
      .port_path_or_id = NULL,
      .database_name = NULL,
  };
  nr_segment_datastore_params_t params = {
    .datastore = {
      .type = NR_DATASTORE_MONGODB,
    },
    .operation = nr_strdup (wraprec->extra),
    .instance = &instance,
    .callbacks = {
      .backtrace = nr_php_backtrace_callback,
    },
  };

  /*
   * We check for the interface all Collection operations extend, rather than
   * their specific class. Not all operations have the properties we need but
   * the ones we hook do (as of mongo-php-library v.1.1).
   */
  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_object_instanceof_class(this_var, this_klass TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: operation is not %s", __func__,
                     this_klass);
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  collection
      = nr_php_get_zval_object_property(this_var, "collectionName" TSRMLS_CC);
  if (nr_php_is_zval_valid_string(collection)) {
    params.collection = Z_STRVAL_P(collection);
  }

  database
      = nr_php_get_zval_object_property(this_var, "databaseName" TSRMLS_CC);
  if (nr_php_is_zval_valid_string(database)) {
    instance.database_name = Z_STRVAL_P(database);
  }

  server = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  nr_mongodb_get_host_and_port_path_or_id(server, &instance.host,
                                          &instance.port_path_or_id TSRMLS_CC);

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  NR_PHP_WRAPPER_CALL;
  nr_segment_datastore_end(&segment, &params);

leave:
  nr_php_arg_release(&server);
  nr_php_scope_release(&this_var);
  nr_free(instance.host);
  nr_free(instance.port_path_or_id);
  nr_free(params.operation);
}
NR_PHP_WRAPPER_END

#else

NR_PHP_WRAPPER(nr_mongodb_operation_before) {
    (void)wraprec;
    nr_segment_t* segment;
    segment = nr_segment_start(NRPRG(txn), NULL, NULL);
    segment->wraprec = auto_segment->wraprec;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_mongodb_operation_after) {
  const char* this_klass = "MongoDB\\Operation\\Executable";
  zval* collection = NULL;
  zval* database = NULL;
  zval* server = NULL;
  zval* this_var = NULL;
  nr_datastore_instance_t instance = {
      .host = NULL,
      .port_path_or_id = NULL,
      .database_name = NULL,
  };
  nr_segment_datastore_params_t params = {
    .datastore = {
      .type = NR_DATASTORE_MONGODB,
    },
    .operation = nr_strdup (wraprec->extra),
    .instance = &instance,
    .callbacks = {
      .backtrace = nr_php_backtrace_callback,
    },
  };

  /*
   * We check for the interface all Collection operations extend, rather than
   * their specific class. Not all operations have the properties we need but
   * the ones we hook do (as of mongo-php-library v.1.1).
   */
  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS);
  if (!nr_php_object_instanceof_class(this_var, this_klass)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: operation is not %s", __func__,
                     this_klass);
    goto leave;
  }

  collection
      = nr_php_get_zval_object_property(this_var, "collectionName");
  if (nr_php_is_zval_valid_string(collection)) {
    params.collection = Z_STRVAL_P(collection);
  }

  database
      = nr_php_get_zval_object_property(this_var, "databaseName");
  if (nr_php_is_zval_valid_string(database)) {
    instance.database_name = Z_STRVAL_P(database);
  }

  server = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS);
  nr_mongodb_get_host_and_port_path_or_id(server, &instance.host,
                                          &instance.port_path_or_id);

  nr_segment_datastore_end(&auto_segment, &params);

leave:
  nr_php_arg_release(&server);
  nr_php_scope_release(&this_var);
  nr_free(instance.host);
  nr_free(instance.port_path_or_id);
  nr_free(params.operation);
}
NR_PHP_WRAPPER_END

#endif /* OAPI */

void nr_mongodb_enable() {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Aggregate::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "aggregate"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\BulkWrite::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "bulkWrite"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Count::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "count"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\CreateIndexes::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "createIndexes"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Delete::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "delete"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Distinct::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "distinct"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\DropCollection::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "dropCollection"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\DropIndexes::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "dropIndexes"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Find::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "find"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\FindAndModify::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "findAndModify"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\InsertMany::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "insertMany"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\InsertOne::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "insertOne"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\ListIndexes::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "listIndexes"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\Update::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "update"
    );

    nr_php_wrap_user_function_before_after_clean_extra(
        NR_PSTR("MongoDB\\Operation\\DatabaseCommand::execute"),
        nr_mongodb_operation_before,
        nr_mongodb_operation_after,
        nr_mongodb_operation_after,
        "databaseCommand"
    );

#else /* Non-OAPI */

  /*
   * We instrument interesting methods on the MongoDB\Collection class via their
   * associated MongoDB\Operation classes.
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\Aggregate::execute"), nr_mongodb_operation,
      "aggregate" TSRMLS_CC);
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\BulkWrite::execute"), nr_mongodb_operation,
      "bulkWrite" TSRMLS_CC);
  nr_php_wrap_user_function_extra(NR_PSTR("MongoDB\\Operation\\Count::execute"),
                                  nr_mongodb_operation, "count" TSRMLS_CC);

  /*
   * This also catches MongoDB\Collection::createIndex
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\CreateIndexes::execute"),
      nr_mongodb_operation, "createIndexes" TSRMLS_CC);

  /*
   * This also catches:
   *     MongoDB\Collection::DeleteMany
   *     MongoDB\Collection::DeleteOne
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\Delete::execute"), nr_mongodb_operation,
      "delete" TSRMLS_CC);

  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\Distinct::execute"), nr_mongodb_operation,
      "distinct" TSRMLS_CC);

  /*
   * This also catches MongoDB\Collection::drop
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\DropCollection::execute"),
      nr_mongodb_operation, "dropCollection" TSRMLS_CC);

  /*
   * This also catches MongoDB\Collection::dropIndex
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\DropIndexes::execute"), nr_mongodb_operation,
      "dropIndexes" TSRMLS_CC);

  /*
   * This also catches MongoDB\Collection::findOne
   */
  nr_php_wrap_user_function_extra(NR_PSTR("MongoDB\\Operation\\Find::execute"),
                                  nr_mongodb_operation, "find" TSRMLS_CC);

  /*
   * This also catches:
   *     MongoDB\Collection::FindOneAndDelete
   *     MongoDB\Collection::FindOneAndReplace
   *     MongoDB\Collection::FindOneAndUpdate
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\FindAndModify::execute"),
      nr_mongodb_operation, "findAndModify" TSRMLS_CC);

  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\InsertMany::execute"), nr_mongodb_operation,
      "insertMany" TSRMLS_CC);
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\InsertOne::execute"), nr_mongodb_operation,
      "insertOne" TSRMLS_CC);
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\ListIndexes::execute"), nr_mongodb_operation,
      "listIndexes" TSRMLS_CC);

  /*
   * This also catches:
   *     MongoDB\Collection::ReplaceOne
   *     MongoDB\Collection::UpdateMany
   *     MongoDB\Collection::UpdateOne
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\Update::execute"), nr_mongodb_operation,
      "update" TSRMLS_CC);

  /*
   * This operation is used by the MongoDB\Database class. Because the operation
   * is scoped to a database, it does not have a collection name property.
   */
  nr_php_wrap_user_function_extra(
      NR_PSTR("MongoDB\\Operation\\DatabaseCommand::execute"),
      nr_mongodb_operation, "databaseCommand" TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "mongodb/mongodb",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
#endif /* OAPI */
}
