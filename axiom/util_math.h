/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains utility maths functions.
 */
#ifndef UTIL_MATH_HDR
#define UTIL_MATH_HDR

#include <limits.h>
#include <stdint.h>

/*
 * Purpose : Calculate floor(log2(n)) for the given 32 bit integer n.
 *
 * Params  : 1. The integer to calculate the binary logarithm of.
 *
 * Returns : The logarithm, rounded down if necessary.
 *
 * Notes   : The behaviour of this function is undefined if the input is 0.
 */
inline static uint32_t nr_log2_32(uint32_t n) {
#if UINT32_MAX == UINT_MAX && (defined(__GNUC__) || defined(__clang__))
  return (8 * sizeof(uint32_t)) - __builtin_clz(n) - 1;
#elif UINT32_MAX == ULONG_MAX && (defined(__GNUC__) || defined(__clang__))
  return (8 * sizeof(uint32_t)) - __builtin_clzl(n) - 1;
#else
  uint32_t value = 0;

  while (n >>= 1) {
    value++;
  }

  return value;
#endif
}

/*
 * Purpose : Calculate floor(log2(n)) for the given 64 bit integer n.
 *
 * Params  : 1. The integer to calculate the binary logarithm of.
 *
 * Returns : The logarithm, rounded down if necessary.
 *
 * Notes   : The behaviour of this function is undefined if the input is 0.
 */
inline static uint64_t nr_log2_64(uint64_t n) {
#if UINT64_MAX == ULLONG_MAX && (defined(__GNUC__) || defined(__clang__))
  return (8 * sizeof(uint64_t)) - __builtin_clzll(n) - 1;
#else
  uint64_t value = 0;

  while (n >>= 1) {
    value++;
  }

  return value;
#endif
}

#endif /* UTIL_MATH_HDR */
