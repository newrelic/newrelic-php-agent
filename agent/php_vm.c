/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_vm.h"
#include "php_call.h"
#include "util_logging.h"

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */

/*
 * An entry in the previous_opcode_handlers table.
 *
 * The is_set flag is needed to correctly identify NULL opcode handlers that
 * were overwritten by the agent.
 */
typedef struct {
  bool is_set; /* True if the opcode handler was overwritten by the agent. */
  user_opcode_handler_t
      handler; /* The original opcode handler before the agent overwrote it. */
} nr_php_opcode_handler_entry_t;

/*
 * Opcode handlers are per-process, not per-request or per-thread, so in order
 * to track the opcode handlers we replaced and still call them, we have to
 * keep them in a per-process global.
 *
 * Note that this means that we should never write to previous_opcode_handlers
 * outside of MINIT and MSHUTDOWN.
 */
#define OPCODE_COUNT 256
static nr_php_opcode_handler_entry_t previous_opcode_handlers[OPCODE_COUNT];

/*
 * Purpose : Set a single opcode handler.
 *
 * Params  : 1. The opcode to set a handler for.
 *           2. The handler to add.
 */
static void nr_php_set_opcode_handler(zend_uchar opcode,
                                      user_opcode_handler_t handler) {
  nr_php_opcode_handler_entry_t previous
      = {.is_set = true, .handler = zend_get_user_opcode_handler(opcode)};

  /*
   * We want to store this even if it's NULL, because that allows us to restore
   * the original state on shutdown.
   */
  previous_opcode_handlers[opcode] = previous;

  if (SUCCESS != zend_set_user_opcode_handler(opcode, handler)) {
    /*
     * There's nothing much to be done, so just log an error and move on.
     */
    nrl_info(NRL_AGENT, "%s: error setting handler for opcode %u", __func__,
             (unsigned int)opcode);
  }
}

/*
 * Purpose : User opcode handler for the ZEND_DO_FCALL handler.
 *
 *           This function is written unusually for our code base: it uses the
 *           EXPECTED and UNEXPECTED macros heavily.
 *
 *           The reason here is performance: without wanting to re-establish the
 *           worst excesses of our old nr_php_execute_*() family of functions,
 *           this is on a hot path in the PHP 7 agent (every function call --
 *           internal or user function alike -- goes through here), and we want
 *           to minimise our overhead. Where there's a normal case, branch
 *           prediction macros are used to (hopefully) speed up execution by
 *           reducing the likelihood of a branch misprediction stall.
 *
 *           The logic here is fundamentally fairly simple: If the
 *           cufa_callback global is set, then we're instrumenting
 *           call_user_func_array() and should invoke it (provided we can pull
 *           the fields we need out of execute_data). After that, regardless of
 *           whether cufa_callback was set, we should invoke any previous
 *           opcode handler for the same opcode, otherwise return
 *           ZEND_USER_OPCODE_DISPATCH to the Zend Engine, which signals that
 *           the Zend Engine should execute the opline normally.
 */
static int nr_php_handle_cufa_fcall(zend_execute_data* execute_data) {
  nrphpcufafn_t cufa_callback = NRPRG(cufa_callback);
  zend_uchar opcode;
  nr_php_opcode_handler_entry_t prev_handler;
  const zend_op* prev_opline;

  /*
   * We should have execute_data (and there isn't a realistic case where we
   * wouldn't other than memory corruption), so if we don't, we should bail as
   * quickly as possible. We can't arrange to dispatch to the previous opcode
   * handler as we need execute_data to be able to get the opcode, so let's
   * simply return and let the Zend Engine figure out what to do.
   */
  if (UNEXPECTED(NULL == execute_data)) {
    return ZEND_USER_OPCODE_DISPATCH;
  }

  /*
   * If we don't have a call_user_func_array() callback installed, we don't
   * need to instrument anything. Let's choose our own adventure and jump to
   * the last page.
   */
  if (EXPECTED(NULL == cufa_callback)) {
    goto call_previous_and_return;
  }

  /*
   * If we don't have haven't instrumented hooks that require this, skip to the
   * end.
   */
  if (false == NRPRG(check_cufa)) {
    goto call_previous_and_return;
  }

  /*
   * Since we're in the middle of a function call, the Zend Engine is actually
   * only partway through constructing the new function frame. As a result, it
   * hasn't yet replaced the execute_data global with the details of the
   * function that are being called, but that's available through the "call"
   * field.
   *
   * If it's not available, we can't instrument, since we don't know what's
   * getting called.
   */
  if (UNEXPECTED((NULL == execute_data->call)
                 || (NULL == execute_data->call->func))) {
    goto call_previous_and_return;
  }

  /*
   * An internal function being invoked via call_user_func_array() will still
   * be instrumented through the normal php_internal_instrument.c mechanisms,
   * so we don't need to do anything here.
   */
  if (ZEND_USER_FUNCTION != execute_data->call->func->type) {
    goto call_previous_and_return;
  }

  /*
   * To actually determine whether this is a call_user_func_array() call we
   * have to look at one of the previous opcodes. ZEND_DO_FCALL will never be
   * the first opcode in an op array -- minimally, there is always at least a
   * ZEND_INIT_FCALL before it -- so this is safe.
   *
   * When PHP 7 flattens a call_user_func_array() call into direct opcodes, it
   * uses ZEND_SEND_ARRAY to send the arguments in a single opline, and that
   * opcode is the opcode before the ZEND_DO_FCALL. Therefore, if we see
   * ZEND_SEND_ARRAY, we know it's call_user_func_array(). The relevant code
   * can be found at:
   * https://github.com/php/php-src/blob/php-7.0.19/Zend/zend_compile.c#L3082-L3098
   * https://github.com/php/php-src/blob/php-7.1.5/Zend/zend_compile.c#L3564-L3580
   *
   * In PHP 8, sometimes a ZEND_CHECK_UNDEF_ARGS opcode is added after the call
   * to ZEND_SEND_ARRAY and before ZEND_DO_FCALL so we need to sometimes look
   * back two opcodes instead of just one.
   *
   * Note that this heuristic will fail if the Zend Engine ever starts
   * compiling flattened call_user_func_array() calls differently. PHP 7.2 made
   * a change, but it only optimized array_slice() calls, which as an internal
   * function won't get this far anyway.) We can disable this behaviour by
   * setting the ZEND_COMPILE_NO_BUILTINS compiler flag, but since that will
   * cause additional performance overhead, this should be considered a last
   * resort.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
  prev_opline = execute_data->opline - 1;
  if (ZEND_CHECK_UNDEF_ARGS == prev_opline->opcode) {
    prev_opline = execute_data->opline - 2;
  }
#else
  prev_opline = execute_data->opline - 1;
#endif

  if (ZEND_SEND_ARRAY == prev_opline->opcode) {
    if (UNEXPECTED((NULL == execute_data->call)
                   || (NULL == execute_data->call->func))) {
      nrl_verbosedebug(NRL_AGENT, "%s: cannot get function from call",
                       __func__);
      return ZEND_USER_OPCODE_DISPATCH;
    }

    nr_php_call_user_func_array_handler(cufa_callback, execute_data->call->func,
                                        execute_data);
  }

  goto call_previous_and_return;

call_previous_and_return:
  /*
   * To call any previous user opcode handler, we have to first get the opline
   * so we can get the opcode.
   */
  if (UNEXPECTED(NULL == execute_data->opline)) {
    return ZEND_USER_OPCODE_DISPATCH;
  }
  opcode = execute_data->opline->opcode;

  /*
   * Now we have the opcode, let's see if there's a handler and, if so, call
   * it.
   */
  prev_handler = previous_opcode_handlers[opcode];
  if (UNEXPECTED(prev_handler.is_set && prev_handler.handler)) {
    return (prev_handler.handler)(execute_data);
  }

  /*
   * If there wasn't a handler, we'll return ZEND_USER_OPCODE_DISPATCH to tell
   * the Zend Engine to execute the opline normally.
   */
  return ZEND_USER_OPCODE_DISPATCH;
}

void nr_php_set_opcode_handlers(void) {
  nr_php_set_opcode_handler(ZEND_DO_FCALL, nr_php_handle_cufa_fcall);
}

void nr_php_remove_opcode_handlers(void) {
  size_t i;

  for (i = 0; i < OPCODE_COUNT; i++) {
    if (previous_opcode_handlers[i].is_set) {
      zend_set_user_opcode_handler(i, previous_opcode_handlers[i].handler);
      previous_opcode_handlers[i].is_set = false;
    }
  }
}

#else

void nr_php_set_opcode_handlers(void) {}

void nr_php_remove_opcode_handlers(void) {}

#endif /* PHP 7.0+ */
