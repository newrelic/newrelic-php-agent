/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_api_distributed_trace.h"
#include "php_call.h"
#include "php_error.h"
#include "php_hash.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_object.h"
#include "util_serialize.h"
#include "fw_support.h"
#include "fw_laravel_queue.h"

/*
 * This file includes functions for instrumenting Laravel's Queue component. As
 * with our primary Laravel instrumentation, all 4.x, 5.x  and 6.x versions are
 * supported.
 *
 * Userland docs for this can be found at: http://laravel.com/docs/5.0/queues
 * (use the dropdown to change to other versions)
 *
 * As with most of our framework files, the entry point is in the last
 * function, and it may be easier to read up from there to get a sense of how
 * this fits together.
 */

/*
 * Purpose : Check if the given job is a SyncJob.
 *
 * Params  : 1. The job object.
 *
 * Purpose : Non-zero if the job is a SyncJob, zero if it is any other type.
 */
static int nr_laravel_queue_is_sync_job(zval* job TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      job, "Illuminate\\Queue\\Jobs\\SyncJob" TSRMLS_CC);
}

/*
 * Purpose : Extract the actual job name from a job that used CallQueuedHandler
 *           to enqueue a serialised object.
 *
 * Params  : 1. The job object.
 *
 * Returns : The job name, which is owned by the caller, or NULL if it cannot
 *           be found.
 */
static char* nr_laravel_queue_job_command(zval* job TSRMLS_DC) {
  nrobj_t* body = NULL;
  const char* command = NULL;
  const nrobj_t* data = NULL;
  zval* json = NULL;
  char* retval = NULL;

  json = nr_php_call(job, "getRawBody");
  if (!nr_php_is_zval_non_empty_string(json)) {
    goto end;
  }

  body = nro_create_from_json(Z_STRVAL_P(json));
  data = nro_get_hash_value(body, "data", NULL);
  command = nro_get_hash_string(data, "command", NULL);
  if (NULL == command) {
    goto end;
  }

  /*
   * The command is a serialised object. We're only interested in the class
   * name, so rather than trying to parse it entirely, we'll just parse enough
   * to get at that name.
   */
  retval = nr_serialize_get_class_name(command, nr_strlen(command));

end:
  nro_delete(body);
  nr_php_zval_free(&json);

  return retval;
}

/*
 * Purpose : Infer the job name from a job's payload, provided the job is not a
 *           SyncJob.
 *
 * Params  : 1. The job object.
 *
 * Returns : The job name, which is owned by the caller, or NULL if it cannot
 *           be found.
 */
static char* nr_laravel_queue_infer_generic_job_name(zval* job TSRMLS_DC) {
  nrobj_t* data = NULL;
  zval* json = NULL;
  const char* klass;
  char* name = NULL;

  /*
   * The base Job class in Laravel 4.1 onwards provides a getRawBody() method
   * that we can use to get the normal JSON, from which we can access the "job"
   * property which normally contains the class name.
   */
  json = nr_php_call(job, "getRawBody");
  if (!nr_php_is_zval_non_empty_string(json)) {
    goto end;
  }

  data = nro_create_from_json(Z_STRVAL_P(json));
  klass = nro_get_hash_string(data, "job", NULL);
  if (klass) {
    name = nr_strdup(klass);
  }

end:
  nro_delete(data);
  nr_php_zval_free(&json);

  return name;
}

/*
 * Purpose : Infer the job name from a SyncJob instance.
 *
 * Params  : 1. The SyncJob object.
 *
 * Returns : The job name, which is owned by the caller, or NULL if it cannot
 *           be found.
 */
static char* nr_laravel_queue_infer_sync_job_name(zval* job TSRMLS_DC) {
  zval* job_name;

  /*
   * SyncJob instances have the class name in a property, which is easy.
   */
  job_name = nr_php_get_zval_object_property(job, "job" TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string(job_name)) {
    return nr_strndup(Z_STRVAL_P(job_name), Z_STRLEN_P(job_name));
  }

  return NULL;
}

/*
 * Purpose : Infer the job name from a job's payload.
 *
 * Params  : 1. The job object.
 *
 * Returns : The job name, which is owned by the caller, or NULL if it cannot
 *           be found.
 */
static char* nr_laravel_queue_infer_job_name(zval* job TSRMLS_DC) {
  if (nr_laravel_queue_is_sync_job(job TSRMLS_CC)) {
    return nr_laravel_queue_infer_sync_job_name(job TSRMLS_CC);
  }

  return nr_laravel_queue_infer_generic_job_name(job TSRMLS_CC);
}

/*
 * Purpose : Retrieve the name for a job.
 *
 * Params  : 1. The job object.
 *
 * Returns : The job name, which is owned by the caller, or NULL if it cannot
 *           be found.
 */
static char* nr_laravel_queue_job_name(zval* job TSRMLS_DC) {
  char* name = NULL;

  if (!nr_php_object_instanceof_class(
          job, "Illuminate\\Queue\\Jobs\\Job" TSRMLS_CC)) {
    return NULL;
  }

  /*
   * We have a few options available to us. The simplest option is to use the
   * result of Job::getName(), but this isn't very specific for queued jobs and
   * closures. In those cases, we'll dig around and see if we can come up with
   * something better.
   *
   * Step one, of course, is to see what Job::getName() actually gives us.
   */
  if (!nr_php_object_has_method(job, "getName" TSRMLS_CC)) {
    /*
     * Laravel 4.1 didn't have the Job::getName() method because each job
     * subclass could define its own metadata storage format. We'll try to root
     * around a bit more.
     */
    name = nr_laravel_queue_infer_job_name(job TSRMLS_CC);
  } else {
    zval* job_name = nr_php_call(job, "getName");

    if (nr_php_is_zval_non_empty_string(job_name)) {
      name = nr_strndup(Z_STRVAL_P(job_name), Z_STRLEN_P(job_name));
    } else {
      /*
       * That's weird, but let's again try to infer the job name based on the
       * payload.
       */
      name = nr_laravel_queue_infer_job_name(job TSRMLS_CC);
    }

    nr_php_zval_free(&job_name);
  }

  if (NULL == name) {
    return NULL;
  }

  /*
   * If the job is a CallQueuedHandler job, then we should extract the command
   * name of the actual command that has been queued.
   *
   * This string comparison feels slightly fragile, but there's literally
   * nothing else we can poke at in the job record to check this.
   */
  if (0 == nr_strcmp(name, "Illuminate\\Queue\\CallQueuedHandler@call")) {
    char* command = nr_laravel_queue_job_command(job TSRMLS_CC);

    if (command) {
      nr_free(name);
      return command;
    }
  }

  /*
   * If we haven't already returned, then the job name is the best we have, so
   * let's return that.
   */
  return name;
}

/*
 * Iterator function and supporting struct to walk an nrobj_t hash to extract
 * CATMQ headers in a case insensitive manner.
 */
typedef struct {
  const char* id;
  const char* synthetics;
  const char* transaction;
  const char* dt_payload;
  const char* traceparent;
  const char* tracestate;
} nr_laravel_queue_headers_t;

static nr_status_t nr_laravel_queue_iterate_headers(
    const char* key,
    const nrobj_t* val,
    nr_laravel_queue_headers_t* headers) {
  char* key_lc;

  if (NULL == headers) {
    return NR_SUCCESS;
  }

  key_lc = nr_string_to_lowercase(key);
  if (NULL == key_lc) {
    return NR_SUCCESS;
  }

  if (0 == nr_strcmp(key_lc, X_NEWRELIC_ID_MQ_LOWERCASE)) {
    headers->id = nro_get_string(val, NULL);
  } else if (0 == nr_strcmp(key_lc, X_NEWRELIC_SYNTHETICS_MQ_LOWERCASE)) {
    headers->synthetics = nro_get_string(val, NULL);
  } else if (0 == nr_strcmp(key_lc, X_NEWRELIC_TRANSACTION_MQ_LOWERCASE)) {
    headers->transaction = nro_get_string(val, NULL);
  } else if (0 == nr_strcmp(key_lc, X_NEWRELIC_DT_PAYLOAD_MQ_LOWERCASE)) {
    headers->dt_payload = nro_get_string(val, NULL);
  } else if (0 == nr_strcmp(key_lc, X_NEWRELIC_W3C_TRACEPARENT_MQ_LOWERCASE)) {
    headers->traceparent = nro_get_string(val, NULL);
  } else if (0 == nr_strcmp(key_lc, X_NEWRELIC_W3C_TRACESTATE_MQ_LOWERCASE)) {
    headers->tracestate = nro_get_string(val, NULL);
  }

  nr_free(key_lc);
  return NR_SUCCESS;
}

/*
 * Purpose : Parse a Laravel 4.1+ job object for CATMQ metadata and update the
 *           transaction type accordingly.
 *
 * Params  : 1. The job object.
 */
static void nr_laravel_queue_set_cat_txn(zval* job TSRMLS_DC) {
  zval* json = NULL;
  nrobj_t* payload = NULL;
  nr_laravel_queue_headers_t headers = {NULL, NULL, NULL, NULL};

  /*
   * We're not interested in SyncJob instances, since they don't run in a
   * separate queue worker and hence don't need to be linked via CATMQ.
   */
  if (nr_laravel_queue_is_sync_job(job TSRMLS_CC)) {
    return;
  }

  /*
   * Let's see if we can access the payload.
   */
  if (!nr_php_object_has_method(job, "getRawBody" TSRMLS_CC)) {
    return;
  }

  json = nr_php_call(job, "getRawBody");
  if (!nr_php_is_zval_non_empty_string(json)) {
    goto end;
  }

  /*
   * We've got it. Let's decode the payload and extract our CATMQ properties.
   *
   * Our nro code doesn't handle NULLs particularly gracefully, but it doesn't
   * matter here, as we're not turning this back into JSON and aren't
   * interested in the properties that could include NULLs.
   */
  payload = nro_create_from_json(Z_STRVAL_P(json));
  nro_iteratehash(payload, (nrhashiter_t)nr_laravel_queue_iterate_headers,
                  (void*)&headers);

  if (headers.id && headers.transaction) {
    nr_header_set_cat_txn(NRPRG(txn), headers.id, headers.transaction);
  }

  if (headers.synthetics) {
    nr_header_set_synthetics_txn(NRPRG(txn), headers.synthetics);
  }

  if (headers.dt_payload || headers.traceparent) {
    char* dt_payload = nr_strdup(headers.dt_payload);
    char* tracestate = nr_strdup(headers.tracestate);
    char* traceparent = nr_strdup(headers.traceparent);

    nr_hashmap_t* header_map = nr_header_create_distributed_trace_map(
        dt_payload, traceparent, tracestate);

    nr_php_api_accept_distributed_trace_payload_httpsafe(NRPRG(txn), header_map,
                                                         "Other");
    nr_free(dt_payload);
    nr_free(tracestate);
    nr_free(traceparent);
    nr_hashmap_destroy(&header_map);
  }

end:
  nr_php_zval_free(&json);
  nro_delete(payload);
}

/*
 * Purpose : Parse the decoded payload array from a Laravel 4.0 job and set the
 *           transaction name accordingly.
 *
 * Params  : 1. The payload array.
 */
static void nr_laravel_queue_name_from_payload_array(
    const zval* payload TSRMLS_DC) {
  const zval* job = nr_php_zend_hash_find(Z_ARRVAL_P(payload), "job");

  /*
   * If the payload contains a "job" entry, we'll
   * use that for the name. Otherwise, there's no standard entry we can look
   * at, so we'll just bail.
   */
  if (!nr_php_is_zval_non_empty_string(job)) {
    return;
  }

  nr_txn_set_path("Laravel", NRPRG(txn), Z_STRVAL_P(job), NR_PATH_TYPE_CUSTOM,
                  NR_OK_TO_OVERWRITE);
}

/*
 * Purpose : Parse the decoded payload array from a Laravel 4.0 job and set the
 *           transaction type accordingly.
 *
 * Params  : 1. The payload array.
 */
static void nr_laravel_queue_set_cat_txn_from_payload_array(
    const zval* payload TSRMLS_DC) {
  const zval* id = NULL;
  const zval* synthetics = NULL;
  const zval* transaction = NULL;
  const zval* dt_payload = NULL;
  const zval* traceparent = NULL;
  const zval* tracestate = NULL;

  /*
   * This is ugly, but actually very simple: we want to get the array values
   * for the metadata keys, and if they're set, we'll call
   * nr_header_set_cat_txn and (optionally) nr_header_set_synthetics_txn to set
   * the transaction type and attributes.
   */
  id = nr_php_zend_hash_find(Z_ARRVAL_P(payload), X_NEWRELIC_ID_MQ);
  synthetics
      = nr_php_zend_hash_find(Z_ARRVAL_P(payload), X_NEWRELIC_SYNTHETICS_MQ);
  transaction
      = nr_php_zend_hash_find(Z_ARRVAL_P(payload), X_NEWRELIC_TRANSACTION_MQ);
  dt_payload
      = nr_php_zend_hash_find(Z_ARRVAL_P(payload), X_NEWRELIC_DT_PAYLOAD_MQ);
  traceparent = nr_php_zend_hash_find(Z_ARRVAL_P(payload),
                                      X_NEWRELIC_W3C_TRACEPARENT_MQ);
  tracestate = nr_php_zend_hash_find(Z_ARRVAL_P(payload),
                                     X_NEWRELIC_W3C_TRACEPARENT_MQ);

  if (!nr_php_is_zval_non_empty_string(id)
      || !nr_php_is_zval_non_empty_string(transaction)) {
    return;
  }

  nr_header_set_cat_txn(NRPRG(txn), Z_STRVAL_P(id), Z_STRVAL_P(transaction));

  if (nr_php_is_zval_non_empty_string(synthetics)) {
    nr_header_set_synthetics_txn(NRPRG(txn), Z_STRVAL_P(synthetics));
  }

  if (nr_php_is_zval_non_empty_string(dt_payload)) {
    nr_hashmap_t* header_map = nr_header_create_distributed_trace_map(
        Z_STRVAL_P(dt_payload), Z_STRVAL_P(traceparent),
        Z_STRVAL_P(tracestate));

    nr_php_api_accept_distributed_trace_payload_httpsafe(NRPRG(txn), header_map,
                                                         "Other");
    nr_hashmap_destroy(&header_map);
  }
}

/*
 * Handle:
 *   Illuminate\Queue\Jobs\Job::resolveAndFire (array $payload): void
 *
 * Although this function exists on all versions of Laravel, we only hook this
 * on Laravel 4.0, as we have better ways of getting the job name directly in
 * the Worker::process() callback on other versions.
 */
NR_PHP_WRAPPER(nr_laravel_queue_job_resolveandfire) {
  zval* payload = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);

  payload = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_array(payload)) {
    goto end;
  }

  nr_laravel_queue_name_from_payload_array(payload TSRMLS_CC);
  nr_laravel_queue_set_cat_txn_from_payload_array(payload TSRMLS_CC);

end:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&payload);
}
NR_PHP_WRAPPER_END

/*
 * Handle:
 *   (Laravel 4.0)
 *   Illuminate\Queue\Worker::process (Job $job, int $delay): void
 *
 *   (Laravel 4.1+)
 *   Illuminate\Queue\Worker::process (string $connection, Job $job, int
 * $maxTries = 0, int $delay = 0): void
 */
NR_PHP_WRAPPER(nr_laravel_queue_worker_process) {
  zval* connection = NULL;
  zval* job = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);

  /*
   * Throw away the current transaction, since it only exists to ensure this
   * hook is called.
   */
  nr_php_txn_end(1, 0 TSRMLS_CC);

  /*
   * Begin the transaction we'll actually record.
   */
  if (NR_SUCCESS == nr_php_txn_begin(NULL, NULL TSRMLS_CC)) {
    nr_txn_set_as_background_job(NRPRG(txn), "Laravel job");

    connection = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    if (nr_php_is_zval_non_empty_string(connection)) {
      /*
       * Laravel 4.1 and later (including 5.x) pass the name of the connection
       * as the first parameter.
       */
      char* connection_name = NULL;
      char* job_name;
      char* txn_name = NULL;

      if (nr_php_is_zval_non_empty_string(connection)) {
        connection_name
            = nr_strndup(Z_STRVAL_P(connection), Z_STRLEN_P(connection));
      } else {
        connection_name = nr_strdup("unknown");
      }

      job = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
      job_name = nr_laravel_queue_job_name(job TSRMLS_CC);
      if (NULL == job_name) {
        job_name = nr_strdup("unknown job");
      }

      txn_name = nr_formatf("%s (%s)", job_name, connection_name);

      nr_laravel_queue_set_cat_txn(job TSRMLS_CC);

      nr_txn_set_path("Laravel", NRPRG(txn), txn_name, NR_PATH_TYPE_CUSTOM,
                      NR_OK_TO_OVERWRITE);

      nr_free(connection_name);
      nr_free(job_name);
      nr_free(txn_name);
    } else {
      /*
       * Laravel 4.0 only provides the job to this method, and the Job class
       * doesn't provide a getRawBody method. We'll hook the resolveAndFire
       * method on the job argument (which is the first argument, so is
       * normally the connection on newer versions), since that gets the
       * payload, and then use that to name if we can.
       */
      if (nr_php_is_zval_valid_object(connection)) {
        const char* klass = nr_php_class_entry_name(Z_OBJCE_P(connection));
        char* method = nr_formatf("%s::resolveAndFire", klass);

        nr_php_wrap_user_function(
            method, nr_strlen(method),
            nr_laravel_queue_job_resolveandfire TSRMLS_CC);

        nr_free(method);
      }

      /*
       * We'll set a fallback name just in case.
       */
      nr_txn_set_path("Laravel", NRPRG(txn), "unknown job", NR_PATH_TYPE_CUSTOM,
                      NR_OK_TO_OVERWRITE);
    }
  }

  NR_PHP_WRAPPER_CALL;

  /*
   * We need to report any uncaught exceptions now, so that they're on the
   * transaction we're about to end. We can see if there's an exception waiting
   * to be caught by looking at EG(exception).
   */
  if (EG(exception)) {
    zval* exception_zval = NULL;

#ifdef PHP7
    /*
     * On PHP 7, EG(exception) is stored as a zend_object, and is only wrapped
     * in a zval when it actually needs to be. Unfortunately, our error handling
     * code assumes that an exception is always provided as a zval, and
     * unravelling that would make PHP 5 support more difficult. So we'll just
     * set up a zval here for now.
     *
     * We don't particularly want to use nr_php_zval_alloc() and
     * nr_php_zval_free() here: nr_php_zval_free() will destroy the exception
     * object, and that's bad news for when it's caught in a few frames.
     * Instead, we'll do the exact same thing the Zend Engine itself does when
     * it needs to wrap an in-flight exception in a full zval: we'll create a
     * zval on the stack, set its object to EG(exception), and then just let the
     * zval disappear into the aether without any destructors being run.
     */
    zval exception;

    ZVAL_OBJ(&exception, EG(exception));
    exception_zval = &exception;
#else
    /*
     * On PHP 5, the exception is just a regular old zval.
     */
    exception_zval = EG(exception);
#endif /* PHP7 */

    nr_php_error_record_exception(
        NRPRG(txn), exception_zval, NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION,
        "Unhandled exception within Laravel Queue job: ",
        &NRPRG(exception_filters) TSRMLS_CC);
  }

  nr_php_arg_release(&connection);
  nr_php_arg_release(&job);

  /*
   * End the real transaction and then start a new transaction so our
   * instrumentation continues to fire, knowing that we'll ignore that
   * transaction either when Worker::process() is called again or when
   * WorkCommand::fire() exits.
   */
  nr_php_txn_end(0, 0 TSRMLS_CC);
  nr_php_txn_begin(NULL, NULL TSRMLS_CC);
}
NR_PHP_WRAPPER_END

/*
 * Handle:
 *   Illuminate\Queue\Console\WorkCommand::fire (): void
 */
NR_PHP_WRAPPER(nr_laravel_queue_workcommand_fire) {
  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);

  /*
   * Here's the problem: we want to record individual transactions for each job
   * that is executed, but don't want to record a transaction for the actual
   * queue:work command, since it spends most of its time sleeping. The naive
   * approach would be to end the transaction immediately and instrument
   * Worker::process(). The issue with that is that instrumentation hooks
   * aren't executed if we're not actually in a transaction.
   *
   * So instead, what we'll do is to keep recording, but ensure that we ignore
   * the transaction after WorkCommand::fire() has finished executing, at which
   * point no more jobs can be run.
   */

  /*
   * Start listening for jobs.
   */
  nr_php_wrap_user_function(NR_PSTR("Illuminate\\Queue\\Worker::process"),
                            nr_laravel_queue_worker_process TSRMLS_CC);

  /*
   * Actually execute the command's fire() method.
   */
  NR_PHP_WRAPPER_CALL;

  /*
   * Stop recording the transaction and throw it away.
   */
  nr_php_txn_end(1, 0 TSRMLS_CC);
}
NR_PHP_WRAPPER_END

/*
 * This supports the mapping of outbound payload headers to their
 * message queue variants
 */
static const struct {
  char* header;
  char* header_mq;
} nr_laravel_payload_header_mappings[] = {
    {X_NEWRELIC_ID, X_NEWRELIC_ID_MQ},
    {X_NEWRELIC_TRANSACTION, X_NEWRELIC_TRANSACTION_MQ},
    {X_NEWRELIC_SYNTHETICS, X_NEWRELIC_SYNTHETICS_MQ},
    {NEWRELIC, X_NEWRELIC_DT_PAYLOAD_MQ},
    {W3C_TRACEPARENT, X_NEWRELIC_W3C_TRACEPARENT_MQ},
    {W3C_TRACESTATE, X_NEWRELIC_W3C_TRACESTATE_MQ},
};

/*
 * Purpose : Helper function that returns the message queue variant of the
 *           outbound payload header name.
 *
 * Params  : 1. The request header name.
 *
 * Returns : The message queue variant of the header name
 */
static char* nr_laravel_get_payload_header_mq(char* header) {
  size_t header_count = (sizeof(nr_laravel_payload_header_mappings)
                         / sizeof(nr_laravel_payload_header_mappings[0]));
  size_t i;

  if (NULL == header) {
    return NULL;
  }

  for (i = 0; i < header_count; i++) {
    if (0 == nr_strcmp(header, nr_laravel_payload_header_mappings[i].header)) {
      return nr_laravel_payload_header_mappings[i].header_mq;
    }
  }

  return NULL;
}

/*
 * Handle:
 *   Illuminate\Queue\Queue::createPayload (string $job, ...): string
 */
NR_PHP_WRAPPER(nr_laravel_queue_queue_createpayload) {
  zval* json = NULL;
  zval* payload = NULL;
  zval** retval_ptr = nr_php_get_return_value_ptr(TSRMLS_C);
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* header_mq = NULL;
  char* value = NULL;
  size_t i;
  size_t header_count;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);
  NR_PHP_WRAPPER_CALL;

  if ((NULL == retval_ptr) || !nr_php_is_zval_non_empty_string(*retval_ptr)) {
    goto end;
  }

  /*
   * Get the "headers" that we need to attach to the payload.
   */
  outbound_headers
      = nr_header_outbound_request_create(NRPRG(txn), auto_segment);

  if (NULL == outbound_headers) {
    goto end;
  }

  /*
   * The payload should be a JSON string: in essence, we want to decode it, add
   * our attributes, and then re-encode it. Unfortunately, the payload will
   * include NULL bytes for closures, and this causes nro to choke badly
   * because it can't handle NULLs in strings, so we'll call back into PHP's
   * own JSON functions.
   */
  payload = nr_php_json_decode(*retval_ptr TSRMLS_CC);
  if (NULL == payload) {
    goto end;
  }

  /*
   * As the payload is an object, we need to set properties on it.
   */
  header_keys = nr_hashmap_keys(outbound_headers);
  header_count = nr_vector_size(header_keys);

  for (i = 0; i < header_count; i++) {
    header = nr_vector_get(header_keys, i);
    value = (char*)nr_hashmap_get(outbound_headers, header, nr_strlen(header));
    header_mq = nr_laravel_get_payload_header_mq(header);

    if (header_mq) {
      zend_update_property_string(Z_OBJCE_P(payload), payload, header_mq,
                                  nr_strlen(header_mq), value TSRMLS_CC);
    }
  }

  json = nr_php_json_encode(payload TSRMLS_CC);
  if (NULL == json) {
    goto end;
  }

  /*
   * Finally, we change the string in the return value to our new JSON.
   */
#ifdef PHP7
  zend_string_free(Z_STR_P(*retval_ptr));
  Z_STR_P(*retval_ptr) = zend_string_copy(Z_STR_P(json));
#else
  efree(Z_STRVAL_PP(retval_ptr));
  nr_php_zval_str_len(*retval_ptr, Z_STRVAL_P(json), Z_STRLEN_P(json));
#endif /* PHP7 */

end:
  nr_php_zval_free(&payload);
  nr_php_zval_free(&json);
  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}
NR_PHP_WRAPPER_END

void nr_laravel_queue_enable(TSRMLS_D) {
  /*
   * Hook the command class that implements Laravel's queue:work command so
   * that we can disable the default transaction and add listeners to generate
   * appropriate background transactions when handling jobs.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Illuminate\\Queue\\Console\\WorkCommand::fire"),
      nr_laravel_queue_workcommand_fire TSRMLS_CC);

  /*
   * Laravel 5.5 renamed the methods on all its command classes from `fire`
   * to `handle`.  As a result, we also need to hook the following.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Illuminate\\Queue\\Console\\WorkCommand::handle"),
      nr_laravel_queue_workcommand_fire TSRMLS_CC);

  /*
   * Hook the method that creates the JSON payloads for queued jobs so that we
   * can add our metadata for CATMQ.
   */
  nr_php_wrap_user_function(NR_PSTR("Illuminate\\Queue\\Queue::createPayload"),
                            nr_laravel_queue_queue_createpayload TSRMLS_CC);
}
