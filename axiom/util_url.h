/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains utilities for URL manipulation.
 */
#ifndef UTIL_URL_HDR
#define UTIL_URL_HDR

/*
 * Purpose : Extract the domain from a string containing a URL.
 *
 * Params  : 1. The URL containing the domain. May not be null terminated.
 *           2. The length of the string. Required.
 *           3. Pointer to hold the length of the domain name or error code.
 *
 * Returns : Pointer to the start of the domain or NULL on error. Sets the
 *           int pointed to by DNLEN to be the length of the domain name.
 */
extern const char* nr_url_extract_domain(const char* url,
                                         int urllen,
                                         int* dnlen);

/*
 * Purpose : Cleanse a URL for inclusion in a transaction trace.
 *
 * Params  : 1. The full URL, which may not be null terminated.
 *           2. The URL length.
 *
 * Returns : A null-terminated cleansed version of the URL, in which the
 *           user, user:password, and/or fragment parameters have been
 *           removed. The returned string is allocated on the heap and the
 *           caller needs to free it when done with it.
 */
extern char* nr_url_clean(const char* url, int urllen);

/*
 * Purpose : Cleanse a proxy URL of its user and password.
 *
 * Params  : 1. The full URL, null terminated.
 *
 * Returns : A newly allocated, null-terminated cleansed version of the URL,
 *           in which the user, user:password have been replaced with "****"
 *           (e.g., "john:secret@foo.com:1234" becomes
 * "****:****@foo.com:1234").
 */
extern char* nr_url_proxy_clean(const char* url);

#endif /* UTIL_URL_HDR */
