/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_DATASTORE_H
#define NR_SEGMENT_DATASTORE_H

#include "nr_datastore.h"
#include "nr_datastore_instance.h"
#include "nr_slowsqls.h"
#include "nr_txn.h"

typedef char* (*nr_backtrace_fn_t)(void);
typedef void (*nr_modify_table_name_fn_t)(char* table_name);

typedef struct _nr_segment_datastore_params_t {
  /*
   * Common fields to all datastore segments.
   */
  char* collection; /* The null-terminated collection; if NULL, this will be
                       extracted from the SQL for SQL segments. */
  char* operation;  /* The null-terminated operation; if NULL, this will be
                       extracted from the SQL for SQL segments. */
  nr_datastore_instance_t*
      instance; /* Any instance information that was collected. */
  bool instance_only; /* true if only the instance metric is wanted,
                         collection and operation fields will not be
                         used or extracted from the SQL */

  /*
   * Datastore type fields.
   */
  struct {
    nr_datastore_t type; /* The datastore type that made the call. */
    char* string;        /* The datastore type as a string, if datastore is
                            NR_DATASTORE_OTHER. This field is ignored for other type
                            values. */
  } datastore;

  /*
   * These fields are only used for SQL datastore types.
   */
  struct {
    char* sql; /* The null-terminated SQL statement that was executed. */

    /*
     * The explain plan JSON for the SQL node, or NULL if no explain plan is
     * available. Note that this function does not check if explain plans are
     * enabled globally: this should be done before calling this function
     * (preferably, before even generating the explain plan!).
     */
    char* plan_json;

    /*
     * If a query language (such as DQL) was used to create the SQL, put that
     * command here.
     */
    nr_slowsqls_labelled_query_t* input_query;
  } sql;

  /*
   * Fields used to register callbacks.
   */
  struct {
    /* The function used to return a backtrace, if one is required for the
     * slowsql. This may be NULL. */
    nr_backtrace_fn_t backtrace;

    /* The function used to post-process the table (collection) name before it
     * is saved to the segment. This may be NULL.
     *
     * Note that the callback must modify the given name in place. (By
     * extension, it is impossible to modify a table name to be longer.)
     */
    nr_modify_table_name_fn_t modify_table_name;
  } callbacks;
} nr_segment_datastore_params_t;

/*
 * Purpose : Create and record metrics and a segment for a datastore call.
 *
 * Params : 1. The address of the datastore segment to be ended.
 *          2. The parameters listed above.
 *
 * Returns: true on success.
 */
extern bool nr_segment_datastore_end(nr_segment_t** segment,
                                     nr_segment_datastore_params_t* params);

/*
 * Purpose : Decide if an SQL segment of the given duration would be considered
 *           for explain plan generation.
 *
 * Params  : 1. The current transaction.
 *           2. The duration of the SQL query.
 *
 * Returns : True if the duration is above the relevant threshold and explain
 *           plans are enabled; false otherwise.
 */
extern bool nr_segment_potential_explain_plan(const nrtxn_t* txn,
                                              nrtime_t duration);

/*
 * Purpose : Decide if an SQL segment of the given duration would be considered
 *           as a potential slow SQL.
 *
 * Params  : 1. The current transaction.
 *           2. The duration of the SQL query.
 *
 * Returns : True if the duration is above the relevant threshold and slow SQLs
 *           are enabled; false otherwise.
 */
extern bool nr_segment_potential_slowsql(const nrtxn_t* txn, nrtime_t duration);

#endif /* NR_SEGMENT_DATASTORE_HDR */
