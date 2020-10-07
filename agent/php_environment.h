/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function that describes the environment.
 */
#ifndef PHP_ENVIRONMENT_HDR
#define PHP_ENVIRONMENT_HDR

/*
 * Purpose : Produce the object that describes the invariant parts of the
 *           execution environment.
 *
 */
extern nrobj_t* nr_php_get_environment(TSRMLS_D);

/*
 * Purpose : Scan the given string looking for textual representations of
 *           key/value assignments.
 *
 *           The scanner looks for lines holding "hash rocket" style
 *           assignments:
 *
 *             key => value
 *
 *           The expected format delimits lines by newline characters, and
 *           expects single space characters before and after the literal '=>'.
 *           Any other spaces (before or after the key and/or value) will be
 *           included in the key or value as appropriate.
 *
 *           This format is generally seen with plain text phpinfo() output.
 *
 * Params  : 1. The string to scan.
 *           2. The length of the string to scan.
 *           3. The object that will have the key/value pairs added to it.
 *
 * Warning : The input string will be modified in place: key and value strings
 *           will have their trailing space or newline replaced with null
 *           bytes.
 */
void nr_php_parse_rocket_assignment_list(char* s, size_t len, nrobj_t* kv_hash);

#endif /* PHP_ENVIRONMENT_HDR */
