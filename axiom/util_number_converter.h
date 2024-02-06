/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file containts functions to convert doubles to and from ASCII, managing
 * issues with locale.
 */

#ifndef UTIL_NUMBER_CONVERTER_H
#define UTIL_NUMBER_CONVERTER_H

#include <stddef.h>

/*
 * Purpose : Convert an integer to a base-10 string.
 *
 * Params  : 1. The destination buffer.
 *           2. The length of the buffer, in bytes.
 *           3. The value to format.
 *
 * Notes   : The caller is responsible for ensuring that the buffer has
 *           sufficient space. Otherwise, truncation may occur.
 */
extern void nr_itoa(char* buf, size_t len, int x);

/*
 * Purpose      Format double priority numbers to string.
 *
 * @param       value   double
 * @return      char*   priority string buffer
 */
extern char* nr_priority_double_to_str(double value);

/*
 * Purpose : Format double precision numbers.
 *
 * Params  : 1. The buffer to write into
 *           2. The length of the buffer
 *           3. The double to format.
 *
 * Returns : 1. Returns the number of bytes (not including the nul-terminator)
 *              written to the buffer.  Returns -1 on error.  Note that this
 *              behavior does not match snprintf.
 */
extern int nr_double_to_str(char* buf, int buf_len, double input);

/*
 * Purpose : Scan double precision numbers, following strtod, but only accepting
 * '.' as a decimal point.
 *
 * Params  : 1. The buffer to scan.
 *           2. A reference to a pointer that will be set pointing to the first
 * nonscanned character.
 *
 * It is undefined what is returned if the contents of buf denotes a
 * NaN or Infinity. The input must use '.' as the decimal point. The C
 * library strtod is used to do the conversion to achieve conversion
 * accuracy, with appropriate manipulation of the input to accomodate
 * the locale sensitivity of strtod.
 *
 */
extern double nr_strtod(const char* buf, char** endptr);

#endif /* UTIL_NUMBER_CONVERTER_H */
