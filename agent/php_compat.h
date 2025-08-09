/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_COMPAT_HDR
#define PHP_COMPAT_HDR

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
#define PHP8
#endif /* PHP 8.0+ */

typedef uint32_t zend_uint;
typedef size_t nr_string_len_t;
typedef size_t nr_output_buffer_string_len_t;
typedef zend_string nr_php_string_hash_key_t;

#define NR_STRING_LEN_FMT "%zu"
#define ZEND_STRING_LEN(s) (s)->len
#define ZEND_STRING_VALUE(s) (s)->val

#define NR_ULONG_FMT ZEND_ULONG_FMT

static inline zval* nr_php_zval_dereference(zval* zv) {
  if ((NULL == zv) || (IS_REFERENCE != Z_TYPE_P(zv))) {
    return zv;
  }

  return Z_REFVAL_P(zv);
}

/*
 * Purpose : Given an IS_INDIRECT zval, return the actual zval it points to.
 */
static inline zval* nr_php_zval_direct(zval* zv) {
  while ((NULL != zv) && (IS_INDIRECT == Z_TYPE_P(zv))) {
    zv = Z_INDIRECT_P(zv);
  }

  return zv;
}

#endif /* PHP_COMPAT_HDR */
