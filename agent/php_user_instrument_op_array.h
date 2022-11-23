/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#if LOOKUP_METHOD == LOOKUP_USE_OP_ARRAY
/*
 * The functions nr_php_op_array_set_wraprec and
 * nr_php_op_array_get_wraprec set and retrieve pointers to function wrappers
 * (wraprecs) stored in the oparray of zend functions.
 *
 * There's the danger that other PHP modules or even other PHP processes
 * overwrite those pointers. We try to detect that by validating the
 * stored pointers.
 *
 * Since PHP 7.3, OpCache stores functions and oparrays in shared
 * memory. Consequently, the wraprec pointers we store in the oparray
 * might be overwritten by other processes. Dereferencing an overwritten
 * wraprec pointer will most likely cause a crash.
 *
 * The remedy, applied for all PHP versions:
 *
 *  1. All wraprec pointers are stored in a global vector.
 *
 *  2. The index of the wraprec pointer in the vector is mangled with
 *     the current process id. This results in a value with the lower 16 bits
 *     holding the vector index (i) and the higher bits holding the process
 *     id (p):
 *
 *       0xppppiiii (32 bit)
 *       0xppppppppppppiiii (64 bit)
 *
 *     This supports a maximum of 65536 instrumented functions.
 *
 *  3. This mangled value is stored in the oparray.
 *
 *  4. When a zend function is called and the agent tries to obtain the
 *     wraprec, the upper bits of the value are compared to the current process
 *     id. If they match, the index in the lower 16 bits is considered safe and
 *     is used. Otherwise the function is considered as uninstrumented.
 */

/*
 * Purpose : Set the wraprec associated with a user function op_array.
 *
 * Params  : 1. The zend function's oparray.
 *	     2. The function wrapper.
 */
static inline void nr_php_op_array_set_wraprec(zend_op_array* op_array,
                                 nruserfn_t* func TSRMLS_DC) {
  uintptr_t index;

  if (NULL == op_array || NULL == func) {
    return;
  }

  if (!nr_vector_push_back(NRPRG(user_function_wrappers), func)) {
    return;
  }

  index = nr_vector_size(NRPRG(user_function_wrappers)) - 1;

  index |= (NRPRG(pid) << 16);

  op_array->reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)index;
}

/*
 * Purpose : Get the wraprec associated with a user function op_array.
 *
 * Params  : 1. The zend function's oparray.
 *
 * Returns : The function wrapper. NULL if no function wrapper was registered
 *           or if the registered function wrapper is invalid.
 */
static inline nruserfn_t* nr_php_op_array_get_wraprec(
    const zend_op_array* op_array TSRMLS_DC) {
  uintptr_t index;
  uint64_t pid;

  if (nrunlikely(NULL == op_array)) {
    return NULL;
  }

  index = (uintptr_t)op_array->reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];

  if (0 == index) {
    return NULL;
  }

  pid = index >> 16;
  index &= 0xffff;

  if (pid != NRPRG(pid)) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Skipping instrumented function: pid mismatch, got " NR_INT64_FMT
        ", expected " NR_INT64_FMT,
        pid, NRPRG(pid));
    return NULL;
  }

  return (nruserfn_t*)nr_vector_get(NRPRG(user_function_wrappers), index);
}
#endif