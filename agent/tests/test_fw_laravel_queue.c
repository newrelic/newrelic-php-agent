/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_wrapper.h"
#include "fw_support.h"
#include "fw_laravel_queue.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO

static void setup_classes() {
  // clang-format off
  /* Mock up the classes we'll use to test. */
  const char* job_class =
      "class my_job {"
        "private ?string $job_name;"
        "private ?string $connection_name;"
        "private ?string $queue_name;"
        "function resolveName() {return $this->job_name;}"
        "function getConnectionName() {return $this->connection_name;}"
        "function getQueue() {return $this->queue_name;}"
        "function __construct(?string $job_name = null, ?string $connection_name = null, ?string $queue_name = null) {"
          "$this->job_name = $job_name;"
          "$this->connection_name = $connection_name;"
          "$this->queue_name = $queue_name;"
        "}"
      "}";
    const char* queue_classes =
      "namespace Illuminate\\Queue;"
      "class SyncQueue{"
      "function trycatchExecuteJob() { try { $this->executeJob(); } catch (\\Exception $e) { } }"
      "function executeJob() { throw new \\Exception('oops'); }"
      "function raiseBeforeJobEvent($job) { return; }"
      "}"
      "class Worker{"
      "function process() { return; }"
      "function raiseBeforeJobEvent(string $connectionName, $job) { return; }"
      "}";
  // clang-format on
  tlib_php_request_eval(job_class);
  tlib_php_request_eval(queue_classes);
}

static void test_job_txn_naming_wrappers(TSRMLS_D) {
  /*
   * Test the wrappers that name the job txn:
   * Illuminate\Queue\Worker::raiseBeforeJobEvent(connectionName, job)
   * Illuminate\Queue\SyncQueue::raiseBeforeJobEvent(job)
   * These wrappers should correctly name the transaction with the format:
   * "<job_name> (<connection_name>:<queue_name>)"
   */

  zval* expr = NULL;
  zval* worker_obj = NULL;
  zval* job_obj = NULL;
  zval* arg_unused = NULL;
  char* arg_unused_string = NULL;

  tlib_php_request_start();

  setup_classes();

  arg_unused_string = "'unused'";
  arg_unused = tlib_php_request_eval_expr(arg_unused_string);

  NRINI(force_framework) = NR_FW_LARAVEL;
  nr_laravel_queue_enable();

  tlib_pass_if_not_null("Txn should not be null at the start of the test.",
                        NRPRG(txn));
  nr_txn_set_path("ToBeChanged", NRPRG(txn), "Farewell", NR_PATH_TYPE_CUSTOM,
                  NR_OK_TO_OVERWRITE);
  tlib_pass_if_str_equal("Path should exist", "Farewell", NRTXN(path));

  /*
   * Create the mocked Illuminate\Queue\Work queue worker obj to trigger the
   * wrappers
   */
  worker_obj = tlib_php_request_eval_expr("new Illuminate\\Queue\\Worker");
  tlib_pass_if_not_null("Mocked worker object shouldn't be NULL", worker_obj);

  /* Get class with all values set to NULL.*/
  job_obj = tlib_php_request_eval_expr("new my_job");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class with job_name empty string. */
  job_obj = tlib_php_request_eval_expr("new my_job(job_name:'')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class jobname set.*/
  job_obj = tlib_php_request_eval_expr("new my_job(job_name:'JobName')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "JobName (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class with connection_name empty string. */
  job_obj = tlib_php_request_eval_expr("new my_job(connection_name:'')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class connection_name set. */
  job_obj = tlib_php_request_eval_expr(
      "new my_job(connection_name:'ConnectionName')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (ConnectionName:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class with queue_name empty string. */
  job_obj = tlib_php_request_eval_expr("new my_job(queue_name:'')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class queue_name set. */
  job_obj = tlib_php_request_eval_expr("new my_job(queue_name:'QueueName')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:QueueName)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class all vars set. */
  job_obj = tlib_php_request_eval_expr(
      "new my_job(job_name:'JobName',connection_name:'ConnectionName', "
      "queue_name:'QueueName')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", arg_unused, job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "JobName (ConnectionName:QueueName)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /*
   * Create the mocked Illuminate\Queue\SyncQueue queue worker obj to trigger
   * the wrappers that we tested with Worker::raiseBeforeJobEvent, we only to do
   * basic tests of all null, all emptystring, and all properly set.
   */
  nr_php_zval_free(&worker_obj);
  worker_obj = tlib_php_request_eval_expr("new Illuminate\\Queue\\SyncQueue");
  tlib_pass_if_not_null("Mocked worker object shouldn't be NULL", worker_obj);

  /* Get class with all values set to NULL.*/
  job_obj = tlib_php_request_eval_expr("new my_job");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class with all set to empty string. */
  job_obj = tlib_php_request_eval_expr(
      "new my_job(job_name:'', connection_name:'', queue_name:'')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "unknown (unknown:default)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  /* Get class all set.*/
  job_obj = tlib_php_request_eval_expr(
      "new my_job(job_name:'JobName', connection_name:'ConnectionName', "
      "queue_name:'QueueName')");
  tlib_pass_if_not_null("Mocked job object shouldn't be NULL", job_obj);
  /* trigger raiseBeforeJobEvent to name the txn*/
  expr = nr_php_call(worker_obj, "raiseBeforeJobEvent", job_obj);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  tlib_pass_if_not_null("Txn name should not be null", NRTXN(path));
  tlib_pass_if_str_equal("Txn name should be changed",
                         "JobName (ConnectionName:QueueName)", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&job_obj);

  nr_php_zval_free(&worker_obj);
  nr_php_zval_free(&arg_unused);
  tlib_php_request_end();
}

static void test_job_txn_startstop_wrappers(TSRMLS_D) {
  /*
   * Test the wrappers that start and end the job txn:
   * Illuminate\Queue\Worker::process
   * Illuminate\Queue\SyncQueue::executeJob
   * These wrappers should correctly end the current txn and start a new one
   * in the before wrapper and end/start again in the after/clean
   */

  zval* expr = NULL;
  zval* obj = NULL;
  nrtime_t txn_time = 0;
  nrtime_t new_txn_time = 0;
  tlib_php_engine_create("");
  tlib_php_request_start();

  setup_classes();

  NRINI(force_framework) = NR_FW_LARAVEL;
  nr_laravel_queue_enable();

  /*
   * nr_laravel_queue_worker_before will end the txn and discard it and all
   * segments before starting a new txn. With OAPI we store wraprecs on the
   * segment in func_begin. Since nr_laravel_queue_worker_before is destroying
   * the old txn and discarding all segments, ensure the wraprec is preserved on
   * a segment for "after" wrappers that could be called in func_end.
   * Illuminate\\Queue\\SyncQueue::executeJob and
   * Illuminate\\Queue\\Worker::process both resolve to the same wrapper
   * callback.  We'll use the mocked process to show the happy path, and we'll
   * use execute job to show the exception path.
   */

  tlib_pass_if_not_null("Txn should not be null at the start of the test.",
                        NRPRG(txn));
  txn_time = nr_txn_start_time(NRPRG(txn));

  nr_txn_set_path("ToBeDiscarded", NRPRG(txn), "Farewell", NR_PATH_TYPE_CUSTOM,
                  NR_OK_TO_OVERWRITE);
  tlib_pass_if_str_equal("Path should exist", "Farewell", NRTXN(path));

  /* Create the mocked Worker and call process*/
  obj = tlib_php_request_eval_expr("new Illuminate\\Queue\\Worker");
  tlib_pass_if_not_null("object shouldn't be NULL", obj);
  expr = nr_php_call(obj, "process");
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  new_txn_time = nr_txn_start_time(NRPRG(txn));
  tlib_pass_if_not_null(
      "Txn should not be null after the call to end and start a txn.",
      NRPRG(txn));
  tlib_pass_if_true("Txn times should NOT match.", txn_time != new_txn_time,
                    "Verified times are different, new time is: " NR_TIME_FMT,
                    new_txn_time);
  /*
   * The before wrapper will stop/start a txn and name the new one unknown until
   * we get naming. The after/clean wrapper stop/start a txn and give no name to
   * the new txn that wlil get discarded later. So if both txns have been
   * started/stopped, we should end up with a NULL txn name.
   */
  tlib_pass_if_null("Txn name should be NULL", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&obj);
  tlib_php_request_end();
  tlib_php_engine_destroy();

  tlib_php_engine_create("");
  tlib_php_request_start();

  setup_classes();

  NRINI(force_framework) = NR_FW_LARAVEL;
  nr_laravel_queue_enable();

  tlib_pass_if_not_null("Txn should not be null at the start of the test.",
                        NRPRG(txn));
  txn_time = nr_txn_start_time(NRPRG(txn));

  nr_txn_set_path("ToBeDiscarded", NRPRG(txn), "Farewell", NR_PATH_TYPE_CUSTOM,
                  NR_OK_TO_OVERWRITE);
  tlib_pass_if_str_equal("Path should exist", "Farewell", NRTXN(path));

  /* Create the mocked Worker and call process*/
  obj = tlib_php_request_eval_expr("new Illuminate\\Queue\\SyncQueue");
  tlib_pass_if_not_null("object shouldn't be NULL", obj);
  /* We're doing the trycatch because otherwise our fragile testing system can't
   * handle an uncaught exception.*/
  expr = nr_php_call(obj, "trycatchExecuteJob");
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  new_txn_time = nr_txn_start_time(NRPRG(txn));
  tlib_pass_if_not_null(
      "Txn should not be null after the call to end and start a txn.",
      NRPRG(txn));
  tlib_pass_if_true("Txn times should NOT match.", txn_time != new_txn_time,
                    "Verified times are different, new time is: " NR_TIME_FMT,
                    new_txn_time);
  /*
   * The Job txn will either be named after the job or named with unknown.
   * Any txn started as we wait for another job will have a NULL name.
   */
  tlib_pass_if_null("Txn name should be NULL", NRTXN(path));
  nr_php_zval_free(&expr);
  nr_php_zval_free(&obj);
  tlib_php_request_end();
  tlib_php_engine_destroy();
}

#endif

void test_main(void* p NRUNUSED) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  tlib_php_engine_create("");

  test_job_txn_naming_wrappers();

  tlib_php_engine_destroy();

  test_job_txn_startstop_wrappers();

#endif /* PHP 8.0+ */
}
