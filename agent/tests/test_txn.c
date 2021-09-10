/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_header.h"
#include "php_txn_private.h"
#include "php_newrelic.h"
#include "php_hash.h"

#include "php_globals.h"

static void test_handle_fpm_error(TSRMLS_D) {
  nrobj_t* agent_attributes;
  char* sapi_name;
  /*
   * Test : Bad parameters.
   */
  nr_php_txn_handle_fpm_error(NULL TSRMLS_CC);

  /*
   * Test : Non-FPM. By default, the unit tests report the SAPI as embed, so
   *        the PHP-FPM behavior won't be triggered.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * The next few tests will all pretend to be FPM, so let's set the SAPI name
   * accordingly.
   */
  sapi_name = sapi_module.name;
  sapi_module.name = "fpm-fcgi";

  /*
   * Test : FPM, but with at least one frame called.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  tlib_php_request_eval("$a = 1 + 1; // create a PHP call frame" TSRMLS_CC);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, but with a non-URI path set.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_ACTION,
                         (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, with the specific case that should result in a status code
   *        based transaction name: a fallback URI path, plus a zero call count
   *        in PHP (since no user function or file is ever executed in the
   *        request).
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_sapi_headers(TSRMLS_C)->http_response_code = 404;
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be updated", "404",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be updated",
                         (int)NR_PATH_TYPE_STATUS_CODE,
                         (int)NRTXN(status).path_type);

  agent_attributes = nr_attributes_agent_to_obj(NRTXN(attributes),
                                                NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_not_null("agent attributes must be defined", agent_attributes);
  tlib_pass_if_str_equal(
      "agent attributes must include a request.uri with the original path",
      "foo", nro_get_hash_string(agent_attributes, "request.uri", NULL));
  nro_delete(agent_attributes);

  tlib_php_request_end();

  /*
   * Put the SAPI name back to what it should be.
   */
  sapi_module.name = sapi_name;
}

static void test_max_segments_config_values(TSRMLS_D) {
  nrtxn_t* txn;
  /*
   * Test : max_segments_cli defaults correctly.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_pass_if_size_t_equal("max_segments should be the default of 100,000",
                            100000, txn->options.max_segments);
  tlib_php_request_end();

  /*
   * Test : max_segments_cli gets set correctly.
   */
  NRINI(tt_max_segments_cli) = 200;
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();

  txn = NRPRG(txn);

  tlib_pass_if_size_t_equal("max_segments should be 200", 200,
                            txn->options.max_segments);

  tlib_php_request_end();

  /*
   * Test : correctly defaults to 0 when web txn.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_pass_if_size_t_equal("max_segments 0 when it's a web txn", 0,
                            txn->options.max_segments);

  tlib_php_request_end();

  /*
   * Test : max_segments_cli does not change web txn max segments.
   */
  NRINI(tt_max_segments_web) = 400;
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  tlib_php_request_start();

  txn = NRPRG(txn);

  tlib_pass_if_size_t_equal("max_segments should be set by web when a web txn",
                            400, txn->options.max_segments);

  tlib_php_request_end();
}


static void test_set_initial_path(TSRMLS_D) {
  nrtxn_t* txn;
  zval* server;  
  /*
   * Test : max_segments_cli defaults correctly.
   */

  NR_PHP_PROCESS_GLOBALS(cli) = 1;

#ifdef PHP7
  server = &PG(http_globals)[TRACK_VARS_SERVER];
#else
  server = PG(http_globals)[TRACK_VARS_SERVER];
#endif

  tlib_php_request_start();
  nr_php_add_assoc_string(server, "SCRIPT_FILENAME", "test/script_file.php");
  txn = NRPRG(txn);

  // skip pattern
  txn->options.collect_script_name = 0;
  nr_php_set_initial_path(txn);
  tlib_pass_if_null("Transaction path", txn->path);
  tlib_pass_if_int_equal("Path type", NR_PATH_TYPE_UNKNOWN, txn->status.path_type);

  // not skip
  txn->options.collect_script_name = 1;
  nr_php_set_initial_path(txn);
  tlib_pass_if_str_equal("Transaction path", "test/script_file.php", txn->path);
  tlib_pass_if_int_equal("Path type", NR_PATH_TYPE_URI, txn->status.path_type);

  tlib_php_request_end();
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  /*
   * We're setting up our own engine instance because we need to control the
   * attribute configuration.
   */
  tlib_php_engine_create(
      "newrelic.transaction_events.attributes.include=request.uri" PTSRMLS_CC);

  test_handle_fpm_error(TSRMLS_C);
  test_max_segments_config_values(TSRMLS_C);
  test_set_initial_path(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
