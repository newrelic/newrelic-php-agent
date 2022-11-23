/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#if LOOKUP_METHOD == LOOKUP_USE_UTIL_HASHMAP

/*
 * Purpose : Return number of digits in a number (line number)
 *
 * Params  : 1. A number
 *
 * Returns : Number of decimal digits
 *
 * This is a zf2key helper function to increase z2fkey clarity.
 */
static inline size_t number_of_digits(uint32_t lineno) {
  size_t n = 0;
  while (lineno > 0) {
    lineno /= 10;
    n++;
  }
  return n;
}

/*
 * Purpose : Convert an integer (line number) to a string
 *
 * Params  : 1. Pointer to destination
 *           2. The number to be converted (line number)
 *           3. Number of digits in a number
 *
 * Returns : Pointer to terminating NUL
 *
 * This is a zf2key helper function to increase zf2key clarity.
 * This code replaces nr_format("%d", lineno) to increase zf2key
 * performance but is placed in helper function to maintain zf2key
 * clarity. Because of that asserts about dst pointer are skipped.
 */
static inline char* nr_itoa(char* dst, uint32_t lineno, size_t ndigits) {
  char* buf = nr_alloca(ndigits);
  uint32_t r;
  for (size_t i = 0; i < ndigits; i++) {
    r = lineno % 10;
    buf[i] = '0' + r;
    lineno /= 10;
  }
  for (int i = ndigits - 1; i >= 0; i--) {
    *dst++ = buf[i];
  }
  *dst = '\0';
  return dst;
}

/*
 * Purpose : Create a key for the wraprecs hash map from zend_function
             metadata (scope, function name, filename, line number).
 *
 * Params  : 1. Pointer to storage for key length [output]
 *           2. Pointer to zend_function [input]
 *
 * Returns : A pointer to allocated memory with the generated key for the
 *           wraprecs hash map - caller must free memory.
 *
 * The key generation method is based on nr_php_function_debug_name:
 *
 * - for user function: combine scope (if any) with function name
 * - for closure: cobine filename with line number
 *
 * This guarantees uniqueness in most cases. zf2key will generate the same
 * key only closures declared on the same line in the same file, e.g:
 *
 * $g = function () { $l = function() {echo "in l\n";}; echo "in g\n"; $f();};
 *
 * $g and $l are closures (aka lambdas, aka unnamed functions) and zf2key
 * will generate the same key for both of them.
 *
 */
static inline char* zf2key(size_t* key_len, const zend_function* zf) {
  char* key = NULL;
  char* cp;
  size_t len = 0;
  size_t ndigits = 0;

  if (nrunlikely(NULL == key_len || NULL == zf
                 || ZEND_USER_FUNCTION != zf->type)) {
    return NULL;
  }

  if (zf->common.fn_flags & ZEND_ACC_CLOSURE) {
    ndigits = number_of_digits(zf->op_array.line_start);
    len = nr_php_op_array_file_name_length(&zf->op_array);
    len += 1; /* colon */
    len += ndigits;
    len += 1; /* NUL terminator */
    cp = key = nr_malloc(len);
    cp = nr_strcpy(cp, nr_php_op_array_file_name(&zf->op_array));
    cp = nr_strcpy(cp, ":");
    cp = nr_itoa(cp, zf->op_array.line_start, ndigits);
  } else {
    const zend_class_entry* scope = zf->common.scope;
    len = (scope ? nr_php_class_entry_name_length(scope) : 0);
    len += (scope ? 2 : 0); /* double colon */
    len += nr_php_function_name_length(zf);
    len += 1; /* NUL terminator */
    cp = key = nr_malloc(len);
    if (scope) {
      cp = nr_strcpy(cp, nr_php_class_entry_name(scope));
      cp = nr_strcpy(cp, "::");
    }
    cp = nr_strcpy(cp, nr_php_function_name(zf));
  }

  if (NULL != key_len) {
    *key_len = len;
  }

  return key;
}

static nr_status_t util_hashmap_set_wraprec(const zend_function* zf,
                                                      nruserfn_t* wraprec) {
  size_t key_len = 0;
  char* key = zf2key(&key_len, zf);
  nr_status_t result;
  result = nr_hashmap_set(NRPRG(user_function_wrappers), key, key_len, wraprec);
  nr_free(key);
  return result;
}

static nruserfn_t* util_hashmap_get_wraprec(zend_function* zf) {
  size_t key_len = 0;
  char* key = zf2key(&key_len, zf);
  nruserfn_t* wraprec = NULL;
  wraprec = nr_hashmap_get(NRPRG(user_function_wrappers), key, key_len);
  nr_free(key);
  return wraprec;
}

#endif