/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file includes inline functions related to handling zvals.
 *
 * These functions are generally on the main path for a lot of our
 * instrumentation code, and hence are inlined here.
 *
 * If you're using vim, you may want to turn on marker folding (:set
 * fdm=marker), as the functions are broken up into categories using marker
 * folds below.
 */
#ifndef PHP_ZVAL_HDR
#define PHP_ZVAL_HDR

#include "util_memory.h"
#include "util_strings.h"
#include "php_call.h"

/*
 * The evolution of zval ownership:
 *
 * In the beginning (PHP 5), zvals were generally handled as pointers to heap
 * allocated memory. You allocated them with MAKE_STD_ZVAL(), freed them with
 * zval_ptr_dtor() (which would only free them once their refcount hit zero),
 * and life was good.
 *
 * In PHP 7, this changed: zvals are now generally handled as stack variables.
 * zval_ptr_dtor() still exists, but will never free the zval struct, since
 * that's not its problem (it only destroys the values within the zval, for
 * zval types that are refcounted or otherwise allocated). However, we need to
 * preserve the same semantics as PHP 5 to be able to use the same general
 * instrumentation code.
 *
 * Here are the rules around what to call:
 *
 * If you want to allocate a zval: nr_php_zval_alloc().
 *
 * If you want to destroy a zval created with nr_php_zval_alloc():
 * nr_php_zval_free().
 *
 * If you want to destroy a zval returned from the Zend Engine: DON'T. We have
 * literally no cases at present where we need to do this, and any case where
 * we did have to would be version specific, since it's likely that ownership
 * rules would change between PHP 5 and PHP 7.
 */

/* {{{ Allocation and deallocation functions */

/*
 * Purpose : Allocate and initialise a zval.
 *
 * Returns : A newly allocated and initialised zval (which will be IS_UNDEF in
 *           PHP 7 and IS_NULL in PHP 5). The zval must be destroyed with
 *           nr_php_zval_free() rather than nr_php_zval_dtor().
 */
inline static zval* nr_php_zval_alloc(void) {
  zval* zv = NULL;

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  zv = (zval*)emalloc(sizeof(zval));
  ZVAL_UNDEF(zv);
#else
  MAKE_STD_ZVAL(zv);
  ZVAL_NULL(zv);
#endif

  return zv;
}

/*
 * Purpose : Destroy and free a zval and set it to NULL, after checking if it
 *           was NULL in the first place.
 *
 * Params  : 1. A zval ** to be destroyed.
 *
 * Warning : This function should only be used for zvals allocated by
 *           nr_php_zval_alloc().
 */
inline static void nr_php_zval_free(zval** zv) {
  if ((NULL != zv) && (NULL != *zv)) {
#ifdef PHP7
    zval_ptr_dtor(*zv);
    efree(*zv);
    *zv = NULL;
#else
    zval_ptr_dtor(zv);
    *zv = NULL;
#endif
  }
}

/* }}} */
/* {{{ Type checking functions */

/*
 * Purpose : Check if the argument evaluates to true.
 *
 * Params  : 1. The zval to check.
 *
 * Returns : Non-zero if the argument is true; zero otherwise.
 */
static inline int nr_php_is_zval_true(zval* z) {
  if (NULL == z) {
    return 0;
  }

  return zend_is_true(z);
}

static inline int nr_php_is_zval_valid_bool(const zval* z) {
  if (0 == z) {
    return 0;
  }

#ifdef PHP7
  if ((IS_TRUE == Z_TYPE_P(z)) || (IS_FALSE == Z_TYPE_P(z))) {
    return 1;
  }
#else
  if (IS_BOOL == Z_TYPE_P(z)) {
    return 1;
  }
#endif /* PHP7 */

  return 0;
}

static inline int nr_php_is_zval_valid_resource(const zval* z) {
  if ((0 == z) || (IS_RESOURCE != Z_TYPE_P(z))) {
    return 0;
  }

#ifdef PHP7
  if (NULL == Z_RES_P(z)) {
    return 0;
  }
#endif /* PHP7 */

  return 1;
}

/*
 * Purpose : Check if the argument is a valid PHP string.
 *
 * Returns : a 1 if the argument is a valid PHP string; 0 otherwise.
 *
 * Note    : On PHP 5, this check includes checking that the length is
 *           non-negative.
 */
static inline int nr_php_is_zval_valid_string(const zval* z) {
  if ((0 == z) || (IS_STRING != Z_TYPE_P(z))) {
    return 0;
  }

#ifdef PHP7
  if (NULL == Z_STR_P(z)) {
    return 0;
  }
#else
  if (Z_STRLEN_P(z) < 0) {
    return 0;
  }
#endif /* PHP7 */

  return 1;
}

/*
 * Purpose : Check if the argument is a valid, non-empty PHP string.
 *
 * Returns : a 1 if the argument is a valid non-empty PHP string; 0 otherwise
 */
static inline int nr_php_is_zval_non_empty_string(const zval* z) {
  if (!nr_php_is_zval_valid_string(z) || (0 == Z_STRVAL_P(z))
      || (Z_STRLEN_P(z) <= 0)) {
    return 0;
  }
  return 1;
}

static inline int nr_php_is_zval_valid_object(const zval* z) {
  if ((0 == z) || (IS_OBJECT != Z_TYPE_P(z))) {
    return 0;
  }

#ifdef PHP7
  /*
   * It's possible in PHP 7 to have a zval with type IS_OBJECT but a NULL
   * zend_object pointer.
   */
  if (NULL == Z_OBJ_P(z)) {
    return 0;
  }
#endif /* PHP7 */

  return 1;
}

static inline int nr_php_is_zval_valid_array(const zval* z) {
  if ((0 == z) || (IS_ARRAY != Z_TYPE_P(z)) || (NULL == Z_ARRVAL_P(z))) {
    return 0;
  }
  return 1;
}

static inline int nr_php_is_zval_valid_callable(zval* z TSRMLS_DC) {
  if (NULL == z) {
    return 0;
  }

  /*
   * This takes a non-const zval because the underlying API function does.
   */
  return zend_is_callable(z, 0, NULL TSRMLS_CC);
}

static inline int nr_php_is_zval_valid_integer(const zval* z) {
  if ((NULL == z) || (IS_LONG != Z_TYPE_P(z))) {
    return 0;
  }
  return 1;
}

static inline int nr_php_is_zval_valid_double(const zval* z) {
  if ((NULL == z) || (IS_DOUBLE != Z_TYPE_P(z))) {
    return 0;
  }
  return 1;
}

static inline int nr_php_is_zval_valid_scalar(const zval* z) {
  if (NULL == z) {
    return 0;
  }

  switch (Z_TYPE_P(z)) {
#ifdef PHP7
    case IS_TRUE:
    case IS_FALSE:
#else
    case IS_BOOL:
#endif
    case IS_LONG:
    case IS_DOUBLE:
      return 1;

    case IS_STRING:
      return nr_php_is_zval_valid_string(z);

    default:
      return 0;
  }
}

/*
 * Purpose : Determine if userland PHP would treat a a zval as NULL. There's
 *           some ambiguity around the difference between
 *           undefined and NULL in PHP.  See the test_is_zval_null test for
 *           more context.
 *
 *
 * Params  : 1. The zval a client programmer wants to check
 *
 * Returns : An int, 1 for true, 0 for false.
 */
static inline bool nr_php_is_zval_null(const zval* z) {
  if (NULL == z) {
    return 0;
  }

  return IS_NULL == Z_TYPE_P(z);
}

/* }}} */
/* {{{ Accessors */

/*
 * Purpose : Get the ID for the given resource.
 *
 * Params  : 1. The resource zval to retrieve the ID for.
 *
 * Returns : An ID, or 0 on error.
 *
 */
static inline long nr_php_zval_resource_id(const zval* zv) {
  if (!nr_php_is_zval_valid_resource(zv))
  {
      return 0;
  }
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  return Z_RES_P(zv)->handle;
#else
  return Z_LVAL_P(zv);
#endif /* PHP7 */
}

/*
 * Purpose : Get the ID for the given object.
 *
 * Params  : 1. The object zval to retrieve the ID for.
 *
 * Returns : An ID, or 0 on error.
 *
 */
static inline long nr_php_zval_object_id(const zval* zv) {
  if (!nr_php_is_zval_valid_object(zv))
  {
      return 0;
  }
  return Z_OBJ_HANDLE_P(zv);
}

/* }}} */
/* {{{ Mutators */

/*
 * These wrapper functions are to avoid "cast discards const qualifier" warning
 * messages created by PHP's ZVAL_STRINGL() macro when the pointer is handled
 * results.
 */
#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline void nr_php_zval_str_len(zval* zv,
                                       const char* str,
                                       nr_string_len_t len) {
#ifdef PHP7
  ZVAL_STRINGL(zv, str, len);
#else
  ZVAL_STRINGL(zv, str, len, 1);
#endif /* PHP7 */
}

static inline void nr_php_zval_str(zval* zv, const char* str) {
#ifdef PHP7
  nr_php_zval_str_len(zv, str, nr_strlen(str));
#else
  ZVAL_STRING(zv, str, 1);
#endif /* PHP7 */
}

#if defined(__clang__) || (__GNUC__ > 4) \
    || ((__GNUC__ == 4) && (__GNUC_MINOR__ > 5))
#pragma GCC diagnostic pop
#endif

static inline void nr_php_zval_bool(zval* zv, int b) {
  ZVAL_BOOL(zv, b);
}

/*
 * Purpose : Sets up a zval to be ready for use as an out argument (ie an
 *           argument that will be passed to a function by reference, where
 *           that function will then set the value).
 */
static inline void nr_php_zval_prepare_out_arg(zval* zv) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  ZVAL_NEW_REF(zv, &EG(uninitialized_zval));
#else
  ZVAL_NULL(zv);
#endif
}

/* }}} */
/* {{{ Reference handling functions */

/*
 * Unwrap any references around the actual value. This must be called before
 * performing any switch (Z_TYPE_P (zv)) { ... } statements in PHP 7.
 *
 * This is defined as a macro instead of an inline function so it remains const
 * correct in places where we're dealing with a const zval *.
 *
 * Note that you will need to use nr_php_zval_real_value() (below) if you don't
 * want to do this in place.
 */
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
#define nr_php_zval_unwrap(zv) ZVAL_DEREF(zv)

/*
 * Purpose : Walk the chain of references in the given zval and return the
 *           concrete zval that is ultimately referred to.
 *
 * Params  : 1. The zval to get the real value of.
 *
 * Returns : A pointer to the concrete zval.
 */
static inline zval* nr_php_zval_real_value(zval* zv) {
  if (NULL == zv) {
    return zv;
  }

  while (Z_ISREF_P(zv)) {
    zv = Z_REFVAL_P(zv);
  }
  return zv;
}
#else
#define nr_php_zval_unwrap(zv) (void)(zv)

static inline zval* nr_php_zval_real_value(zval* zv) {
  /*
   * As PHP 5 doesn't have a concept of typed reference zvals, this function
   * should just return the input value.
   */
  return zv;
}
#endif /* PHP7 */

/* }}} */

#endif /* PHP_ZVAL_HDR */
