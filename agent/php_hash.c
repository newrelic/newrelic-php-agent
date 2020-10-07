/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_hash.h"

#ifdef PHP7
static int nr_php_zend_hash_ptr_apply_wrapper(zval* value,
                                              int num_args,
                                              va_list args,
                                              zend_hash_key* hash_key) {
  nr_php_ptr_apply_t apply_func;
  void* arg;

  (void)num_args;

  apply_func = (nr_php_ptr_apply_t)va_arg(args, nr_php_ptr_apply_t);
  arg = (void*)va_arg(args, void*);

  if ((NULL == value) || (IS_PTR != Z_TYPE_P(value))) {
    return ZEND_HASH_APPLY_KEEP;
  }

  return (apply_func)(Z_PTR_P(value), arg, hash_key TSRMLS_CC);
}
#else
static int nr_php_zend_hash_ptr_apply_wrapper(void* value TSRMLS_DC,
                                              int num_args,
                                              va_list args,
                                              zend_hash_key* hash_key) {
  nr_php_ptr_apply_t apply_func;
  void* arg;

  (void)num_args;

  apply_func = (nr_php_ptr_apply_t)va_arg(args, nr_php_ptr_apply_t);
  arg = (void*)va_arg(args, void*);

  if (NULL == value) {
    return ZEND_HASH_APPLY_KEEP;
  }

  return (apply_func)(value, arg, hash_key TSRMLS_CC);
}
#endif /* PHP7 */

void nr_php_zend_hash_ptr_apply(HashTable* ht,
                                nr_php_ptr_apply_t apply_func,
                                void* arg TSRMLS_DC) {
  zend_hash_apply_with_arguments(
      ht TSRMLS_CC, (apply_func_args_t)nr_php_zend_hash_ptr_apply_wrapper, 2,
      apply_func, arg);
}

#ifdef PHP7
static int nr_php_zend_hash_zval_apply_wrapper(zval* value,
                                               int num_args,
                                               va_list args,
                                               zend_hash_key* hash_key) {
  nr_php_zval_apply_t apply_func;
  void* arg;

  (void)num_args;

  apply_func = (nr_php_zval_apply_t)va_arg(args, nr_php_zval_apply_t);
  arg = (void*)va_arg(args, void*);

  return (apply_func)(value, arg, hash_key TSRMLS_CC);
}
#else
static int nr_php_zend_hash_zval_apply_wrapper(zval** value TSRMLS_DC,
                                               int num_args,
                                               va_list args,
                                               zend_hash_key* hash_key) {
  nr_php_zval_apply_t apply_func;
  void* arg;

  (void)num_args;

  apply_func = (nr_php_zval_apply_t)va_arg(args, nr_php_zval_apply_t);
  arg = (void*)va_arg(args, void*);

  if ((NULL == value) || (NULL == *value)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  (void)num_args;

  return (apply_func)(*value, arg, hash_key TSRMLS_CC);
}
#endif /* PHP7 */

void nr_php_zend_hash_zval_apply(HashTable* ht,
                                 nr_php_zval_apply_t apply_func,
                                 void* arg TSRMLS_DC) {
  zend_hash_apply_with_arguments(
      ht TSRMLS_CC, (apply_func_args_t)nr_php_zend_hash_zval_apply_wrapper, 2,
      apply_func, arg);
}

int nr_php_zend_hash_del(HashTable* ht, const char* key) {
  if ((NULL == ht) || (NULL == key)) {
    return 0;
  }

#ifdef PHP7
  int retval;
  zend_string* zs = zend_string_init(key, nr_strlen(key), 0);

  retval = zend_hash_del(ht, zs);
  zend_string_free(zs);

  return (SUCCESS == retval);
#else
  return (SUCCESS == zend_hash_del(ht, key, nr_strlen(key) + 1));
#endif /* PHP7 */
}

int nr_php_zend_hash_exists(const HashTable* ht, const char* key) {
/*
 * PHP 5 includes the null terminator in the string length, whereas PHP 7
 * does not. This is important, as it affects function and class table
 * lookups!
 */

#ifdef PHP7
  return zend_hash_str_exists(ht, key, nr_strlen(key));
#else
  return zend_hash_exists(ht, key, nr_strlen(key) + 1);
#endif /* PHP7 */
}

#ifdef PHP7
zval* nr_php_zend_hash_find(const HashTable* ht, const char* key) {
  if ((NULL == ht) || (NULL == key) || ('\0' == key[0])) {
    return NULL;
  }

  return zend_hash_str_find(ht, key, nr_strlen(key));
}

void* nr_php_zend_hash_find_ptr(const HashTable* ht, const char* key) {
  if ((NULL == ht) || (NULL == key) || ('\0' == key[0])) {
    return NULL;
  }

  return zend_hash_str_find_ptr(ht, key, nr_strlen(key));
}

zval* nr_php_zend_hash_index_find(const HashTable* ht, zend_ulong index) {
  if (NULL == ht) {
    return NULL;
  }

  return zend_hash_index_find(ht, index);
}
#else  /* Not PHP7 */
void* nr_php_zend_hash_find_ptr(const HashTable* ht, const char* key) {
  void* data = NULL;
  int keylen;
  int rv;

  if ((0 == ht) || (0 == key)) {
    return NULL;
  }

  keylen = nr_strlen(key);
  if (keylen <= 0) {
    return NULL;
  }
  keylen += 1; /* Lookup length requires null terminator */

  rv = zend_hash_find(ht, key, keylen, &data);
  if (SUCCESS != rv) {
    return NULL;
  }

  return data;
}

zval* nr_php_zend_hash_find(const HashTable* ht, const char* key) {
  zval** zv_pp = (zval**)nr_php_zend_hash_find_ptr(ht, key);

  if (NULL == zv_pp) {
    return NULL;
  }

  return *zv_pp;
}

zval* nr_php_zend_hash_index_find(const HashTable* ht, zend_ulong index) {
  void* data = NULL;
  int rv;

  rv = zend_hash_index_find(ht, index, &data);
  if ((SUCCESS != rv) || (NULL == data)) {
    return NULL;
  }

  return *((zval**)data);
}
#endif /* PHP7 */
