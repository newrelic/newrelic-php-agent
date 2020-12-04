/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_COMPAT_HDR
#define PHP_COMPAT_HDR

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
#define PHP7

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 7.0+ */
#define PHP8

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

#else /* PHP 5 */
typedef int nr_string_len_t;
typedef uint nr_output_buffer_string_len_t;
typedef const char nr_php_string_hash_key_t;

typedef long zend_long;

#define NR_STRING_LEN_FMT "%d"
#define ZEND_STRING_LEN(s) nr_strlen(s)
#define ZEND_STRING_VALUE(s) (s)

#define NR_ULONG_FMT "%lu"

/*
 * We need INIT_PZVAL_COPY for ZVAL_DUP, but it's not exported in PHP 5.3.
 */
#if ZEND_MODULE_API_NO <= ZEND_5_3_X_API_NO
#define INIT_PZVAL_COPY(z, v) \
  (z)->value = (v)->value;    \
  Z_TYPE_P(z) = Z_TYPE_P(v);  \
  Z_SET_REFCOUNT_P(z, 1);     \
  Z_UNSET_ISREF_P(z);
#endif

/*
 * Reimplement PHP 7's ZVAL_DUP macro, which is basically just copying the zval
 * value and calling zval_copy_ctor() to reinitialise the destination zval's
 * garbage collection and reference data.
 */
#define ZVAL_DUP(__dest, __src) \
  do {                          \
    zval* __d = (__dest);       \
    zval* __s = (__src);        \
                                \
    INIT_PZVAL_COPY(__d, __s);  \
    zval_copy_ctor(__d);        \
  } while (0);

static inline zval* nr_php_zval_dereference(zval* zv) {
  return zv;
}

static inline zval* nr_php_zval_direct(zval* zv) {
  return zv;
}

/*
 * Reimplement some of the macros that PHP 7 defines to make iteration easier.
 * For now, only the macros we actually need are implemented.
 */

#define ZEND_HASH_FOREACH(ht)                                              \
  do {                                                                     \
    HashPosition pos;                                                      \
    zval** value_ptr = NULL;                                               \
                                                                           \
    for (zend_hash_internal_pointer_reset_ex((ht), &pos);                  \
         SUCCESS                                                           \
         == zend_hash_get_current_data_ex((ht), (void**)&value_ptr, &pos); \
         zend_hash_move_forward_ex((ht), &pos)) {
#define ZEND_HASH_FOREACH_VAL(ht, _value) \
  ZEND_HASH_FOREACH(ht)                   \
  if (NULL == value_ptr) {                \
    continue;                             \
  }                                       \
  _value = *value_ptr;

#define ZEND_HASH_FOREACH_KEY_VAL(ht, _index, _key, _value)                    \
  ZEND_HASH_FOREACH(ht)                                                        \
  zend_ulong index = 0;                                                        \
  uint key_len = 0;                                                            \
  char* key_ptr = NULL;                                                        \
  int key_type;                                                                \
                                                                               \
  if (NULL == value_ptr) {                                                     \
    continue;                                                                  \
  }                                                                            \
  _value = *value_ptr;                                                         \
                                                                               \
  key_type = zend_hash_get_current_key_ex((ht), &key_ptr, &key_len, &index, 0, \
                                          &pos);                               \
  switch (key_type) {                                                          \
    case HASH_KEY_IS_LONG:                                                     \
      _key = NULL;                                                             \
      _index = index;                                                          \
      break;                                                                   \
                                                                               \
    case HASH_KEY_IS_STRING:                                                   \
      _key = key_ptr;                                                          \
      _index = 0;                                                              \
      break;                                                                   \
                                                                               \
    default:                                                                   \
      continue;                                                                \
  }

#define ZEND_HASH_FOREACH_PTR(ht, _ptr)                                    \
  do {                                                                     \
    HashPosition pos;                                                      \
    void** value_ptr = NULL;                                               \
                                                                           \
    for (zend_hash_internal_pointer_reset_ex((ht), &pos);                  \
         SUCCESS                                                           \
         == zend_hash_get_current_data_ex((ht), (void**)&value_ptr, &pos); \
         zend_hash_move_forward_ex((ht), &pos)) {
#define ZEND_HASH_FOREACH_END() \
  } /* close the for loop */    \
  }                             \
  while (0)                     \
    ;

#endif /* PHP 7.0+ */

#endif /* PHP_COMPAT_HDR */
