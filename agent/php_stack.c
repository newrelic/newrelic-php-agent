/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file produces a JSON-formatted stack dump.
 */
#include "nr_axiom.h"
#include "php_includes.h"
#include "php_hash.h"
#include "php_agent.h"
#include "util_buffer.h"
#include "util_number_converter.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_logging.h"

#ifdef PHP7
#include "zend_generators.h"
#endif

static int nr_php_stack_iterator(zval* frame,
                                 nrobj_t* arr,
                                 zend_hash_key* key NRUNUSED TSRMLS_DC) {
  nrbuf_t* buf;
  zval* file;
  zval* function;
  zval* klass;
  zval* line;

  NR_UNUSED_TSRMLS;
  if (!nr_php_is_zval_valid_array(frame)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if (NULL != key && NR_PHP_STACKTRACE_LIMIT <= key->h) {
    nrl_debug(NRL_API, "Stack trace was too large, truncating");
    return ZEND_HASH_APPLY_STOP;
  }

  file = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "file");
  line = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "line");
  function = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "function");
  klass = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "class");

  /*
   * A utility buffer is used rather than fixed sized buffer on the stack to
   * avoid truncation.
   */
  buf = nr_buffer_create(1024, 1024);

  nr_buffer_add(buf, NR_PSTR(" in "));

  if (nr_php_is_zval_non_empty_string(klass)) {
    nr_buffer_add(buf, Z_STRVAL_P(klass), Z_STRLEN_P(klass));
    nr_buffer_add(buf, NR_PSTR("::"));
  }

  if (nr_php_is_zval_non_empty_string(function)) {
    nr_buffer_add(buf, Z_STRVAL_P(function), Z_STRLEN_P(function));
  } else {
    nr_buffer_add(buf, NR_PSTR("?"));
  }

  nr_buffer_add(buf, NR_PSTR(" called at "));

  if (nr_php_is_zval_non_empty_string(file)) {
    nr_buffer_add(buf, Z_STRVAL_P(file), Z_STRLEN_P(file));
  } else {
    nr_buffer_add(buf, NR_PSTR("?"));
  }

  if (nr_php_is_zval_valid_integer(line)) {
    char line_str[24];
    int line_str_len;

    line_str[0] = '\0';
    line_str_len
        = snprintf(line_str, sizeof(line_str), " (%ld)", (long)Z_LVAL_P(line));

    nr_buffer_add(buf, line_str, line_str_len);
  } else {
    nr_buffer_add(buf, NR_PSTR(" (?)"));
  }

  nr_buffer_add(buf, "\0", 1);

  nro_set_array_string(arr, 0, (const char*)nr_buffer_cptr(buf));

  nr_buffer_destroy(&buf);

  return ZEND_HASH_APPLY_KEEP;
}

static char* nr_php_backtrace_to_json_internal(zval* trace TSRMLS_DC) {
  nrobj_t* arr;
  char* json;
  int stack_trace_size = 0;

  if (0 == nr_php_is_zval_valid_array(trace)) {
    return NULL;
  }

  arr = nro_new_array();

  nr_php_zend_hash_zval_apply(Z_ARRVAL_P(trace),
                              (nr_php_zval_apply_t)nr_php_stack_iterator,
                              arr TSRMLS_CC);

  stack_trace_size = nr_php_zend_hash_num_elements(Z_ARRVAL_P(trace));
  if (NR_PHP_STACKTRACE_LIMIT <= stack_trace_size) {
    char buf[100];
    int lines_removed = stack_trace_size - NR_PHP_STACKTRACE_LIMIT;
    nrtxn_t* txn = NRPRG(txn);

    snprintf(
        buf, 100,
        "*** The stack trace was truncated here - %d line(s) were removed ***",
        lines_removed);
    nro_set_array_string(arr, 0, buf);
    if (NULL != txn) {
      nrm_force_add(txn->unscoped_metrics,
                    "Supportability/PHP/StackFramesRemoved", lines_removed);
    }
  }

  json = nro_to_json(arr);

  nro_delete(arr);

  return json;
}

char* nr_php_backtrace_to_json(zval* itrace TSRMLS_DC) {
  zval* trace;
  char* json;

  if (itrace) {
    return nr_php_backtrace_to_json_internal(itrace TSRMLS_CC);
  }

  trace = nr_php_backtrace(TSRMLS_C);
  json = nr_php_backtrace_to_json_internal(trace TSRMLS_CC);
  nr_php_zval_free(&trace);

  return json;
}

zval* nr_php_backtrace(TSRMLS_D) {
  zval* trace = NULL;
  int skip_last = 0;
  int limit = NR_PHP_BACKTRACE_LIMIT;
  int options = DEBUG_BACKTRACE_IGNORE_ARGS;

  trace = nr_php_zval_alloc();

#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  zend_fetch_debug_backtrace(trace, skip_last, options, limit TSRMLS_CC);
#else /* PHP < 5.4 */
  zend_fetch_debug_backtrace(trace, skip_last, options TSRMLS_CC);
  (void)limit;
#endif

  return trace;
}

char* nr_php_backtrace_callback(void) {
  TSRMLS_FETCH();

  return nr_php_backtrace_to_json(NULL TSRMLS_CC);
}

typedef struct _nr_php_frame_info_t {
  const char* class_name; /* scope name */
  const char* call_type;  /* scope resolution operator ("::", "->", or "") */
  const char*
      func_name; /* function or operator name (e.g. "foo", "eval", "include") */
  const char* file; /* file name of the call site */
  int line;         /* line number of the call site */

  /*
   * The following fields are only populated for closures.
   */

  const char* decl_file; /* file name of the declaration site */
  int decl_line;         /* starting line number of the declaration site */
} nr_php_frame_info_t;

#ifdef PHP7

static int nr_php_is_include_or_eval(zend_execute_data* ex) {
  zend_execute_data* prev;

  if ((NULL == ex) || (NULL == ex->prev_execute_data)) {
    return 0;
  }

  prev = ex->prev_execute_data;

  if (NULL == prev->func) {
    return 0;
  }
  if (0 == ZEND_USER_CODE(prev->func->common.type)) {
    return 0;
  }
  return prev->opline->opcode == ZEND_INCLUDE_OR_EVAL;
}

/*
 * Try to determine the execution context of the user code that was the
 * proximate cause of ex.
 */
static zend_execute_data* nr_php_backtrace_get_call_site(
    zend_execute_data* ex) {
  zend_execute_data* prev;

  if ((NULL == ex) || (NULL == ex->func)) {
    /* No active function. */
    return ex;
  }
  if (0 == ZEND_USER_CODE(ex->func->common.type)) {
    /* Active function is not a user function. */
    return ex;
  }

  prev = ex->prev_execute_data;

  if (NULL == prev) {
    /* Reached the bottom of the stack. */
    return ex;
  }
  if ((NULL == prev->func) || (0 == ZEND_USER_CODE(ex->func->common.type))) {
    /* Predecessor is not an active function, or not user code. */
    return ex;
  }

  switch (prev->opline->opcode) {
    case ZEND_DO_FCALL:
    case ZEND_DO_FCALL_BY_NAME:
    case ZEND_DO_ICALL:
    case ZEND_DO_UCALL:
    case ZEND_INCLUDE_OR_EVAL:
      return prev;
  }
  return ex;
}

static void nr_php_frame_info(nr_php_frame_info_t* info,
                              zend_execute_data* ex) {
  zend_execute_data* callsite;
  zend_function* func;
  zend_object* This;

  info->class_name = "";
  info->call_type = "";
  info->func_name = "";
  info->file = "";
  info->line = 0;
  info->decl_file = "";
  info->decl_line = 0;

  if (NULL == ex) {
    return;
  }

  info->func_name = "unknown";

  callsite = nr_php_backtrace_get_call_site(ex);
  if (callsite && callsite->func
      && ZEND_USER_CODE(callsite->func->common.type)) {
    info->file = ZSTR_VAL(callsite->func->op_array.filename);
    if (callsite->opline->opcode == ZEND_HANDLE_EXCEPTION) {
      if (EG(opline_before_exception)) {
        info->line = EG(opline_before_exception)->lineno;
      } else {
        info->line = callsite->func->op_array.line_end;
      }
    } else {
      info->line = callsite->opline->lineno;
    }
  }

  func = ex->func;
  if (NULL == func) {
    return;
  }

  /*
   * For closures, gather the file and line where the closure was declared
   * in addition to the file and line of the call site.
   */
  if ((ZEND_USER_FUNCTION == func->type)
      && (func->common.fn_flags & ZEND_ACC_CLOSURE)) {
    info->decl_file = ZSTR_VAL(func->op_array.filename);
    info->decl_line = func->op_array.line_start;
  }

  if (func->common.function_name) {
    info->func_name = ZSTR_VAL(func->common.function_name);

    This = Z_OBJ(ex->This);
    if (This) {
      info->call_type = "->";
      if (func->common.scope) {
        info->class_name = ZSTR_VAL(func->common.scope->name);
      } else {
        info->class_name = ZSTR_VAL(This->ce->name);
      }
    } else if (func->common.scope) {
      info->class_name = ZSTR_VAL(func->common.scope->name);
      info->call_type = "::";
    }
    return;
  }

  if (nr_php_is_include_or_eval(ex)) {
    switch (ex->prev_execute_data->opline->extended_value) {
      case ZEND_EVAL:
        info->func_name = "eval";
        break;
      case ZEND_INCLUDE:
        info->func_name = "include";
        break;
      case ZEND_REQUIRE:
        info->func_name = "require";
        break;
      case ZEND_INCLUDE_ONCE:
        info->func_name = "include_once";
        break;
      case ZEND_REQUIRE_ONCE:
        info->func_name = "require_once";
        break;
      default:
        info->func_name = "ZEND_INCLUDE_OR_EVAL";
        break;
    }
  }
}

#else /* PHP7 */

static void nr_php_frame_info(nr_php_frame_info_t* info,
                              zend_execute_data* ex TSRMLS_DC) {
  zend_function* func;

  info->class_name = "";
  info->call_type = "";
  info->func_name = "";
  info->file = "";
  info->line = 0;
  info->decl_file = "";
  info->decl_line = 0;

  if (NULL == ex) {
    return;
  }

  info->func_name = "unknown";
  func = ex->function_state.function;

  if (NULL == func) {
    return;
  }

  if (ex->op_array && ex->opline) {
    info->file = ex->op_array->filename;
    info->line = ex->opline->lineno;
  }

  /*
   * For closures, gather the file and line where the closure was declared
   * in addition to the file and line of the call site.
   */
  if ((ZEND_USER_FUNCTION == func->type)
      && (func->common.fn_flags & ZEND_ACC_CLOSURE)) {
    info->decl_file = func->op_array.filename;
    info->decl_line = func->op_array.line_start;
  }

  if (func->common.function_name) {
    info->func_name = func->common.function_name;

    if (ex->object) {
      info->call_type = "->";

      /*
       * Ignore the scope for closures, it's redundant given the file and
       * line where the closure was declared.
       */
      if (0 == (func->common.fn_flags & ZEND_ACC_CLOSURE)) {
        if (func->common.scope) {
          info->class_name = func->common.scope->name;
        } else {
          /*
           * A method was invoked, but the runtime did not set the scope?
           * It's unclear how/when this can happen, but the Zend Engine handles
           * this case, so handle it here too.
           */
          if (Z_OBJCE_P(ex->object) && Z_OBJCE_P(ex->object)->name) {
            info->class_name = Z_OBJCE_P(ex->object)->name;
          } else {
            info->class_name = "???";
          }
        }
      }
    } else if (func->common.scope) {
      info->call_type = "::";
      info->class_name = func->common.scope->name;
    }

    return;
  }

  if (ex->opline && ex->opline->opcode == ZEND_INCLUDE_OR_EVAL) {
    switch (ex->opline->extended_value) {
      case ZEND_EVAL:
        info->func_name = "eval";
        break;
      case ZEND_INCLUDE:
        info->func_name = "include";
        break;
      case ZEND_REQUIRE:
        info->func_name = "require";
        break;
      case ZEND_INCLUDE_ONCE:
        info->func_name = "include_once";
        break;
      case ZEND_REQUIRE_ONCE:
        info->func_name = "require_once";
        break;
      default:
        info->func_name = "ZEND_INCLUDE_OR_EVAL";
        break;
    }
  }
}

#endif /* PHP7 */

/*
Output format:

#0  c() called at [/tmp/include.php:10]
#1  b() called at [/tmp/include.php:6]
#2  a() called at [/tmp/include.php:17]
#3  include() called at [/tmp/test.php:3]
*/

void nr_php_backtrace_fd(int fd, int limit TSRMLS_DC) {
  nr_php_frame_info_t frame;
  char scratch[64];
  zend_execute_data* ex;
  int i;

  i = 0;
  ex = EG(current_execute_data);

  while (ex) {
#ifdef PHP7
    ex = zend_generator_check_placeholder_frame(ex);
#endif

    nr_php_frame_info(&frame, ex TSRMLS_CC);

    nr_write(fd, NR_PSTR("#"));
    nr_itoa(scratch, sizeof(scratch), i);
    nr_write(fd, scratch, nr_strlen(scratch));
    nr_write(fd, NR_PSTR(" "));

    if (frame.class_name && *frame.class_name) {
      nr_write(fd, frame.class_name, nr_strlen(frame.class_name));
      nr_write(fd, frame.call_type, nr_strlen(frame.call_type));
    }

    nr_write(fd, frame.func_name, nr_strlen(frame.func_name));
    nr_write(fd, NR_PSTR("()"));

    if (frame.file && *frame.file) {
      nr_write(fd, NR_PSTR(" called at ["));
      nr_write(fd, frame.file, nr_strlen(frame.file));
      nr_write(fd, NR_PSTR(":"));
      nr_itoa(scratch, sizeof(scratch), frame.line);
      nr_write(fd, scratch, nr_strlen(scratch));
      nr_write(fd, NR_PSTR("]"));
    }

    if (frame.decl_file && *frame.decl_file) {
      nr_write(fd, NR_PSTR(" declared at ["));
      nr_write(fd, frame.decl_file, nr_strlen(frame.decl_file));
      nr_write(fd, NR_PSTR(":"));
      nr_itoa(scratch, sizeof(scratch), frame.decl_line);
      nr_write(fd, scratch, nr_strlen(scratch));
      nr_write(fd, NR_PSTR("]"));
    }

    nr_write(fd, NR_PSTR("\n"));

    ex = ex->prev_execute_data;

    i++;
    if ((limit > 0) && (i >= limit)) {
      break;
    }
  }
}
