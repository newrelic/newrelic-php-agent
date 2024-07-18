/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains internal error data structures and functions.
 */
#ifndef NR_ERRORS_PRIVATE_HDR
#define NR_ERRORS_PRIVATE_HDR

#include "util_time.h"

/*
 * This is the agent's view of an error.
 *
 * It contains the error attributes, but not transaction information
 * (such as request parameters) that will be added when the error is added
 * to the harvest structure.  The char * fields are owned by the error, and
 * will be allocated and assigned when the error is created, and freed when
 * the error is destroyed.
 */
struct _nr_error_t {
  nrtime_t when;         /* When did this error occur */
  int priority;          /* Error priority - lowest to highest */
  char* message;         /* Error message */
  char* klass;           /* Error class */
  char* error_file;      /* Error file */
  int error_line;        /* Error line */
  char* error_context;   /* Error context */
  int error_no;          /* Error number */
  int option;            /* Error option */
  char* stacktrace_json; /* Stack trace in JSON format */
  char* span_id; /* ID of the current executing span at the time the error
                    occurred */
};

#endif /* NR_ERRORS_PRIVATE_HDR */
