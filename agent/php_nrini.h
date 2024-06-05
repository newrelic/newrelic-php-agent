/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file declares nrini functions.
 */
#ifndef PHP_NRINI_HDR
#define PHP_NRINI_HDR
/*
 * Purpose : Convert INI name to matching environment variable name.
 *
 * Params  : 1. INI name as a string
 *
 * Returns : String containing environment name - caller owns allocation
 */
extern char* nr_ini_to_env(const char* ini_name);

/*
 * Purpose : Returns a PHP array for all INI values,
 * which is keyed by INI name with a value of the equivalent
 * env var name.
 */
extern zval* nr_php_get_all_ini_envvar_names(void);

#endif /* PHP_NRINI_HDR */
