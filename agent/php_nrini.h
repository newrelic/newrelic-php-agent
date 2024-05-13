/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file handles agent initialization and registration.
 */
#ifndef PHP_NRINI_HDR
#define PHP_NRINI_HDR

/*
 * Purpose : Iterate over all agent INI directives and check
 *           to see if any environment variable equivalents
 *           exist and use these values if available.
 */
extern void nr_php_handle_envvar_config(void);

/*
 * Purpose : Returns a PHP array for all INI values,
 * which is keyed by INI name with a value of the equivalent
 * env var name.
 */
extern zval* nr_php_get_all_ini_envvar_names(void);

//DONT COMMIT!!!
extern char* nr_ini_to_env(const char* ini_name);
// DONT COMMIT!!!

#endif /* PHP_NRINI_HDR */
