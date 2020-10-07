/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Abstractions for PHP's hash table handling functions.
 *
 * Note that we only support hash tables that contain zval *: we have little
 * need for stashing arbitrary data structures into hash tables, and the
 * massive changes to hash tables in PHP 7 make it difficult to have a unified
 * API for this.
 *
 * If you do need to stash arbitrary data structures into hash tables, you
 * should look at util_hashmap.
 */
#ifndef PHP_HASH_HDR
#define PHP_HASH_HDR

#include "php_compat.h"
#include "zend_hash.h"

/*
 * Convenience functions for zend_hash_key instances.
 */
static inline int NRPURE
nr_php_zend_hash_key_is_string(const zend_hash_key* hash_key) {
  if (NULL == hash_key) {
    return 0;
  }

#ifdef PHP7
  return (NULL != hash_key->key);
#else
  return ((NULL != hash_key->arKey) && (0 != hash_key->nKeyLength));
#endif /* PHP7 */
}

static inline int NRPURE
nr_php_zend_hash_key_is_numeric(const zend_hash_key* hash_key) {
  return !nr_php_zend_hash_key_is_string(hash_key);
}

static inline zend_ulong NRPURE
nr_php_zend_hash_key_integer(const zend_hash_key* hash_key) {
  if (NULL == hash_key) {
    return 0;
  }

  return hash_key->h;
}

static inline nr_string_len_t NRPURE
nr_php_zend_hash_key_string_len(const zend_hash_key* hash_key) {
  if (NULL == hash_key) {
    return 0;
  }

#ifdef PHP7
  return hash_key->key ? hash_key->key->len : 0;
#else
  return (nr_string_len_t)NRSAFELEN(hash_key->nKeyLength);
#endif
}

static inline const char* NRPURE
nr_php_zend_hash_key_string_value(const zend_hash_key* hash_key) {
  if (NULL == hash_key) {
    return NULL;
  }

#ifdef PHP7
  return hash_key->key ? hash_key->key->val : NULL;
#else
  return hash_key->arKey;
#endif
}

/*
 * Purpose : Add a string to an associative array (with or without a length).
 *
 *           These macros exist due to API breaks between PHP 5 and PHP 7.
 *
 *           Strings will always be duplicated, since that's non-optional in
 *           PHP 7 anyway.
 */
#ifdef PHP7
#define nr_php_add_assoc_string(ht, key, str) \
  add_assoc_string((ht), (key), (str))

#define nr_php_add_assoc_stringl(ht, key, str, strlen) \
  add_assoc_stringl((ht), (key), (str), (strlen))

#define nr_php_add_next_index_string(ht, str) add_next_index_string((ht), (str))

#define nr_php_add_next_index_stringl(ht, str, strlen) \
  add_next_index_stringl((ht), (str), (strlen))
#else
#define nr_php_add_assoc_string(ht, key, str) \
  add_assoc_string((ht), (key), (str), 1)

#define nr_php_add_assoc_stringl(ht, key, str, strlen) \
  add_assoc_stringl((ht), (key), (str), (strlen), 1)

#define nr_php_add_next_index_string(ht, str) \
  add_next_index_string((ht), (str), 1)

#define nr_php_add_next_index_stringl(ht, str, strlen) \
  add_next_index_stringl((ht), (str), (strlen), 1)
#endif /* PHP7 */

/*
 * Purpose : Wrap add_assoc_zval to ensure consistent ownership behaviour.
 *
 * Params  : 1. The array to add the value to.
 *           2. The key.
 *           3. The value.
 *
 * Note    : A copy of the value is added to the array, rather than the value
 *           itself. Ownership of the value remains with the caller.
 */
static inline int nr_php_add_assoc_zval(zval* arr,
                                        const char* key,
                                        zval* value) {
#ifdef PHP7
  zval copy;

  ZVAL_DUP(&copy, value);

  return add_assoc_zval(arr, key, &copy);
#else
  zval* copy;

  ALLOC_ZVAL(copy);
  INIT_PZVAL(copy);

  /*
   * When we drop support for PHP 5.3, we can just use ZVAL_COPY_VALUE here.
   */
  copy->value = value->value;
  Z_TYPE_P(copy) = Z_TYPE_P(value);

  zval_copy_ctor(copy);

  return add_assoc_zval(arr, key, copy);
#endif /* PHP7 */
}

/*
 * Purpose : Wrap add_index_zval to ensure consistent ownership behaviour.
 *
 * Params  : 1. The array to add the value to.
 *           2. The key.
 *           3. The value.
 *
 * Note    : A copy of the value is added to the array, rather than the value
 *           itself. Ownership of the value remains with the caller.
 */
static inline int nr_php_add_index_zval(zval* arr,
                                        zend_ulong index,
                                        zval* value) {
#ifdef PHP7
  zval copy;

  ZVAL_DUP(&copy, value);

  return add_index_zval(arr, index, &copy);
#else
  zval* copy;

  ALLOC_ZVAL(copy);
  INIT_PZVAL(copy);

  /*
   * When we drop support for PHP 5.3, we can just use ZVAL_COPY_VALUE here.
   */
  copy->value = value->value;
  Z_TYPE_P(copy) = Z_TYPE_P(value);

  zval_copy_ctor(copy);

  return add_index_zval(arr, index, copy);
#endif /* PHP7 */
}

typedef int (*nr_php_ptr_apply_t)(void* value,
                                  void* arg,
                                  zend_hash_key* hash_key TSRMLS_DC);

/*
 * Purpose : Apply a function with an argument to a HashTable containing bare
 *           pointers.
 *
 * Params  : 1. The hash table.
 *           2. The function to call for each zval.
 */
extern void nr_php_zend_hash_ptr_apply(HashTable* ht,
                                       nr_php_ptr_apply_t apply_func,
                                       void* arg TSRMLS_DC);

typedef int (*nr_php_zval_apply_t)(zval* value,
                                   void* arg,
                                   zend_hash_key* hash_key TSRMLS_DC);

/*
 * Purpose : Apply a function with an argument to a HashTable containing zvals.
 *
 * Params  : 1. The hash table.
 *           2. The function to call for each zval.
 */
extern void nr_php_zend_hash_zval_apply(HashTable* ht,
                                        nr_php_zval_apply_t apply_func,
                                        void* arg TSRMLS_DC);

/*
 * Purpose : Remove an element from a PHP HashTable.
 *
 * Params  : 1. The hash table being modified.
 *           2. The key to remove. This string must be NULL terminated.
 *
 * Returns : Non-zero if the element exists and was removed; zero otherwise.
 */
extern int nr_php_zend_hash_del(HashTable* ht, const char* key);

/*
 * Purpose : Check if an element exists within a PHP HashTable.
 *
 * Params  : 1. The hash table being searched.
 *           2. The key to search for. This string must be NULL terminated.
 *
 * Returns : Non-zero if the element exists; zero otherwise.
 */
extern int nr_php_zend_hash_exists(const HashTable* ht, const char* key);

/*
 * Purpose : Look up data within a PHP HashTable using a string index.
 *           This wrapper was originally introduced to isolate const
 *           correctness problems with early PHP versions with the first two
 *           parameters, but now mimics the PHP 7 API.
 *
 * Params  : 1. The hash table being searched.
 *           2. The key to search for. This string must be NULL terminated.
 *
 * Returns : A pointer to the returned zval, or NULL if the key doesn't exist.
 */
extern zval* nr_php_zend_hash_find(const HashTable* ht, const char* key);

/*
 * Purpose : Look up a raw pointer within a PHP HashTable.
 *
 * Params  : 1. The hash table being searched.
 *           2. The key to search for. This string must be NULL terminated.
 *
 * Returns : A pointer, or NULL if the key doesn't exist.
 */
extern void* nr_php_zend_hash_find_ptr(const HashTable* ht, const char* key);

/*
 * Purpose : Look up data within a PHP HashTable using a numeric index.
 *           This wrapper was originally introduced to isolate const
 *           correctness problems with early PHP versions with the first two
 *           parameters, but now mimics the PHP 7 API.
 *
 * Params  : 1. The hash table being searched.
 *           2. The key to be searched for.
 *
 * Returns : A pointer to the returned zval, or NULL if the key doesn't exist.
 */
extern zval* nr_php_zend_hash_index_find(const HashTable* ht, zend_ulong index);

#define nr_php_zend_hash_num_elements(ht) ((size_t)zend_hash_num_elements(ht))

#endif /* PHP_HASH_HDR */
