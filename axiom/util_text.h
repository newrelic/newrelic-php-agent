/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains routines for scanning text in various formats.
 */
#ifndef UTIL_TEXT_HDR
#define UTIL_TEXT_HDR

#include <stddef.h>

#include "util_object.h"

/*
 * Reads the contents of a file, and returns the contents in a null terminated
 * string.
 *
 * Params : 1. The name of the file.
 *          2. The maximum size to read.
 *
 * Returns : NULL on some fault (non existant or unreadable file), or
 *           a pointer to newly allocated memory holding the file's contents.
 */
extern char* nr_read_file_contents(const char* file_name, size_t max_bytes);

#endif /* UTIL_TEXT_HDR */
