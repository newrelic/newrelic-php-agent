/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_error.h"
#include "php_globals.h"
#include "php_hash.h"
#include "nr_rum.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#include <Zend/zend_exceptions.h>

#include "php_variables.h"

static zval* nr_php_get_zval_object_property_with_class_internal(
    zval* object,
    zend_class_entry* ce,
    const char* cname TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  /*
   * Although the below notes still apply in principle, PHP 7 additionally broke
   * the API for zend_read_property by adding an rv parameter, which is used to
   * provide storage for the return value in the case that a __get() magic
   * method is called. It is unclear why zend_read_property() doesn't do this
   * itself.
   *
   * For now, we shall do what every caller of zend_read_property in php-src/ext
   * does, which is to provide a pointer to a value that isn't subsequently
   * used.
   */
  zval* data;
  zval rv;
  zend_bool silent = 1;

  data = zend_read_property(ce, ZVAL_OR_ZEND_OBJECT(object), cname,
                            nr_strlen(cname), silent, &rv);
  if (&EG(uninitialized_zval) != data) {
    return data;
  }
#else
  /*
   * This attempts to read uninitialized (or non existing) properties always
   * return uninitialized_zval_ptr, even in the case where we read a property
   * during pre-hook time on a constructor.
   */
  zend_bool silent = 1; /* forces BP_VAR_IS semantics */
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  zval* data = zend_read_property(ce, object, cname, nr_strlen(cname),
                                  silent TSRMLS_CC);
#else
  zval* data = zend_read_property(ce, object, (char*)nr_remove_const(cname),
                                  nr_strlen(cname), silent TSRMLS_CC);
#endif /* PHP >= 5.4 */

  if (EG(uninitialized_zval_ptr) != data) {
    return data;
  }
#endif /* PHP7+ */

  return NULL;
}

zval* nr_php_get_zval_object_property(zval* object,
                                      const char* cname TSRMLS_DC) {
  if ((NULL == object) || (NULL == cname) || (0 == cname[0])) {
    return NULL;
  }

  if (nr_php_is_zval_valid_object(object)) {
    return nr_php_get_zval_object_property_with_class_internal(
        object, Z_OBJCE_P(object), cname TSRMLS_CC);
  } else if (IS_ARRAY == Z_TYPE_P(object)) {
    zval* data;

    data = nr_php_zend_hash_find(Z_ARRVAL_P(object), cname);
    if (data) {
      return data;
    }
  }

  return NULL;
}

zval* nr_php_get_zval_base_exception_property(zval* exception,
                                              const char* cname) {
  zend_class_entry* ce;

  if ((NULL == exception) || (NULL == cname) || (0 == cname[0])) {
    return NULL;
  }

  if (nr_php_is_zval_valid_object(exception)) {
    if (nr_php_error_zval_is_exception(exception)) {
    /* 
     * This is inline with what the php source code does to extract properties from
     * errors and exceptions. Without getting the base class entry, certain values
     * are incorrect for either errors/exceptions.
     */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
      ce = zend_get_exception_base(Z_OBJ_P(exception));
#else
      ce = zend_get_exception_base(exception);
#endif
      return nr_php_get_zval_object_property_with_class_internal(
          exception, ce, cname TSRMLS_CC);
    }
  }
  return NULL;
}

zval* nr_php_get_zval_object_property_with_class(zval* object,
                                                 zend_class_entry* ce,
                                                 const char* cname TSRMLS_DC) {
  if ((!nr_php_is_zval_valid_object(object)) || (NULL == ce) || (NULL == cname)
      || ('\0' == cname[0])) {
    return NULL;
  }

  return nr_php_get_zval_object_property_with_class_internal(object, ce,
                                                             cname TSRMLS_CC);
}

int nr_php_object_has_method(zval* object, const char* lcname TSRMLS_DC) {
  zend_class_entry* ce;
  int namelen;
  char* vname;

  if (nrunlikely((0 == lcname) || (0 == lcname[0]))) {
    return 0;
  }

  if (nrunlikely(0 == nr_php_is_zval_valid_object(object))) {
    return 0;
  }

  namelen = nr_strlen(lcname);
  vname = (char*)nr_alloca(namelen + 1);
  nr_strcpy(vname, lcname);

  ce = Z_OBJCE_P(object);
  if (nr_php_zend_hash_exists(&ce->function_table, vname)) {
    return 1;
  } else {
    if (NULL == Z_OBJ_HT_P(object)->get_method) { /* nr_php_object_has_method */
      return 0;
    } else {
      void* func;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
      zend_string* name_str = zend_string_init(vname, namelen, 0);

      func = (void*)Z_OBJ_HT_P(object)->get_method(&Z_OBJ_P(object), name_str,
                                                   NULL TSRMLS_CC);

      zend_string_release(name_str);
#elif ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
      /*
       * This can leak if the object has a __call() method, as in that situation
       * only, zend_std_get_method() will indirectly allocate a new
       * zend_function in zend_get_user_call_function().
       *
       * We can't easily detect this, and the zend_function() is allocated via
       * emalloc(), so we're just going to let this slide and let the Zend
       * Engine clean it up at RSHUTDOWN. Note that this needs to be suppressed
       * in Valgrind, though.
       */
      func = (void*)Z_OBJ_HT_P(object)->get_method(
          &object, vname, namelen,
          NULL TSRMLS_CC); /* nr_php_object_has_method */
#else /* PHP < 5.4 */
      func = (void*)Z_OBJ_HT_P(object)->get_method(
          &object, vname, namelen TSRMLS_CC); /* nr_php_object_has_method */
#endif

      if (NULL == func) {
        return 0;
      }
      return 1;
    }
  }
}

int nr_php_object_has_concrete_method(zval* object,
                                      const char* lcname TSRMLS_DC) {
  if (nrunlikely((NULL == lcname) || (0 == lcname[0]))) {
    return 0;
  }

  if (nrunlikely(0 == nr_php_is_zval_valid_object(object))) {
    return 0;
  }

  return nr_php_zend_hash_exists(&Z_OBJCE_P(object)->function_table, lcname);
}

zend_function* nr_php_find_function(const char* name TSRMLS_DC) {
  if (NULL == name) {
    return NULL;
  }

  /*
   * Both PHP 5 and PHP 7 store zend_function * in the function table, so we
   * can simply directly return the result of zend_hash_find_ptr.
   */
  return (zend_function*)nr_php_zend_hash_find_ptr(EG(function_table), name);
}

/*
 * PHP 5 stores a double pointer to a zend_class_entry in the class table,
 * whereas PHP 7 only uses a single level of indirection.
 */
zend_class_entry* nr_php_find_class(const char* name TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  if (NULL == name) {
    return NULL;
  }

  return (zend_class_entry*)nr_php_zend_hash_find_ptr(EG(class_table), name);
#else
  zend_class_entry** ce_ptr = 0;

  if (0 == name) {
    return 0;
  }

  ce_ptr = (zend_class_entry**)nr_php_zend_hash_find_ptr(EG(class_table), name);

  return ce_ptr ? *ce_ptr : NULL;
#endif /* PHP7+ */
}

zend_function* nr_php_find_class_method(const zend_class_entry* klass,
                                        const char* name) {
  if (0 == klass) {
    return 0;
  }
  if (0 == name) {
    return 0;
  }

  /*
   * Both PHP 5 and PHP 7 store zend_function in the function table, so we
   * can simply directly return the result of zend_hash_find_ptr.
   */
  return (zend_function*)nr_php_zend_hash_find_ptr(&(klass->function_table),
                                                   name);
}

int nr_php_class_entry_instanceof_class(const zend_class_entry* ce,
                                        const char* class_name TSRMLS_DC) {
  int found = 0;

  if (NULL != ce) {
    char* class_name_lower = nr_string_to_lowercase(class_name);
    zend_class_entry* class_name_ce
        = nr_php_find_class(class_name_lower TSRMLS_CC);

    if (NULL != class_name_ce) {
      found = instanceof_function(ce, class_name_ce TSRMLS_CC);
    }

    nr_free(class_name_lower);
  }

  return found;
}

int nr_php_object_instanceof_class(const zval* object,
                                   const char* class_name TSRMLS_DC) {
  if (!nr_php_is_zval_valid_object(object)) {
    return 0;
  }

  return nr_php_class_entry_instanceof_class(Z_OBJCE_P(object),
                                             class_name TSRMLS_CC);
}

zend_function* nr_php_zval_to_function(zval* zv TSRMLS_DC) {
  zend_fcall_info_cache fcc;

  if (NULL == zv) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  if (zend_is_callable_ex(zv, NULL, 0, NULL, &fcc, NULL)) {
    return fcc.function_handler;
  }
#else
  if (zend_is_callable_ex(zv, NULL, 0, NULL, NULL, &fcc, NULL TSRMLS_CC)) {
    return fcc.function_handler;
  }
#endif /* PHP7+ */

  return NULL;
}

zend_execute_data* nr_get_zend_execute_data(NR_EXECUTE_PROTO TSRMLS_DC) {
  NR_UNUSED_FUNC_RETURN_VALUE;

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP 8.0+ and OAPI */

  /*
   * There is no other recourse.  We must return what OAPI gave us.  This should
   * theoretically never be NULL since we check for NULL before calling the
   * handlers; however, if it was NULL, there is nothing we can do about it.
   */
  return execute_data;
#endif
  zend_execute_data* ptrg
      = EG(current_execute_data); /* via zend engine global data structure */
  NR_UNUSED_SPECIALFN;
  NR_UNUSED_FUNC_RETURN_VALUE;
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
  {
    /*
     * ptra is argument passed in to us, it might be NULL if the caller doesn't
     * have that info.
     */
    zend_execute_data* ptra = execute_data;
    if (NULL != ptra) {
      return ptra;
    } else {
      return ptrg;
    }
  }
#else /* PHP < 5.5 */
  return ptrg;
#endif
}

/*
 * Purpose : Return a pointer to the arguments for the true frame that is
 *           legitimate_frame_delta frames down from the top.
 *
 * Params  : 1. The number of true frames to walk down.  0 means "top of stack".
 *              The PHP5.5 half-formed stack top frame (with a null arguments
 *              block) is ignored.
 *
 *           2. The execution context supplied by the zend engine; this changes
 *              from PHP5.4 to PHP5.5, hence the use of macros.
 *
 * See this web page discussion migrating from 5.4 to 5.5 (July 22, 2013)
 *   http://www.php.net/manual/en/migration55.internals.php
 *
 * If the arguments pointer is null, it represents either a half-formed frame
 * (or the base of the call stack). Go up one frame, and use the arguments
 * vector from there. The two functions appear to be identical.
 *
 * For PHP 5.4 (and presumably earlier), this additional stack frame isn't
 * there.
 *
 * See the picture near line 1525 of PHP 5.5.3 zend_execute.c
 */
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO && !defined(PHP7) \
    && !defined(PHP8) /* PHP 5.5 and 5.6 */
static void** nr_php_get_php55_stack_arguments(int legitimate_frame_delta,
                                               NR_EXECUTE_PROTO TSRMLS_DC) {
  zend_execute_data* ex;
  void** arguments;
  int i;

  ex = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  arguments = ex->function_state.arguments;
  if (NULL == arguments) {
    ex = ex->prev_execute_data; /* discard top partially formed frame */
  }

  if (NULL == ex) {
    return NULL;
  }

  arguments = ex->function_state.arguments;
  if (NULL == arguments) {
    return NULL; /* PHP stack appears to be be malformed */
  }

  for (i = 0; i < legitimate_frame_delta; i++) {
    ex = ex->prev_execute_data;
    if (NULL == ex) {
      return NULL; /* No caller; we're at the bottom of the stack */
    }
    arguments = ex->function_state.arguments;
    if (NULL == arguments) {
      return NULL; /* PHP stack appears to be be malformed */
    }
  }

  return arguments;
}
#endif

#if !defined(PHP7) && !defined(PHP8) /* PHP 5.5 and 5.6 */
/*
 * Use detailed zend specific knowledge of the interpreter stack
 * to read the argument vector.
 * Here, the 'h' suffix means "hackery".
 */
static zval* nr_php_get_user_func_arg_via_h(int requested_arg_index,
                                            int* arg_count_p,
                                            NR_EXECUTE_PROTO TSRMLS_DC) {
  void** p = 0;
  zval** argp = 0;
  zval* arg = 0;

  NR_UNUSED_SPECIALFN;

  if (NULL == arg_count_p) {
    return NULL;
  }
  *arg_count_p = -1;
  if (NULL == nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS TSRMLS_CC)) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
  p = nr_php_get_php55_stack_arguments(0, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == p) {
    return NULL;
  }
  *arg_count_p = (int)(zend_uintptr_t)*p;
#else
  p = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS TSRMLS_CC)
          ->function_state.arguments;
  if (NULL == p) {
    return NULL;
  }
  *arg_count_p = (int)(zend_uintptr_t)*p;
#endif /* PHP >= 5.5 */

  if (requested_arg_index > *arg_count_p) {
    return NULL;
  }

  argp = ((zval**)p) - *arg_count_p + requested_arg_index - 1;
  if (NULL == argp) {
    return NULL;
  }

  arg = *argp;
  return arg;
}
#endif /* !PHP7 && !PHP8*/

/*
 * NOTICE: requested_arg_index is a 1-based value, not a 0-based value!
 */
zval* nr_php_get_user_func_arg(size_t requested_arg_index,
                               NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* arg_via_h = 0;
  int arg_count_via_h = -1;

  NR_UNUSED_FUNC_RETURN_VALUE;

  if (requested_arg_index < 1) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  (void)arg_count_via_h;

  if (requested_arg_index > ZEND_CALL_NUM_ARGS(execute_data)) {
    return NULL;
  }

  arg_via_h = ZEND_CALL_ARG(execute_data, requested_arg_index);
#else
  arg_via_h = nr_php_get_user_func_arg_via_h(
      requested_arg_index, &arg_count_via_h, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
#endif /* PHP7+ */

  return arg_via_h;
}

size_t nr_php_get_user_func_arg_count(NR_EXECUTE_PROTO TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  NR_UNUSED_FUNC_RETURN_VALUE;
  return (size_t)ZEND_CALL_NUM_ARGS(execute_data);
#else
  int arg_count_via_h = -1;

  if (NULL
      == nr_php_get_user_func_arg_via_h(1, &arg_count_via_h,
                                        NR_EXECUTE_ORIG_ARGS TSRMLS_CC)) {
    return 0;
  } else if (arg_count_via_h < 0) {
    nrl_verbosedebug(NRL_AGENT, "%s: unexpected argument count %d", __func__,
                     arg_count_via_h);
    return 0;
  }

  return (size_t)arg_count_via_h;
#endif /* PHP7+ */
}

zend_execute_data* nr_php_get_caller_execute_data(NR_EXECUTE_PROTO,
                                                  ssize_t offset TSRMLS_DC) {
  zend_execute_data* ced;
  ssize_t i;

  NR_UNUSED_SPECIALFN;
  NR_UNUSED_TSRMLS;

#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
  ced = execute_data;

  if (NULL == ced) {
    ced = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }
#else /* PHP < 5.5 */
  ced = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
#endif

  for (i = 0; i < offset; i++) {
    if (NULL == ced) {
      return NULL;
    }

    ced = ced->prev_execute_data;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  if ((NULL == ced) || (NULL == ced->opline)) {
    return NULL;
  }
#else
  if ((NULL == ced) || (NULL == ced->op_array)) {
    return NULL;
  }
#endif /* PHP7+ */

  if ((ZEND_DO_FCALL != ced->opline->opcode)
      && (ZEND_DO_FCALL_BY_NAME != ced->opline->opcode)) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  if (NULL == ced->func) {
    return NULL;
  }
#else
  if (0 == ced->function_state.function) {
    return NULL;
  }
#endif /* PHP7+ */

  return ced;
}

const zend_function* nr_php_get_caller(NR_EXECUTE_PROTO,
                                       ssize_t offset TSRMLS_DC) {
  const zend_execute_data* ped
      = nr_php_get_caller_execute_data(NR_EXECUTE_ORIG_ARGS, offset TSRMLS_CC);

  if (NULL == ped) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  return ped->func;
#else
  return ped->function_state.function;
#endif /* PHP7+ */
}

zval* nr_php_get_active_php_variable(const char* name TSRMLS_DC) {
  HashTable* table;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  table = zend_rebuild_symbol_table();

  /*
   * Variables declared at compile time within the scope are stored as compiled
   * variables within the execution frame, and the symbol table will include
   * only an IS_INDIRECT variable pointing to that variable. As a result, we
   * need to use nr_php_zval_direct() to get the actual variable the caller
   * wants to be consistent with PHP 5.
   *
   * For more information, see:
   * https://nikic.github.io/2015/06/19/Internal-value-representation-in-PHP-7-part-2.html#indirect-zvals
   */
  return nr_php_zval_direct(nr_php_zend_hash_find(table, name));
#else
  table = EG(active_symbol_table);
  return nr_php_zend_hash_find(table, name);
#endif /* PHP7+ */
}

int nr_php_silence_errors(TSRMLS_D) {
  int error_reporting = EG(error_reporting);

  EG(error_reporting) = 0;

  return error_reporting;
}

void nr_php_restore_errors(int error_reporting TSRMLS_DC) {
  EG(error_reporting) = error_reporting;
}

zval* nr_php_get_constant(const char* name TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  zval* constant;
  zval* copy = NULL;
  zend_string* name_str;

  if (NULL == name) {
    return NULL;
  }

  name_str = zend_string_init(name, nr_strlen(name), 0);
  constant = zend_get_constant(name_str);
  zend_string_release(name_str);

  if (NULL == constant) {
    return NULL;
  }

  /*
   * For consistency with PHP 5, we'll copy the constant to a new zval.
   */
  copy = nr_php_zval_alloc();
  ZVAL_DUP(copy, constant);

  return copy;
#else
  nr_string_len_t len;
  int rv;
  zval* constant = NULL;

  if (NULL == name) {
    return NULL;
  }

  len = nr_strlen(name);
  constant = nr_php_zval_alloc();

  /* zend_get_constant returns 0 and 1 (not SUCCESS or FAILURE) */
  rv = zend_get_constant(name, len, constant TSRMLS_CC);
  if (0 == rv) {
    nr_php_zval_free(&constant);
    return NULL;
  }

  return constant;
#endif /* PHP7+ */
}

zval* nr_php_get_class_constant(const zend_class_entry* ce, const char* name) {
#if ZEND_MODULE_API_NO >= ZEND_7_1_X_API_NO
  zend_class_constant* constant = NULL;
  zval* copy = NULL;

  if (NULL == ce) {
    return NULL;
  }

  constant = nr_php_zend_hash_find_ptr(&(ce->constants_table), name);

  if (constant) {
    copy = nr_php_zval_alloc();
    ZVAL_DUP(copy, &constant->value);
  }

  return copy;
#else
  zval* constant = NULL;
  zval* copy = NULL;

  if (NULL == ce) {
    return NULL;
  }

  constant = nr_php_zend_hash_find(&(ce->constants_table), name);

  if (constant) {
    copy = nr_php_zval_alloc();

    /*
     * PHP 7.0 usually returns an IS_REF. We need to unwrap to ensure that we
     * duplicate the concrete value, otherwise the caller will end up freeing a
     * value that it doesn't own, and bad things will happen.
     */
    ZVAL_DUP(copy, nr_php_zval_real_value(constant));
  }

  return copy;
#endif
}

char* nr_php_get_object_constant(zval* app, const char* name) {
  char* retval = NULL;
  zval* version = NULL;
  zend_class_entry* ce = NULL;

  if (NULL == name || 0 >= nr_strlen(name)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application has NULL object name",
                     __func__);
    return NULL;
  }

  if (0 == nr_php_is_zval_valid_object(app)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application object is invalid",
                     __func__);
    return NULL;
  }

  ce = Z_OBJCE_P(app);
  if (NULL == ce) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application has NULL class entry",
                     __func__);
    return NULL;
  }

  version = nr_php_get_class_constant(ce, name);
  if (NULL == version) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application does not have %s",
                     __func__, name);
    return NULL;
  }

  if (nr_php_is_zval_valid_string(version)) {
    retval = nr_strndup(Z_STRVAL_P(version), Z_STRLEN_P(version));
  } else if (nr_php_is_zval_valid_integer(version)) {
    zend_string* zstr = zend_long_to_str(Z_LVAL_P(version));
    retval = nr_strndup(ZSTR_VAL(zstr), ZSTR_LEN(zstr));
    zend_string_release(zstr);
  } else {
    nrl_verbosedebug(
        NRL_FRAMEWORK,
        "%s: expected VERSION to be a valid string or int, got type %d",
        __func__, Z_TYPE_P(version));
  }

  nr_php_zval_free(&version);
  return retval;
}

int nr_php_is_zval_named_constant(const zval* zv, const char* name TSRMLS_DC) {
  int is_equal = 0;
  zval* constant;

  if ((NULL == zv) || (IS_LONG != Z_TYPE_P(zv)) || (NULL == name)) {
    return 0;
  }

  constant = nr_php_get_constant(name TSRMLS_CC);
  if (NULL == constant) {
    return 0;
  }
  if ((IS_LONG == Z_TYPE_P(constant)) && (Z_LVAL_P(zv) == Z_LVAL_P(constant))) {
    is_equal = 1;
  }

  nr_php_zval_free(&constant);
  return is_equal;
}

int nr_php_zend_is_auto_global(const char* name, size_t name_len TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  zend_bool rv;
  zend_string* zs = zend_string_init(name, name_len, 0);

  rv = zend_is_auto_global(zs);

  zend_string_free(zs);
  return rv;
#else
  return (int)zend_is_auto_global(name, (int)name_len TSRMLS_CC);
#endif /* PHP7+ */
}

const char* nr_php_use_license(const char* api_license TSRMLS_DC) {
  const char* lic_to_use = api_license;

  if ((NULL == lic_to_use) || ('\0' == lic_to_use[0])) {
    lic_to_use = NRINI(license);
  }

  if ((NULL == lic_to_use) || ('\0' == lic_to_use[0])) {
    lic_to_use = NR_PHP_PROCESS_GLOBALS(upgrade_license_key);
  }

  if (NR_LICENSE_SIZE == nr_strlen(lic_to_use)) {
    return lic_to_use;
  }
  return NULL;
}

char* nr_php_get_server_global(const char* name TSRMLS_DC) {
  zval* data = NULL;
  zval* global = NULL;

  if (NULL == name) {
    return NULL;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  global = &(PG(http_globals)[TRACK_VARS_SERVER]);
#else
  global = PG(http_globals)[TRACK_VARS_SERVER];
#endif

  if (!nr_php_is_zval_valid_array(global)) {
    return NULL;
  }

  if (NULL == Z_ARRVAL_P(global)) {
    return NULL;
  }

  data = nr_php_zend_hash_find(Z_ARRVAL_P(global), name);

  if (!nr_php_is_zval_non_empty_string(data)) {
    return NULL;
  }

  return nr_strndup(Z_STRVAL_P(data), Z_STRLEN_P(data));
}

int nr_php_extension_loaded(const char* name) {
  int found = 0;
  char* lcname;

  if (NULL == name) {
    return 0;
  }

  lcname = nr_string_to_lowercase(name);
  found = nr_php_zend_hash_exists(&module_registry, lcname);
  nr_free(lcname);

  return found;
}

#define CALLABLE_OBJECT_UNKNOWN "(unknown)"

/*
 * Purpose : Create a single string name from a callable array.
 *
 * Params  : 1. The callable array.
 *
 * Returns : A string containing the name, or NULL if the callable array is
 *           malformed. The caller must nr_free the string after use.
 */
static char* nr_php_callable_array_to_string(zval* callable TSRMLS_DC) {
  zval* function;
  char* name = NULL;
  size_t num_elements = nr_php_zend_hash_num_elements(Z_ARRVAL_P(callable));
  zval* scope;
  size_t function_len = 0;

  if (2 != num_elements) {
    nrl_verbosedebug(NRL_TXN,
                     "nr_php_callable_array_to_string: unexpected number of "
                     "elements in callable array: got %zu, expected 2",
                     num_elements);
    return NULL;
  }

  scope = nr_php_zend_hash_index_find(Z_ARRVAL_P(callable), 0);
  if (NULL == scope) {
    /*
     * This is a warning rather than a verbose debug as we just checked the
     * length above, so this would indicate that something is wrong with the
     * hash table.
     */
    nrl_warning(NRL_TXN,
                "nr_php_callable_array_to_string: finding element 0 of a "
                "callable array with 2 elements failed");
    return NULL;
  }

  function = nr_php_zend_hash_index_find(Z_ARRVAL_P(callable), 1);
  if (NULL == function) {
    /*
     * This is a warning rather than a verbose debug as we just checked the
     * length above, so this would indicate that something is wrong with the
     * hash table.
     */
    nrl_warning(NRL_TXN,
                "nr_php_callable_array_to_string: finding element 1 of a "
                "callable array with 2 elements failed");
    return NULL;
  }

  if (!nr_php_is_zval_valid_string(function)) {
    nrl_verbosedebug(NRL_TXN, "%s: unexpected type for function: got %d",
                     __func__, Z_TYPE_P(function));
    return NULL;
  }

  function_len = Z_STRLEN_P(function);

  if (nr_php_is_zval_valid_string(scope)) {
    /* This is a static method call; eg ['Class', 'method']. */
    size_t scope_len = Z_STRLEN_P(scope);

    name = (char*)nr_malloc(scope_len + function_len + 3);
    nr_strxcpy(name, Z_STRVAL_P(scope), scope_len);
    nr_strcat(name, "::");
    nr_strncat(name, Z_STRVAL_P(function), function_len);
  } else if (nr_php_is_zval_valid_object(scope)) {
    /* This is a normal method call; eg [$object, 'method']. */
    zend_class_entry* ce = Z_OBJCE_P(scope);
    const char* class_name = CALLABLE_OBJECT_UNKNOWN;
    size_t class_name_len = sizeof(CALLABLE_OBJECT_UNKNOWN);

    if (NULL != ce) {
      class_name = nr_php_class_entry_name(ce);
      class_name_len = nr_php_class_entry_name_length(ce);
    } else {
      nrl_warning(NRL_TXN,
                  "nr_php_callable_array_to_string: object does not have a "
                  "class entry");
    }

    name = (char*)nr_malloc(class_name_len + function_len + 3);
    nr_strxcpy(name, class_name, class_name_len);
    nr_strcat(name, "->");
    nr_strncat(name, Z_STRVAL_P(function), function_len);
  } else {
    nrl_verbosedebug(
        NRL_TXN,
        "nr_php_callable_array_to_string: unexpected type for scope: got %d",
        Z_TYPE_P(scope));
    return NULL;
  }

  return name;
}

char* nr_php_callable_to_string(zval* callable TSRMLS_DC) {
  char* name = NULL;

  if (NULL == callable) {
    nrl_verbosedebug(
        NRL_TXN,
        "nr_php_callable_to_string: cannot create name from NULL callable");
    return NULL;
  }

  if (nr_php_is_zval_valid_string(callable)) {
    name = nr_strndup(Z_STRVAL_P(callable), Z_STRLEN_P(callable));
  } else if (nr_php_is_zval_valid_array(callable)) {
    name = nr_php_callable_array_to_string(callable TSRMLS_CC);
  } else if (nr_php_is_zval_valid_object(callable)) {
    zend_class_entry* ce = Z_OBJCE_P(callable);

    if (NULL != ce) {
      name = nr_strndup(nr_php_class_entry_name(ce),
                        nr_php_class_entry_name_length(ce));
    } else {
      nrl_warning(
          NRL_TXN,
          "nr_php_callable_to_string: object does not have a class entry");
      name = nr_strdup(CALLABLE_OBJECT_UNKNOWN);
    }
  } else {
    nrl_verbosedebug(NRL_TXN,
                     "nr_php_callable_to_string: invalid callable of type %d",
                     Z_TYPE_P(callable));
  }

  return name;
}

static int nr_php_filter_class_methods(zend_function* func,
                                       const zend_class_entry* iface_ce,
                                       zend_hash_key* hash_key NRUNUSED
                                           TSRMLS_DC) {
  NR_UNUSED_TSRMLS

  if (iface_ce == func->common.scope) {
    return ZEND_HASH_APPLY_REMOVE;
  }

  return ZEND_HASH_APPLY_KEEP;
}

void nr_php_remove_interface_from_class(zend_class_entry* class_ce,
                                        const zend_class_entry* iface_ce
                                            TSRMLS_DC) {
  zend_uint i;

  if ((NULL == class_ce) || (NULL == iface_ce)) {
    return;
  }

  /*
   * The approach here is basically stolen wholesale from (the BSD licenced)
   * runkit: remove the interface class entry from Subscriber's class entry
   * interface list, then remove any methods inherited from that interface.
   */
  for (i = 0; i < class_ce->num_interfaces; i++) {
    zend_class_entry* class_iface = class_ce->interfaces[i];

    if (NULL == class_iface) {
      continue;
    }

    if (class_iface == iface_ce) {
      if (1 == class_ce->num_interfaces) {
        /*
         * Simple case: it's the only interface.
         */
        class_ce->interfaces = NULL;
        class_ce->num_interfaces = 0;

        break;
      } else if ((i + 1) == class_ce->num_interfaces) {
        /*
         * Almost as simple case: it's the last interface.
         */
        class_ce->interfaces[i] = NULL;
        class_ce->num_interfaces--;
      } else {
        /*
         * Complicated case: it's in the middle of the interfaces array. We'll
         * move the last interface here, since ordering (shouldn't) matter.
         */
        class_ce->interfaces[i]
            = class_ce->interfaces[--class_ce->num_interfaces];
      }
    }
  }

  /*
   * We have to discard the const qualifier on the interface class entry to
   * pass it through to nr_php_zend_hash_ptr_apply, even though the apply
   * callback itself takes a const zend_class_entry *. Since this generates a
   * warning in gcc, we'll squash it.
   */
#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

  nr_php_zend_hash_ptr_apply(&class_ce->function_table,
                             (nr_php_ptr_apply_t)nr_php_filter_class_methods,
                             (void*)iface_ce TSRMLS_CC);

#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic pop
#endif
}

nr_status_t nr_php_swap_user_functions(zend_function* a, zend_function* b) {
  zend_op_array temp;

  if ((NULL == a) || (ZEND_USER_FUNCTION != a->type)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: function a is invalid", __func__);
    return NR_FAILURE;
  }

  if ((NULL == b) || (ZEND_USER_FUNCTION != b->type)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: function b is invalid", __func__);
    return NR_FAILURE;
  }

  nr_memcpy(&temp, (zend_op_array*)a, sizeof(zend_op_array));
  nr_memcpy((zend_op_array*)a, (zend_op_array*)b, sizeof(zend_op_array));
  nr_memcpy((zend_op_array*)b, &temp, sizeof(zend_op_array));

  /*
   * It's unclear whether we should really swap the original scope and name
   * back, but it seems to work fine without doing so, so we'll leave them be
   * for now and hope for the best.
   */

  return NR_SUCCESS;
}

char* nr_php_class_name_from_full_name(const char* full_name) {
  int i;

  if (NULL == full_name) {
    return NULL;
  }

  for (i = 0; full_name[i]; i++) {
    if (':' == full_name[i]) {
      char* class_name = nr_strdup(full_name);

      class_name[i] = '\0';
      return class_name;
    }
  }
  return NULL;
}

char* nr_php_function_name_from_full_name(const char* full_name) {
  int i;

  if (NULL == full_name) {
    return NULL;
  }

  for (i = 0; full_name[i]; i++) {
    if ((':' == full_name[i]) && (':' == full_name[i + 1])) {
      return nr_strdup(full_name + i + 2);
    }
  }

  return nr_strdup(full_name);
}

char* nr_php_function_debug_name(const zend_function* func) {
  char* name = NULL;
  const zend_class_entry* scope;

  if (NULL == func) {
    return NULL;
  }

  scope = func->common.scope;
  name = nr_formatf("%s%s%s",
                    (scope ? NRSAFESTR(nr_php_class_entry_name(scope)) : ""),
                    (scope ? "::" : ""), NRSAFESTR(nr_php_function_name(func)));

  if ((ZEND_USER_FUNCTION == func->type)
      && (func->common.fn_flags & ZEND_ACC_CLOSURE)) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
    const char* filename = ZSTR_VAL(func->op_array.filename);
#else
    const char* filename = func->op_array.filename;
#endif /* PHP7+ */
    char* orig_name = name;

    name = nr_formatf("%s declared at %s:%d", orig_name, NRSAFESTR(filename),
                      func->op_array.line_start);
    nr_free(orig_name);
  }

  return name;
}

const char* nr_php_function_filename(zend_function* func) {
  /*
   * zend_function is a union and therefore may point to a zend_op_array or a
   * zend_internal_function. Checking the type weeds out ZEND_INTERNAL_FUNCTION,
   * which does not have an op_array.
   */
  if (func->type != ZEND_USER_FUNCTION) {
    return NULL;
  }

  if (NULL == func->op_array.filename) {
    return NULL;
  }

  return ZEND_STRING_VALUE(func->op_array.filename);
}

zval* nr_php_json_decode(zval* json TSRMLS_DC) {
  zval* decoded = NULL;

  if (NULL == json) {
    return NULL;
  }

  decoded = nr_php_call(NULL, "json_decode", json);
  if (NULL == decoded) {
    return NULL;
  }

  return decoded;
}

zval* nr_php_json_encode(zval* zv TSRMLS_DC) {
  zval* json = NULL;

  if (NULL == zv) {
    return NULL;
  }

  json = nr_php_call(NULL, "json_encode", zv);
  if (NULL == json) {
    return NULL;
  }

  if (!nr_php_is_zval_non_empty_string(json)) {
    nr_php_zval_free(&json);
    return NULL;
  }

  return json;
}

zval* nr_php_parse_str(const char* str, size_t len TSRMLS_DC) {
  zval* arr;
  char* mutable;

  if ((NULL == str) || (len > INT_MAX)) {
    return NULL;
  }

  arr = nr_php_zval_alloc();

  /*
   * sapi_module.treat_data() requires that the input string be allocated using
   * estrndup(), and that it be mutable, as it will be destroyed as part of the
   * parsing process.
   */
  mutable = estrndup(str, (nr_string_len_t)NRSAFELEN(len));

  array_init(arr);
  sapi_module.treat_data(PARSE_STRING, mutable, arr TSRMLS_CC);

  /*
   * We don't efree() mutable as sapi_module.treat_data() has already done that
   * for us.
   */
  return arr;
}

bool nr_php_function_is_static_method(const zend_function* func) {
  if (NULL == func) {
    return false;
  }

  return (func->common.fn_flags & ZEND_ACC_STATIC);
}
