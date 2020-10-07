/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This files handles the termination that happens once per request.
 */
#include "php_agent.h"
#include "php_curl_md.h"
#include "php_error.h"
#include "php_globals.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "util_logging.h"
#include "lib_guzzle4.h"

#ifdef TAGS
void zm_deactivate_newrelic(void); /* ctags landing pad only */
#endif

/*
 * This function is invoked by the Zend Engine during request shutdown. At the
 * point this is called, the Zend Engine has already called all shutdown
 * functions registered with register_shutdown_function(), called all
 * destructors for objects that were unreachable from the global scope, and
 * flushed all output buffers.
 *
 * In spite of the above, note that it _is_ possible for further PHP code to be
 * executed after this function is called: the executor is still running, and
 * other extensions may execute PHP functions or code from their shutdown
 * functions. This most commonly manifests if a custom session handler is in
 * use: in a normal PHP environment, shutdown functions are run in alphabetical
 * order, so a callable or object registered with session_set_save_handler()
 * may still be executed after us.
 *
 * More specifically, you should be very careful destroying state in this
 * function: if userland instrumentation relies on that state, you should
 * assume that it needs to survive this function.
 */
PHP_RSHUTDOWN_FUNCTION(newrelic) {
  (void)type;
  (void)module_number;

  nrl_verbosedebug(NRL_INIT, "RSHUTDOWN processing started");

  /* nr_php_txn_shutdown will check for a NULL transaction. */
  nr_php_txn_shutdown(TSRMLS_C);

  nr_guzzle4_rshutdown(TSRMLS_C);
  nr_curl_rshutdown(TSRMLS_C);

  nrl_verbosedebug(NRL_INIT, "RSHUTDOWN processing done");

  return SUCCESS;
}

/*
 * This function is invoked by the Zend Engine during the post-RSHUTDOWN phase
 * via zend_post_deactivate_modules(). Between the RSHUTDOWN function above and
 * this function, the following things have happened:
 *
 * 1. The output subsystem has been completely shut down.
 * 2. The names of the registered shutdown functions have been freed.
 * 3. Superglobals have been destroyed.
 * 4. Request globals have been destroyed. In spite of the name, this only
 *    covers a handful of internal globals: most notably, the last error
 *    message and file, and the temporary directory.
 * 5. The executor has been shut down, and request-scoped INI settings have
 *    been destroyed.
 *
 * The most important takeaway here is that nothing within
 * nr_php_post_deactivate() can touch the per-request executor state. You can't
 * access anything in EG(current_execute_data), and you can't execute PHP code.
 * You can, however, clean up whatever needs cleaning up, end the transaction
 * and send data to the daemon.
 */
int nr_php_post_deactivate(void) {
  TSRMLS_FETCH();

  nrl_verbosedebug(NRL_INIT, "post-deactivate processing started");

#ifdef PHP7
  /*
   * PHP 7 has a singleton trampoline op array that is used for the life of an
   * executor (which, in non-ZTS mode, is the life of the process). We need to
   * ensure that it goes back to having a NULL wraprec, lest we accidentally try
   * to dereference a transient wraprec that is about to be destroyed.
   */
  EG(trampoline).op_array.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = NULL;
#endif /* PHP7 */

  nr_php_remove_transient_user_instrumentation();

  nr_php_exception_filters_destroy(&NRPRG(exception_filters));
  nr_regex_destroy(&NRPRG(wordpress_plugin_regex));
  nr_regex_destroy(&NRPRG(wordpress_core_regex));
  nr_regex_destroy(&NRPRG(wordpress_hook_regex));
  nr_regex_destroy(&NRPRG(wordpress_theme_regex));
  nr_hashmap_destroy(&NRPRG(wordpress_file_metadata));

  nr_free(NRPRG(mysql_last_conn));
  nr_free(NRPRG(pgsql_last_conn));
  nr_hashmap_destroy(&NRPRG(datastore_connections));

  nr_free(NRPRG(predis_ctx));
  nr_hashmap_destroy(&NRPRG(predis_commands));

  nr_vector_destroy(&NRPRG(user_function_wrappers));

  NRPRG(cufa_callback) = NULL;

  if (nrlikely(0 != NRPRG(txn))) {
    (void)nr_php_txn_end(0, 1 TSRMLS_CC);
  }

  NRPRG(current_framework) = NR_FW_UNSET;
  NRPRG(framework_version) = 0;

  nrl_verbosedebug(NRL_INIT, "post-deactivate processing done");
  return SUCCESS;
}
