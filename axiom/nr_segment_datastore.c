/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_segment_datastore.h"
#include "nr_segment_datastore_private.h"
#include "nr_txn.h"
#include "util_strings.h"
#include "util_sql.h"
#include "util_logging.h"

static bool create_instance_metric(nr_segment_t* segment,
                                   const char* product,
                                   nr_segment_datastore_t* datastore,
                                   const nr_datastore_instance_t* instance) {
  /*
   * If a datastore instance was provided, we need to add the relevant data to
   * the segment and the relavant metrics.
   */
  nrtxn_t* txn = segment->txn;
  char* instance_metric = NULL;
  if (NULL == instance || 0 == txn->options.instance_reporting_enabled) {
    return false;
  }

  if (txn->options.database_name_reporting_enabled) {
    nr_datastore_instance_set_database_name(&datastore->instance,
                                            instance->database_name);
  }

  instance_metric = nr_formatf("Datastore/instance/%s/%s/%s", product,
                               instance->host, instance->port_path_or_id);
  nr_segment_add_metric(segment, instance_metric, false);
  nr_datastore_instance_set_host(&datastore->instance, instance->host);
  nr_datastore_instance_set_port_path_or_id(&datastore->instance,
                                            instance->port_path_or_id);

  nr_free(instance_metric);
  return true;
}

static char* create_metrics(nr_segment_t* segment,
                            nrtime_t duration,
                            const char* product,
                            const char* collection,
                            const char* operation,
                            nr_segment_datastore_t* datastore,
                            const nr_datastore_instance_t* instance) {
  nrtxn_t* txn = segment->txn;
  char* operation_metric = NULL;
  char* rollup_metric = NULL;
  char* scoped_metric = NULL;
  char* statement_metric = NULL;

  nrm_force_add(txn->unscoped_metrics, "Datastore/all", duration);

  rollup_metric = nr_formatf("Datastore/%s/all", product);
  nrm_force_add(txn->unscoped_metrics, rollup_metric, duration);
  nr_free(rollup_metric);

  operation_metric
      = nr_formatf("Datastore/operation/%s/%s", product, operation);

  if (collection) {
    nr_segment_add_metric(segment, operation_metric, false);
    statement_metric = nr_formatf("Datastore/statement/%s/%s/%s", product,
                                  collection, operation);
    scoped_metric = statement_metric;
  } else {
    scoped_metric = operation_metric;
  }

  nr_segment_add_metric(segment, scoped_metric, true);

  scoped_metric = nr_strdup(scoped_metric);
  nr_free(operation_metric);
  nr_free(statement_metric);

  create_instance_metric(segment, product, datastore, instance);
  return scoped_metric;
}

bool nr_segment_datastore_end(nr_segment_t** segment_ptr,
                              nr_segment_datastore_params_t* params) {
  nrtxn_t* txn = NULL;
  bool is_sql = false;
  const char* datastore_string = NULL;
  const char* collection = NULL;
  char* collection_from_sql = NULL;
  const char* operation = NULL;
  char* scoped_metric = NULL;
  nrtime_t duration;
  const nr_slowsqls_labelled_query_t* input_query = NULL;
  nr_slowsqls_labelled_query_t input_query_allocated = {NULL, NULL};
  char* input_query_query = NULL;
  nr_segment_datastore_t datastore = {0};
  nr_segment_t* segment = NULL;
  bool rv = false;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  /*
   * Check that the params and transaction are non-NULL.
   */
  if (nrunlikely(NULL == params || NULL == segment || NULL == segment->txn)) {
    return false;
  }

  txn = segment->txn;

  /*
   * We don't want datastore segments to have any children, as
   * this would scramble the exclusive time calculation.
   *
   * Therefore, we delete all children of the segment.
   */
  if (segment) {
    for (size_t i = 0; i < nr_segment_children_size(&segment->children); i++) {
      nr_segment_t* child = nr_segment_children_get(&segment->children, i);
      nr_segment_discard(&child);
    }
  }

  if (nr_datastore_is_sql(params->datastore.type)) {
    /*
     * If the datastore type is SQL, we can try to extract
     * the collection and operation from the input SQL, if it was given.
     */
    is_sql = true;
    datastore_string = nr_datastore_as_string(params->datastore.type);

    if ((NULL == params->collection) || (NULL == params->operation)) {
      collection_from_sql = nr_segment_sql_get_operation_and_table(
          txn, &operation, params->sql.sql,
          params->callbacks.modify_table_name);
      collection = collection_from_sql;
    }
  } else {
    /*
     * Otherwise, let's ensure the datastore string is set correctly: if
     * NR_DATASTORE_OTHER is the type, then we should use the string parameter,
     * otherwise we use the string representation of the type and ignore the
     * string parameter, even if it was given, since we want to minimise the
     * risk of an MGI.
     */
    datastore_string = (NR_DATASTORE_OTHER == params->datastore.type)
                           ? params->datastore.string
                           : nr_datastore_as_string(params->datastore.type);
  }

  /*
   * At this point, there's no way to have a NULL datastore_string unless the
   * input parameters are straight up invalid, so we'll just log and get out.
   */
  if (NULL == datastore_string) {
    nrl_verbosedebug(NRL_SQL, "%s: unable to get datastore string from type %d",
                     __func__, (int)params->datastore.type);
    goto end;
  }

  /*
   * We need to add the datastore_string to the transaction, so that the correct
   * rollup metrics are created when the transaction ends.
   */
  nr_string_add(txn->datastore_products, datastore_string);

  /*
   * We'll always use the collection and operation strings IF they exist in
   * the parameter, even if we extracted them from the SQL earlier.
   */
  if (params->collection) {
    collection = params->collection;
  }
  if (params->operation) {
    operation = params->operation;
  } else if (NULL == operation) {
    /*
     * The operation is a bit special: if it's not set, then we should set it to
     * "other".
     */
    operation = "other";
  }
  if (params->sql.input_query) {
    input_query = params->sql.input_query;
  }

  /*
   * We set the end time here because we need the duration, (nr_segment_end will
   * not overwrite this values if it's already set).
   */
  if (!segment->stop_time) {
    segment->stop_time
        = nr_time_duration(nr_txn_start_time(txn), nr_get_time());
  }
  duration = nr_time_duration(segment->start_time, segment->stop_time);

  /*
   * Generate a backtrace if the query was slow enough and we have a callback
   * that allows us to do so.
   */
  if (params->callbacks.backtrace
      && nr_segment_datastore_stack_worthy(txn, duration)) {
    datastore.backtrace_json = (params->callbacks.backtrace)();
  }

  /*
   * Add the metrics that we can reasonably add at this point.
   *
   * The allWeb and allOther rollup metrics are created at the end of the
   * transaction since the background status may change.
   */
  if (!params->instance_only) {
    scoped_metric
        = create_metrics(segment, duration, datastore_string, collection,
                         operation, &datastore, params->instance);
    nr_segment_set_name(segment, scoped_metric);
  } else {
    create_instance_metric(segment, datastore_string, &datastore, params->instance);
  }

  /*
   * Add the explain plan, if we have one.
   */
  if (params->sql.plan_json) {
    datastore.explain_plan_json = params->sql.plan_json;
  }

  /*
   * If the datastore is a SQL datastore and we have a query, then we need to
   * add the query to the data hash, being mindful of the user's obfuscation and
   * security settings. This is also the point we'll handle any input query that
   * was used.
   *
   * We set these to function scoped variables because we can also use these in
   * any slowsql that we save.
   */
  if (is_sql) {
    switch (nr_txn_sql_recording_level(txn)) {
      case NR_SQL_RAW:
        datastore.sql = params->sql.sql;
        break;

      case NR_SQL_OBFUSCATED:
        datastore.sql_obfuscated = nr_sql_obfuscate(params->sql.sql);

        /*
         * If it's set, we have to replace input_query with the obfuscated
         * version of the input_query.
         */
        if (params->sql.input_query) {
          input_query_allocated.query = input_query_query
              = nr_sql_obfuscate(params->sql.input_query->query);
          input_query_allocated.name = params->sql.input_query->name;
          input_query = &input_query_allocated;
        }
        break;

      case NR_SQL_NONE: /* FALLTHROUGH */
      default:
        break;
    }
  }

  datastore.component = nr_strdup(datastore_string);

  if (input_query) {
    nrobj_t* obj = nro_new_hash();

    nro_set_hash_string(obj, "label", input_query->name);
    nro_set_hash_string(obj, "query", input_query->query);
    datastore.input_query_json = nro_to_json(obj);

    nro_delete(obj);
  }

  if (is_sql && nr_segment_potential_slowsql(txn, duration)) {
    nr_slowsqls_params_t slowsqls_params = {
        .sql
        = datastore.sql_obfuscated ? datastore.sql_obfuscated : datastore.sql,
        .duration = duration,
        .stacktrace_json = datastore.backtrace_json,
        .metric_name = scoped_metric,
        .plan_json = params->sql.plan_json,
        .input_query_json = datastore.input_query_json,
        .instance = params->instance,
        .instance_reporting_enabled = txn->options.instance_reporting_enabled,
        .database_name_reporting_enabled
        = txn->options.database_name_reporting_enabled,
    };

    nr_slowsqls_add(txn->slowsqls, &slowsqls_params);
  }

  nr_segment_set_datastore(segment, &datastore);

  rv = nr_segment_end(&segment);

end:
  nr_free(collection_from_sql);
  nr_free(input_query_query);
  nr_free(scoped_metric);
  nr_free(datastore.backtrace_json);
  nr_free(datastore.component);
  nr_free(datastore.input_query_json);
  nr_free(datastore.instance.port_path_or_id);
  nr_free(datastore.instance.host);
  nr_free(datastore.instance.database_name);
  nr_free(datastore.sql_obfuscated);

  return rv;
}

bool nr_segment_potential_explain_plan(const nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn) {
    return false;
  }

  return txn->options.ep_enabled && nr_segment_potential_slowsql(txn, duration);
}

bool nr_segment_potential_slowsql(const nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn || NR_SQL_NONE == txn->options.tt_recordsql
      || 0 == txn->options.tt_slowsql) {
    return false;
  }

  return duration >= txn->options.ep_threshold;
}

char* nr_segment_sql_get_operation_and_table(
    const nrtxn_t* txn,
    const char** operation_ptr,
    const char* sql,
    nr_modify_table_name_fn_t modify_table_name_fn) {
  char* table = NULL;

  if (operation_ptr) {
    *operation_ptr = NULL;
  }
  if (NULL == txn) {
    return NULL;
  }
  if (txn->special_flags.no_sql_parsing) {
    return NULL;
  }

  nr_sql_get_operation_and_table(sql, operation_ptr, &table,
                                 txn->special_flags.show_sql_parsing);
  if (NULL == table) {
    return NULL;
  }

  if (modify_table_name_fn) {
    modify_table_name_fn(table);
  }

  return table;
}

bool nr_segment_datastore_stack_worthy(const nrtxn_t* txn, nrtime_t duration) {
  if (NULL == txn) {
    return false;
  }

  if ((txn->options.ss_threshold > 0)
      && (duration >= txn->options.ss_threshold)) {
    return true;
  }

  if (0 != txn->options.tt_slowsql) {
    if (duration >= txn->options.ep_threshold) {
      return true;
    }
  }

  return false;
}
