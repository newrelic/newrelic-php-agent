/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles the initialization that happens once per module load.
 */
#include "php_agent.h"

#include <dlfcn.h>
#include <signal.h>

#include <Zend/zend_exceptions.h>

#include "php_api_distributed_trace.h"
#include "php_environment.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_extension.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_hooks.h"
#include "php_internal_instrument.h"
#include "php_observer.h"
#include "php_samplers.h"
#include "php_user_instrument.h"
#include "php_vm.h"
#include "php_wrapper.h"
#include "fw_laravel.h"
#include "lib_guzzle4.h"
#include "lib_guzzle6.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "nr_banner.h"
#include "nr_daemon_spawn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_threads.h"

/*
 * Observer API functionality was added with PHP 8.0.
 *
 * The Observer API provide function handlers that trigger on every userland
 * function begin and end.  The handlers provide all zend_execute_data and the
 * end handler provides the return value pointer. The previous way to hook into
 * PHP was via zend_execute_ex which will hook all userland function calls with
 * significant overhead for doing the call. However, depending on user stack
 * size settings, it could potentially generate an extremely deep call stack in
 * PHP because zend_execute_ex limits stack size to whatever user settings
 * are. Observer API bypasses the stack overflow issue that an agent could run
 * into when intercepting userland calls.  Additionally, with PHP 8.0, JIT
 * optimizations could optimize out a call to zend_execute_ex and the agent
 * would not be able to overwite that call properly as the agent wouldn't have
 * access to the JITed information.  This could lead to segfaults and caused PHP
 * to decide to disable JIT when detecting extensions that overwrote
 * zend_execute_ex.
 *
 * It only provides ZEND_USER_FUNCTIONS yet as it was assumed mechanisms already
 * exist to monitor internal functions by overwriting internal function
 * handlers.  This will be included in PHP 8.2: Registered
 * zend_observer_fcall_init handlers are now also called for internal functions.
 *
 * Without overwriting the execute function and therefore being responsible for
 * continuing the execution of ALL functions that we intercepted,  the agent is
 * provided zend_execute_data on each function start/end and is then able to use
 * it with our currently existing logic and instrumentation.
 */

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */

static void nr_show_fcall_init(zend_execute_data *execute_data) {
  const char* filename = nr_php_op_array_file_name(NR_OP_ARRAY);
  const char* function_name = nr_php_op_array_function_name(NR_OP_ARRAY);
  if (NR_OP_ARRAY->scope) {
    nrl_verbosedebug(NRL_AGENT, "fcall_init: scope={%.*s} function={" NRP_FMT_UQ "} @ " NRP_FMT_UQ ":%d",
        NRSAFELEN(nr_php_class_entry_name_length(NR_OP_ARRAY->scope)),
        nr_php_class_entry_name(NR_OP_ARRAY->scope),
        NRP_PHP(function_name ? function_name : "?"),
        NRP_FILENAME(filename), NR_OP_ARRAY->line_start);
  } else if (NR_OP_ARRAY->function_name) {
    nrl_verbosedebug(NRL_AGENT, "fcall_init: function={" NRP_FMT_UQ "} @ " NRP_FMT_UQ ":%d",
                     NRP_PHP(function_name), NRP_FILENAME(filename), NR_OP_ARRAY->line_start);
  } else if (NR_OP_ARRAY->filename) {
    nrl_verbosedebug(NRL_AGENT, "fcall_init: file={" NRP_FMT "}",
                    NRP_FILENAME(filename));
  } else {
    nrl_verbosedebug(NRL_AGENT, "fcall_init: ?");
  }
}
/*
 * Register the begin and end function handlers with the Observer API.
 */
static zend_observer_fcall_handlers nr_php_fcall_register_handlers(
    zend_execute_data* execute_data) {
  zend_observer_fcall_handlers handlers = {NULL, NULL};
  if (NULL == execute_data) {
    nrl_verbosedebug(NRL_AGENT, "%s: execute_data is NULL", __func__);
    return handlers;
  }
  if ((NULL == execute_data->func)
      || (ZEND_INTERNAL_FUNCTION == execute_data->func->type)) {
    nrl_verbosedebug(NRL_AGENT, "%s: execute_data->func is NULL or "
               "internal function", __func__);
    return handlers;
  }
  if (nrunlikely(NR_PHP_PROCESS_GLOBALS(special_flags).show_executes)) {
    nr_show_fcall_init(execute_data);
  }
  handlers.begin = nr_php_observer_fcall_begin;
  handlers.end = nr_php_observer_fcall_end;
  return handlers;
}

static inline zend_observer_fcall_handlers nr_php_observer_fcall_init(zend_execute_data* execute_data) {
  NRPROF_START;
  zend_observer_fcall_handlers handlers = nr_php_fcall_register_handlers(execute_data);
  NRPROF_STOP(FCALL_INIT);
  return handlers;
}

void nr_php_observer_no_op(zend_execute_data* execute_data NRUNUSED){};

void nr_php_observer_minit() {
  /*
   * Register the Observer API handlers.
   */
  zend_observer_fcall_register(nr_php_observer_fcall_init);

  /*
   * For Observer API with PHP 8+, we no longer need to ovewrwrite the zend
   * execute hook.  orig_execute is called various ways in various places, so
   * turn it into a no_op when using OAPI.
   */
  NR_PHP_PROCESS_GLOBALS(orig_execute) = nr_php_observer_no_op;
}

#endif
