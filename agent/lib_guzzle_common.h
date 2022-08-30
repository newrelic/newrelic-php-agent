/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains functions common to all supported Guzzle versions.
 *
 * We support Guzzle 3 (LIB_GUZZLE3) and Guzzle 4 (LIB_GUZZLE4) within the
 * agent. Some aspects of these frameworks are the same (mostly object
 * tracking), while the implementation details differ significantly.
 */
#ifndef LIB_GUZZLE_COMMON_HDR
#define LIB_GUZZLE_COMMON_HDR

/*
 * Purpose : Creates a string containing a Guzzle async context name based on
 *           the provided message object.
 *
 * Params  : 1. The prefix to apply.
 *           2. The object to use as the basis for the name.
 *
 * Returns : A context name, which will need to be freed by the caller.
 */
extern char* nr_guzzle_create_async_context_name(const char* prefix,
                                                 const zval* obj);

/*
 * Purpose : Checks if the current PHP call stack includes a Guzzle frame.
 *
 * Returns : Non-zero if Guzzle is in the call stack; zero otherwise.
 */
extern int nr_guzzle_in_call_stack(TSRMLS_D);

/*
 * Purpose: This function checks which guzzle version is being used by the object
 *
 * Params  : 1. The object to check.
 * 
 * Returns : A string indicating the guzzle version being used
 */
extern char* nr_guzzle_version(zval* obj TSRMLS_DC);

/*
 * Purpose : Adds a Guzzle Request object to the hashmap containing all active
 *           requests, while setting the start time to the current time.
 *           Calling this method generally implies that the request has been
 *           sent and has become active.
 *
 * Params  : 1. The Request object.
 *           2. The version specific prefix for the async context name
 *
 * Returns : The external segment that will be used for the request.
 */
extern nr_segment_t* nr_guzzle_obj_add(const zval* obj,
                                       const char* async_context_prefix
                                           TSRMLS_DC);

/*
 * Purpose : Finds the request metadata struct associated with the given
 *           Request object, and removes it if present. Calling this method
 *           implies that the request is complete, and should be removed from
 *           the active list.
 *
 * Params  : 1. The Request object.
 *           2. The address of a segment that will receive the stored segment
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, the start
 *           struct will be unchanged.
 */
extern nr_status_t nr_guzzle_obj_find_and_remove(const zval* obj,
                                                 nr_segment_t** segment
                                                     TSRMLS_DC);

/*
 * Purpose : Sets the outbound headers on a request object implementing either
 *           Guzzle 3 or 4's MessageInterface.
 *
 * Params  : 1. The request object.
 *           2. The current segment.
 */
extern void nr_guzzle_request_set_outbound_headers(zval* request,
                                                   nr_segment_t* segment
                                                       TSRMLS_DC);

/*
 * Purpose : Returns a header from an object implementing the Guzzle 3 or 4
 *           MessageInterface.
 *
 * Params  : 1. The header to return.
 *           2. The response object.
 *
 * Returns : The header value, or NULL if the header wasn't in the response.
 *           This string needs to be freed when no longer needed.
 */
extern char* nr_guzzle_response_get_header(const char* header,
                                           zval* response TSRMLS_DC);

#endif /* LIB_GUZZLE_COMMON_HDR */
