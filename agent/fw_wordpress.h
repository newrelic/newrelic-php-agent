/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for the Wordpress framework that will be tested
 * in unit tests.
 */
#ifndef FW_WORDPRESS_HDR
#define FW_WORDPRESS_HDR

/*
 * Purpose : ONLY for testing to verify that the appropriate matcher was created
 *           for determining if a filename belongs to the WP core (located
 *           off of the `wp-includes` directory). It destroys the matcher at
 *           the end so again, this is only for testing purposes.
 *
 * Returns : The matching core name; otherwise NULL.
 */
char* nr_php_wordpress_core_match_matcher(const char* filename);

/*
 * Purpose : ONLY for testing to verify that the appropriate matcher was created
 *           for determining if a filename belongs to a plugin. It destroys
 *           the matcher at the end so again, this is only for testing purposes.
 *
 * Returns : The matching plugin; otherwise NULL
 */
char* nr_php_wordpress_plugin_match_matcher(const char* filename);

/*
 * Purpose : ONLY for testing to verify that the appropriate matcher was created
 *           for determining if a filename belongs to a theme. It destroys
 *           the matcher at the end so again, this is only for testing purposes.
 *
 * Returns : The matching theme; otherwise NULL
 */
char* nr_php_wordpress_theme_match_matcher(const char* filename);

extern void nr_wordpress_minit(void);
extern void nr_wordpress_mshutdown(void);

#endif /* FW_WORDPRESS_HDR */
