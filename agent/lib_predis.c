/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_datastore.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_datastore_instance.h"
#include "nr_segment_datastore.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "lib_predis_private.h"

/*
 * Predis instrumentation
 * ======================
 *
 * Fundamentally, the Predis instrumentation relies on looking for matched
 * pairs of ConnectionInterface::writeRequest() and
 * ConnectionInterface::readResponse() methods.
 *
 * (Note: some of these names are different on pre-1.0 versions of Predis.
 * writeRequest() becomes writeCommand().
 * On Predis 0.7, the interfaces are generally called IFoo
 * instead of FooInterface and may reside in different namespaces than in later
 * versions. For simplicity, this description refers only to the Predis 1.x
 * class and method names, but please refer to the code if you're specifically
 * interested in old versions of Predis.)
 *
 * We need to see both of these methods for timing purposes: in effect,
 * writeRequest() starts the timer, and readResponse() stops it, at which point
 * we have the time it took for the Redis operation. These methods both receive
 * a CommandInterface object: the getId() method on that object allows us to
 * retrieve the literal Redis command name and transform it into an operation
 * name.
 *
 * As a Client object may have more than one connection when clustering is in
 * use, we set up the writeRequest() and readResponse() instrumentation via
 * transient wrapping applied to each connection object as it is accessed. This
 * is accomplished by checking the connection within the Client object after the
 * object is instantiated: if it's an AggregateConnectionInterface, we'll hook
 * that object's getConnection() method to ensure that we instrument each
 * connection as it is used. If it's a normal connection, we instrument the
 * connection then and there.
 *
 * Predis also supports pipelines, where a number of commands are executed in
 * parallel. The agent's limited async support is used to correctly break out
 * each command.
 *
 * As a final quirk, WebdisConnection implements an entirely different
 * connection type to interact with Webdis servers. These objects don't use the
 * underlying writeRequest() and readResponse() API, but instead do their work
 * directly in executeCommand(): as a result, we'll instrument
 * WebdisConnection::executeCommand() to handle that.
 */

/*
 * These defaults come from the Predis documentation at
 * https://github.com/nrk/predis/wiki/Connection-Parameters.
 *
 * Note that the path parameter doesn't have a default: we set it to NULL to get
 * the appropriate default behavior from the datastore instance functions, but
 * in practice an omitted path when the scheme is "unix" will result in an
 * exception being thrown and no object being instantiated regardless.
 */
const zend_long nr_predis_default_database = 0;
const char* nr_predis_default_host = "127.0.0.1";
const char* nr_predis_default_path = NULL;
const zend_long nr_predis_default_port = 6379;

static NR_PHP_WRAPPER_PROTOTYPE(nr_predis_connection_readResponse);
static NR_PHP_WRAPPER_PROTOTYPE(nr_predis_connection_writeRequest);

static void nr_predis_command_destroy(nrtime_t* time) {
  nr_free(time);
}

static inline nr_hashmap_t* nr_predis_get_commands(TSRMLS_D) {
  if (NULL == NRPRG(predis_commands)) {
    NRPRG(predis_commands)
        = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_predis_command_destroy);
  }

  return NRPRG(predis_commands);
}

static void nr_predis_instrument_connection(zval* conn TSRMLS_DC) {
  nr_php_wrap_callable(
      nr_php_find_class_method(Z_OBJCE_P(conn), "readresponse"),
      nr_predis_connection_readResponse TSRMLS_CC);

  nr_php_wrap_callable(
      nr_php_find_class_method(Z_OBJCE_P(conn), "writecommand"),
      nr_predis_connection_writeRequest TSRMLS_CC);

  nr_php_wrap_callable(
      nr_php_find_class_method(Z_OBJCE_P(conn), "writerequest"),
      nr_predis_connection_writeRequest TSRMLS_CC);
}

char* nr_predis_get_operation_name_from_object(zval* command_obj TSRMLS_DC) {
  char* lower = NULL;
  zval* op = NULL;

  if (!nr_predis_is_command(command_obj TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Predis command does not seem to be a command.");
    return NULL;
  }

  op = nr_php_call(command_obj, "getId");
  if (nr_php_is_zval_valid_string(op)) {
    lower = nr_string_to_lowercase(Z_STRVAL_P(op));
  }
  nr_php_zval_free(&op);

  return lower;
}

nr_datastore_instance_t* nr_predis_create_datastore_instance_from_fields(
    zval* scheme,
    zval* host,
    zval* port,
    zval* path,
    zval* database) {
  char* dbname = NULL;
  nr_datastore_instance_t* instance = NULL;

  /*
   * Convert the database number into a database name if it was given.
   */
  if (nr_php_is_zval_valid_scalar(database)) {
    zval* dbstr = nr_php_zval_alloc();

    /*
     * The database number can be any scalar type due to PHP's type juggling, so
     * we'll use PHP's rules to get it as a string.
     */
    ZVAL_DUP(dbstr, database);
    convert_to_string(dbstr);
    dbname = nr_strndup(Z_STRVAL_P(dbstr), NRSAFELEN(Z_STRLEN_P(dbstr)));

    nr_php_zval_free(&dbstr);
  } else {
    dbname = nr_formatf(NR_INT64_FMT, (int64_t)nr_predis_default_database);
  }

  if (nr_php_is_zval_valid_string(scheme)
      && (0 == nr_strncmp("unix", Z_STRVAL_P(scheme), Z_STRLEN_P(scheme)))) {
    /*
     * If the scheme is "unix", then Predis will attempt to connect to a UNIX
     * socket.
     */
    const char* pathstr = nr_predis_default_path;

    if (nr_php_is_zval_valid_string(path)) {
      pathstr = Z_STRVAL_P(path);
    }

    instance = nr_datastore_instance_create("localhost", pathstr, dbname);
  } else {
    /*
     * Any other scheme value will result in a TCP or HTTP connection: either
     * way, we use the host and port to build the datastore instance. (If the
     * scheme is omitted, "tcp" is assumed.)
     */
    const char* hoststr = nr_predis_default_host;
    char* portstr = NULL;

    if (nr_php_is_zval_valid_string(host)) {
      hoststr = Z_STRVAL_P(host);
    }

    if (nr_php_is_zval_valid_integer(port)) {
      portstr = nr_formatf(NR_INT64_FMT, (int64_t)Z_LVAL_P(port));
    } else {
      portstr = nr_formatf(NR_INT64_FMT, (int64_t)nr_predis_default_port);
    }

    instance = nr_datastore_instance_create(hoststr, portstr, dbname);
    nr_free(portstr);
  }

  nr_free(dbname);
  return instance;
}

nr_datastore_instance_t* nr_predis_create_datastore_instance_from_array(
    zval* params) {
  return nr_predis_create_datastore_instance_from_fields(
      nr_php_zend_hash_find(Z_ARRVAL_P(params), "scheme"),
      nr_php_zend_hash_find(Z_ARRVAL_P(params), "host"),
      nr_php_zend_hash_find(Z_ARRVAL_P(params), "port"),
      nr_php_zend_hash_find(Z_ARRVAL_P(params), "path"),
      nr_php_zend_hash_find(Z_ARRVAL_P(params), "database"));
}

nr_datastore_instance_t*
nr_predis_create_datastore_instance_from_parameters_object(
    zval* params TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  zval* database = nr_predis_get_parameter(params, "database" TSRMLS_CC);
  zval* host = nr_predis_get_parameter(params, "host" TSRMLS_CC);
  zval* path = nr_predis_get_parameter(params, "path" TSRMLS_CC);
  zval* port = nr_predis_get_parameter(params, "port" TSRMLS_CC);
  zval* scheme = nr_predis_get_parameter(params, "scheme" TSRMLS_CC);

  instance = nr_predis_create_datastore_instance_from_fields(scheme, host, port,
                                                             path, database);

  nr_php_zval_free(&database);
  nr_php_zval_free(&host);
  nr_php_zval_free(&path);
  nr_php_zval_free(&port);
  nr_php_zval_free(&scheme);

  return instance;
}

nr_datastore_instance_t* nr_predis_create_datastore_instance_from_string(
    zval* params TSRMLS_DC) {
  zval* database = NULL;
  zval* host = NULL;
  zval* path = NULL;
  zval* port = NULL;
  zval* query = NULL;
  zval* scheme = NULL;
  zval* parts = NULL;
  nr_datastore_instance_t* instance = NULL;

  /*
   * Predis uses PHP's parse_url() function internally, so we'll do likewise.
   */
  parts = nr_php_call(NULL, "parse_url", params);
  if (!nr_php_is_zval_valid_array(parts)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: parse_url failed on string %s",
                     __func__, Z_STRVAL_P(params));
    goto end;
  }

  host = nr_php_zend_hash_find(Z_ARRVAL_P(parts), "host");
  path = nr_php_zend_hash_find(Z_ARRVAL_P(parts), "path");
  port = nr_php_zend_hash_find(Z_ARRVAL_P(parts), "port");
  scheme = nr_php_zend_hash_find(Z_ARRVAL_P(parts), "scheme");

  /*
   * If the database number is provided, it's via a URL query parameter, so we
   * need to parse the query if one exists to see if it has a database
   * parameter. We'll use the PHP userland parse_str() API for that rather than
   * reinventing the wheel.
   */
  query = nr_php_zend_hash_find(Z_ARRVAL_P(parts), "query");
  if (nr_php_is_zval_valid_string(query)) {
    zval* query_parts
        = nr_php_parse_str(Z_STRVAL_P(query), Z_STRLEN_P(query) TSRMLS_CC);

    if (nr_php_is_zval_valid_array(query_parts)) {
      zval* database_zv
          = nr_php_zend_hash_find(Z_ARRVAL_P(query_parts), "database");

      /*
       * parse_str() only returns string keys. Since the string will disappear
       * once we free query_parts, we'll duplicate it into the database zval.
       */
      if (nr_php_is_zval_valid_string(database_zv)) {
        database = nr_php_zval_alloc();
        ZVAL_DUP(database, database_zv);
      }
    }

    nr_php_zval_free(&query_parts);
  }

  instance = nr_predis_create_datastore_instance_from_fields(scheme, host, port,
                                                             path, database);

end:
  nr_php_zval_free(&database);
  nr_php_zval_free(&parts);

  return instance;
}

nr_datastore_instance_t*
nr_predis_create_datastore_instance_from_connection_params(
    zval* params TSRMLS_DC) {
  /*
   * The documented API for Predis\Client::__construct() allows for either an
   * array or string to be provided with connection parameters. We may also be
   * getting a ParametersInterface here if we're creating the datastore instance
   * from an existing NodeConnectionInterface object, or a callable that returns
   * any of the aforementioned types. Finally, we may get NULL if the parameters
   * were omitted altogether, in which case the defaults are used.
   *
   * If the params zval isn't one of the aforementioned possibilities,
   * Client::createConnection() will throw an InvalidArgumentException, so it's
   * OK that we don't handle that here besides writing a log message: the
   * connection will be unusable anyway, so the lack of instance information is
   * the least of the user's worries.
   */
  if (nr_php_is_zval_valid_string(params)) {
    return nr_predis_create_datastore_instance_from_string(params TSRMLS_CC);
  } else if (nr_php_is_zval_valid_array(params)) {
    return nr_predis_create_datastore_instance_from_array(params);
  } else if (nr_predis_is_parameters(params TSRMLS_CC)) {
    return nr_predis_create_datastore_instance_from_parameters_object(
        params TSRMLS_CC);
  } else if (nr_php_is_zval_valid_callable(params TSRMLS_CC)) {
    nr_datastore_instance_t* instance = NULL;
    zval* retval = NULL;

    /*
     * Calling the callable for a second time might be problematic if the
     * callable has side effects, but it's the only option we have for getting
     * at the connection parameters.
     */
    retval = nr_php_call_callable(params);

    /*
     * Since we should have something for this function to use, we'll just
     * recursively call back into it.
     */
    instance = nr_predis_create_datastore_instance_from_connection_params(
        retval TSRMLS_CC);

    nr_php_zval_free(&retval);
    return instance;
  } else if (nr_php_is_zval_valid_object(params)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: cannot create datastore instance from object of "
                     "class %s",
                     __func__, nr_php_class_entry_name(Z_OBJCE_P(params)));
  } else if (params && !nr_php_is_zval_valid_bool(params)) {
    /*
     * Log a message showing the invalid input.
     *
     * Technically, boolean values (which are exempted above) are also invalid,
     * but Laravel provides a boolean when instantiating the
     * client object in its Cache module with the default configuration, so
     * we'll ignore that altogether. It doesn't affect how Predis operates, nor
     * does it affect our instrumentation.
     */
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: cannot create datastore instance from zval of "
                     "unexpected type %d",
                     __func__, (int)Z_TYPE_P(params));
  }

  /*
   * Either we've fallen through, or it's a default connection. Either way,
   * we'll let the defaults handle everything.
   */
  return nr_predis_create_datastore_instance_from_fields(NULL, NULL, NULL, NULL,
                                                         NULL);
}

nr_datastore_instance_t* nr_predis_save_datastore_instance(const zval* conn,
                                                           zval* params
                                                               TSRMLS_DC) {
  char* key = NULL;
  nr_datastore_instance_t* instance = NULL;

  if (NULL == conn) {
    return NULL;
  }

  key = nr_php_datastore_make_key(conn, "predis");
  instance = nr_predis_create_datastore_instance_from_connection_params(
      params TSRMLS_CC);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(key);
  return instance;
}

nr_datastore_instance_t* nr_predis_retrieve_datastore_instance(
    const zval* conn TSRMLS_DC) {
  char* key = NULL;
  nr_datastore_instance_t* instance = NULL;

  if (NULL == conn) {
    return NULL;
  }

  key = nr_php_datastore_make_key(conn, "predis");
  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);

  nr_free(key);
  return instance;
}

zval* nr_predis_get_parameter(zval* params, const char* name TSRMLS_DC) {
  zval* name_zv = NULL;
  zval* retval = NULL;

  name_zv = nr_php_zval_alloc();
  nr_php_zval_str(name_zv, name);
  retval = nr_php_call(params, "__get", name_zv);
  nr_php_zval_free(&name_zv);

  if ((NULL == retval) || (IS_NULL == Z_TYPE_P(retval))) {
    nr_php_zval_free(&retval);
    return NULL;
  }

  return retval;
}

/*
 * This function allows us to quickly assert whether an object is an instance
 * of any of the given class names.
 *
 * We could probably make this generic and pop it in php_agent.c, but no other
 * instrumentation in the PHP agent currently needs this.
 */
static inline int nr_predis_is_object_one_of(const zval* obj,
                                             const char* classes[],
                                             size_t num_classes TSRMLS_DC) {
  size_t i;

  for (i = 0; i < num_classes; i++) {
    if (nr_php_object_instanceof_class(obj, classes[i] TSRMLS_CC)) {
      return 1;
    }
  }

  return 0;
}

int nr_predis_is_aggregate_connection(const zval* obj TSRMLS_DC) {
  return nr_predis_is_object_one_of(
      obj,
      ((const char* [3]){
          "Predis\\Connection\\AggregateConnectionInterface",
          "Predis\\Connection\\AggregatedConnectionInterface",
          "Predis\\Network\\IConnectionCluster",
      }),
      3 TSRMLS_CC);
}

int nr_predis_is_command(const zval* obj TSRMLS_DC) {
  return nr_predis_is_object_one_of(obj,
                                    ((const char* [2]){
                                        "Predis\\Command\\CommandInterface",
                                        "Predis\\Commands\\ICommand",
                                    }),
                                    2 TSRMLS_CC);
}

int nr_predis_is_connection(const zval* obj TSRMLS_DC) {
  return nr_predis_is_object_one_of(
      obj,
      ((const char* [2]){
          "Predis\\Connection\\ConnectionInterface",
          "Predis\\Network\\IConnection",
      }),
      2 TSRMLS_CC);
}

int nr_predis_is_node_connection(const zval* obj TSRMLS_DC) {
  return nr_predis_is_object_one_of(
      obj,
      ((const char* [3]){
          "Predis\\Connection\\NodeConnectionInterface",
          "Predis\\Connection\\SingleConnectionInterface",
          "Predis\\Network\\IConnectionSingle",
      }),
      3 TSRMLS_CC);
}

int nr_predis_is_parameters(const zval* obj TSRMLS_DC) {
  return nr_predis_is_object_one_of(
      obj,
      ((const char* [3]){
          "Predis\\Connection\\ConnectionParametersInterface",
          "Predis\\Connection\\ParametersInterface",
          "Predis\\IConnectionParameters",
      }),
      3 TSRMLS_CC);
}

NR_PHP_WRAPPER(nr_predis_connection_readResponse) {
  char* async_context = NULL;
  zval* command = NULL;
  zval* conn = NULL;
  uint64_t index;
  nr_segment_t* segment = NULL;
  nr_segment_datastore_params_t params = {
    .datastore = {
      .type = NR_DATASTORE_REDIS,
    },
    .callbacks = {
      .backtrace = nr_php_backtrace_callback,
    },
  };
  char* operation = NULL;
  nrtime_t duration;
  nrtime_t* start = NULL;

  (void)wraprec;

  conn = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  command = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(command)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: command is not an object", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  operation = nr_predis_get_operation_name_from_object(command TSRMLS_CC);

  /*
   * Get the original start time of the paired writeRequest() method from the
   * hashmap.
   */
  index = (uint64_t)Z_OBJ_HANDLE_P(command);
  start = nr_hashmap_index_get(nr_predis_get_commands(TSRMLS_C), index);
  if (NULL == start) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: NULL start time", __func__);
    goto end;
  }
  duration = nr_time_duration(*start, nr_txn_now_rel(NRPRG(txn)));

  params.instance = nr_predis_retrieve_datastore_instance(conn TSRMLS_CC);
  params.operation = operation;

  /*
   * In normal, non-pipeline use, predis_ctx will be NULL, and everything is
   * reported synchronously.
   *
   * When a pipeline is being executed, commands can (and do) run
   * asynchronously. The wrapper that we've hooked on
   * Predis\Pipeline\Pipeline::executePipeline() (and its various children) will
   * have set predis_ctx to a non-NULL async context, so we use that to add an
   * async context to the datastore node.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  char* ctx = (char*)nr_stack_get_top(&NRPRG(predis_ctxs));
#else
  char* ctx = NRPRG(predis_ctx);
#endif /* OAPI */
  if (ctx) {
    /*
     * Since we need a unique async context for each element within the
     * pipeline, we'll concatenate the object ID onto the base context name
     * generated in the executePipeline() instrumentation.
     */
    async_context = nr_formatf("%s." NR_UINT64_FMT, ctx, index);
  }

  segment = nr_segment_start(NRPRG(txn), NULL, async_context);
  nr_segment_set_timing(segment, *start, duration);
  nr_segment_datastore_end(&segment, &params);

end:
  nr_php_arg_release(&command);
  nr_php_scope_release(&conn);
  nr_free(async_context);
  nr_free(operation);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_connection_writeRequest) {
  zval* command = NULL;
  uint64_t index;
  nrtime_t* start = NULL;

  (void)wraprec;

  /*
   * When writing the request, we're only really interested in saving the start
   * time for the command object so we can later create a datastore node when
   * the response comes in.
   */

  command = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(command)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: command is not an object", __func__);
    goto end;
  }

  index = (uint64_t)Z_OBJ_HANDLE_P(command);
  start = (nrtime_t*)nr_malloc(sizeof(nrtime_t));
  *start = nr_txn_now_rel(NRPRG(txn));
  nr_hashmap_index_update(nr_predis_get_commands(TSRMLS_C), index, start);

end:
  nr_php_arg_release(&command);
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_aggregateconnection_getConnection) {
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  (void)wraprec;

  NR_PHP_WRAPPER_CALL;

  if (NULL == retval_ptr) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: error retrieving return value pointer", __func__);
  } else if (!nr_predis_is_node_connection(*retval_ptr TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: got an unexpected value that is not a "
                     "NodeConnectionInterface",
                     __func__);
  } else {
    char* key = nr_php_datastore_make_key(*retval_ptr, "predis");

    /*
     * Add the datastore instance metadata if it hasn't already happened.
     */
    if (!nr_php_datastore_has_conn(key TSRMLS_CC)) {
      zval* params = nr_php_call(*retval_ptr, "getParameters");

      (void)nr_predis_save_datastore_instance(*retval_ptr, params TSRMLS_CC);
      nr_php_zval_free(&params);
    }

    /*
     * Actually instrument the connection.
     */
    nr_predis_instrument_connection(*retval_ptr TSRMLS_CC);
    nr_free(key);
  }
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_client_construct) {
  zval* conn = NULL;
  zval* params = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  zval* scope = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  (void)wraprec;

  NR_PHP_WRAPPER_CALL;
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    char* version = nr_php_get_object_constant(scope, "VERSION");
    // Add php package to transaction
    nr_txn_add_php_package(NRPRG(txn), "predis/predis", version);
    nr_free(version);
  }

  /*
   * Grab the connection object from the client, since we actually instrument
   * the connection(s) rather than the client per se: doing so allows us to
   * capture datastore instance information.
   */
  conn = nr_php_call(scope, "getConnection");
  if (nr_predis_is_aggregate_connection(conn TSRMLS_CC)) {
    /*
     * If an aggregate connection is in use, we don't know which actual
     * connection is going to be used for each command until the command is
     * executed: AggregateConnection is opaque from the perspective of the
     * Client object.
     *
     * What we'll do here is instrument AggregateConnection::getConnection() to
     * ensure that we instrument _each_ connection used within the aggregate,
     * since the connections can still change after instantiation.
     */
    nr_php_wrap_callable(
        nr_php_find_class_method(Z_OBJCE_P(conn), "getconnection"),
        nr_predis_aggregateconnection_getConnection TSRMLS_CC);
    nr_php_wrap_callable(
        nr_php_find_class_method(Z_OBJCE_P(conn), "getconnectionbycommand"),
        nr_predis_aggregateconnection_getConnection TSRMLS_CC);
  } else if (nr_predis_is_connection(conn TSRMLS_CC)) {
    /*
     * Use the given parameters to instrument the connection and create the
     * datastore instance metadata.
     */
    nr_predis_instrument_connection(conn TSRMLS_CC);
    (void)nr_predis_save_datastore_instance(conn, params TSRMLS_CC);
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: connection does not implement ConnectionInterface",
                     __func__);
  }

  nr_php_zval_free(&conn);
  nr_php_arg_release(&params);
  nr_php_scope_release(&scope);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_pipeline_executePipeline) {
  (void)wraprec;

  /*
   * Our normal Predis connection instrumentation correctly handles pipelines as
   * well, since it looks for the underlying writeRequest() and readResponse()
   * method calls that the pipeline functionality uses. The only thing we need
   * to do is set up the predis_ctx global for this pipeline so that async
   * contexts are correctly set up.
   *
   * We'll save any existing context just in case this is a nested pipeline.
   */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_stack_push(&NRPRG(predis_ctxs),
                nr_formatf("Predis #" NR_TIME_FMT, nr_get_time()));
#else
  char* prev_predis_ctx;
  prev_predis_ctx = NRPRG(predis_ctx);
  NRPRG(predis_ctx) = nr_formatf("Predis #" NR_TIME_FMT, nr_get_time());
#endif /* OAPI */

  NR_PHP_WRAPPER_CALL;

  /*
   * Restore any previous context on the way out.
   *
   * If not using OAPI, we can simply free the value after the
   * NR_PHP_WRAPPER_CALL. Otherwise, we need an "after function" to do the
   * freeing
   */
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_free(NRPRG(predis_ctx));
  NRPRG(predis_ctx) = prev_predis_ctx;
#endif /* not OAPI */
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
static void predis_executePipeline_handle_stack() {
  char* predis_ctx = (char*)nr_stack_pop(&NRPRG(predis_ctxs));
  nr_free(predis_ctx);
}

NR_PHP_WRAPPER(nr_predis_pipeline_executePipeline_after) {
  (void)wraprec;
  predis_executePipeline_handle_stack();
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_webdisconnection_executeCommand_before) {
  (void)wraprec;

  nr_segment_t* segment = NULL;
  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  segment->wraprec = auto_segment->wraprec;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_predis_webdisconnection_executeCommand_after) {
  (void)wraprec;
  char* operation = NULL;
  zval* command_obj = NULL;
  zval* conn = NULL;
  nr_segment_datastore_params_t params = {
    .datastore = {
      .type = NR_DATASTORE_REDIS,
    },
  };

  command_obj = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS);
  conn = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS);

  operation = nr_predis_get_operation_name_from_object(command_obj);
  params.operation = operation;

  params.instance = nr_predis_retrieve_datastore_instance(conn);

  nr_segment_datastore_end(&auto_segment, &params);

  nr_free(operation);
  nr_php_arg_release(&command_obj);
  nr_php_scope_release(&conn);
}
NR_PHP_WRAPPER_END

#else

NR_PHP_WRAPPER(nr_predis_webdisconnection_executeCommand) {
  char* operation = NULL;
  zval* command_obj = NULL;
  zval* conn = NULL;
  nr_segment_t* segment = NULL;
  nr_segment_datastore_params_t params = {
    .datastore = {
      .type = NR_DATASTORE_REDIS,
    },
  };

  (void)wraprec;

  command_obj = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  conn = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  operation = nr_predis_get_operation_name_from_object(command_obj TSRMLS_CC);
  params.operation = operation;

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);

  NR_PHP_WRAPPER_CALL;

  params.instance = nr_predis_retrieve_datastore_instance(conn TSRMLS_CC);

  nr_segment_datastore_end(&segment, &params);

  nr_free(operation);
  nr_php_arg_release(&command_obj);
  nr_php_scope_release(&conn);
}
NR_PHP_WRAPPER_END
#endif /* OAPI */

void nr_predis_enable(TSRMLS_D) {
  /*
   * Instrument the Client constructor so we can instrument its connection(s).
   */
  nr_php_wrap_user_function(NR_PSTR("Predis\\Client::__construct"),
                            nr_predis_client_construct TSRMLS_CC);

  /*
   * Instrument the pipeline classes that are bundled with Predis so that we
   * correctly set up async contexts.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Predis\\Pipeline\\Pipeline::executePipeline"),
      nr_predis_pipeline_executePipeline,
      nr_predis_pipeline_executePipeline_after);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Predis\\Pipeline\\Atomic::executePipeline"),
      nr_predis_pipeline_executePipeline,
      nr_predis_pipeline_executePipeline_after);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Predis\\Pipeline\\ConnectionErrorProof::executePipeline"),
      nr_predis_pipeline_executePipeline,
      nr_predis_pipeline_executePipeline_after);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Predis\\Pipeline\\FireAndForget::executePipeline"),
      nr_predis_pipeline_executePipeline,
      nr_predis_pipeline_executePipeline_after);
  /*
   * Instrument Webdis connections, since they don't use the same
   * writeRequest()/readResponse() pair as the other connection types.
   */
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Predis\\Connection\\WebdisConnection::executeCommand"),
      nr_predis_webdisconnection_executeCommand_before,
      nr_predis_webdisconnection_executeCommand_after);
#else
  nr_php_wrap_user_function(
      NR_PSTR("Predis\\Pipeline\\Pipeline::executePipeline"),
      nr_predis_pipeline_executePipeline TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Predis\\Pipeline\\Atomic::executePipeline"),
      nr_predis_pipeline_executePipeline TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Predis\\Pipeline\\ConnectionErrorProof::executePipeline"),
      nr_predis_pipeline_executePipeline TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Predis\\Pipeline\\FireAndForget::executePipeline"),
      nr_predis_pipeline_executePipeline TSRMLS_CC);
  /*
   * Instrument Webdis connections, since they don't use the same
   * writeRequest()/readResponse() pair as the other connection types.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Predis\\Connection\\WebdisConnection::executeCommand"),
      nr_predis_webdisconnection_executeCommand TSRMLS_CC);
#endif /* OAPI */

}
