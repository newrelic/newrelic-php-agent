/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This contains functions to store and format errors.
 */
#ifndef NR_ERRORS_HDR
#define NR_ERRORS_HDR

#include "util_object.h"
#include "util_time.h"

/*
 * This is the agent/transaction view of an error.
 */
typedef struct _nr_error_t nr_error_t;

/*
 * Purpose : Create a new error.
 *
 * Params  : 1. The importance of the error.  Higher number is more important.
 *           2. Message string.
 *           3. Class string.  This string is used to aggregate errors in RPM.
 *              RPM does not expect strings from a specific namespace.
 *              The PHP agent uses PHP's predefined error constant names.
 *              Examples are "E_ERROR" and "E_WARNING".
 *           4. String containing stack trace in JSON format.
 *           5. When the error occurred.
 *
 * Returns : A newly allocated error, or 0 on failure.
 */
extern nr_error_t* nr_error_create(int priority,
                                   const char* message,
                                   const char* klass,
                                   const char* stacktrace_json,
                                   const char* span_id,
                                   nrtime_t when);

/*
 * Purpose : Create a new error for the use case where additional parameters are
 * passed in.
 *
 * Params  : 1. The importance of the error.  Higher number is more important.
 *           2. Message string.
 *           3. Class string.  This string is used to aggregate errors in RPM.
 *              RPM does not expect strings from a specific namespace.
 *              The PHP agent uses PHP's predefined error constant names.
 *              Examples are "E_ERROR" and "E_WARNING".
 *           4. Error file provided by user.
 *           5. Error line provided by user.
 *           6. Error context provided by user.
 *           7. Error number provided by user.
 *           8. String containing stack trace in JSON format.
 *           9. Span ID
 *           10. When the error occurred.
 *
 * Returns : A newly allocated error, or 0 on failure.
 */
extern nr_error_t* nr_error_create_additional_params(
    int priority,
    const char* message,
    const char* klass,
    const char* error_file,
    int error_line,
    const char* error_context,
    int error_no,
    const char* stacktrace_json,
    const char* span_id,
    nrtime_t when);

/*
 * Purpose : Retrieve error fields for the purpose of creating attributes.
 */

/*
 * Purpose : Get the message of an error.
 *
 * Returns : The message of the error or NULL on failure.
 */
extern const char* nr_error_get_message(const nr_error_t* error);
/*
 * Purpose : Get the klass of an error.
 *
 * Returns : The klass of the error or NULL on failure.
 */
extern const char* nr_error_get_klass(const nr_error_t* error);

/*
 * Purpose : Get the error file of an error.
 *
 * Returns : The error file of the error or NULL on failure.
 */
extern const char* nr_error_get_file(const nr_error_t* error);

/*
 * Purpose : Get the error line of an error.
 *
 * Returns : The error line of the error or 0 on failure.
 */
extern int nr_error_get_line(const nr_error_t* error);

/*
 * Purpose : Get the error context of an error.
 *
 * Returns : The error context of the error or NULL on failure.
 */
extern const char* nr_error_get_context(const nr_error_t* error);

/*
 * Purpose : Get the error number of an error.
 *
 * Returns : The error number of the error or 0 on failure.
 */
extern int nr_error_get_no(const nr_error_t* error);

/*
 * Purpose : Determine if the option is 0 or 1. If 1, then this means the error
 * has additional arguments.
 *
 * Returns : Either 0 or 1
 */
extern int nr_error_get_option(const nr_error_t* error);

/*
 * Purpose : Get the span_id of an error.
 *
 * Returns : The span_id of the error or NULL on failure.
 */
extern const char* nr_error_get_span_id(const nr_error_t* error);

/*
 * Purpose : Get the time of an error.
 *
 * Returns : The time of the error or 0 on failure.
 */
extern nrtime_t nr_error_get_time(const nr_error_t* error);

/*
 * Purpose : Get the priority of an error.
 *
 * Returns : The priority of the error or 0 on failure.
 */
extern int nr_error_priority(const nr_error_t* error);

/*
 * Purpose : Destroys an error, freeing all of its associated memory.
 */
extern void nr_error_destroy(nr_error_t** error_ptr);

/*
 * Purpose : Destroys an error with extra parameters, freeing all of its associated memory.
 */
extern void nr_error_destroy_additional_params(nr_error_t** error_ptr);

/*
 * Purpose : Turn an error into the JSON format expected by the 'error_v1'
 *           command.  Returns NULL if an error occurs.
 */
extern char* nr_error_to_daemon_json(const nr_error_t* error,
                                     const char* txn_name,
                                     const char* txn_guid,
                                     const nrobj_t* agent_attributes,
                                     const nrobj_t* user_attributes,
                                     const nrobj_t* intrinsics,
                                     const char* request_uri);

#endif /* NR_ERRORS_HDR */
