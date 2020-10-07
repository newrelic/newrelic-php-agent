/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with data serialised with PHP's
 * serialize() function.
 */
#ifndef UTIL_SERIALIZE_HDR
#define UTIL_SERIALIZE_HDR

/*
 * Purpose : Extract the class name of a PHP object serialised with PHP's
 *           serialize() function.
 *
 * Params  : 1. The serialised data.
 *           2. The length of the data.
 *
 * Returns : The class name, or NULL if the serialised data doesn't represent
 *           an object. The caller is responsible for freeing the class name.
 */
extern char* nr_serialize_get_class_name(const char* data, int data_len);

#endif
