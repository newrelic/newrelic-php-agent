/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_api_internal.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_nrini.h"
#include "nr_commands_private.h"
#include "nr_datastore_instance.h"
#include "nr_header.h"
#include "nr_limits.h"
#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_system.h"

/* Test scaffolding. */
#ifdef TAGS
void zif_newrelic_get_request_metadata(void); /* ctags landing pad only */
void newrelic_get_request_metadata(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_get_request_metadata) {
  char* transport = NULL;
  const char* transport_default = "unknown";
  nr_string_len_t transport_len = 0;
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* value = NULL;
  size_t i;
  size_t header_count;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (FAILURE
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "|s", &transport,
                                  &transport_len)) {
    /*
     * This really, really shouldn't happen, since this is an internal API.
     */
    nrl_debug(NRL_API, "newrelic_get_request_metadata: cannot parse args");
    transport = NULL;
  }

  array_init(return_value);
  outbound_headers = nr_header_outbound_request_create(
      NRPRG(txn), nr_txn_get_current_segment(NRPRG(txn), NULL));

  if (NULL == outbound_headers) {
    return;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='%.*s' %s=" NRP_FMT " %s=" NRP_FMT,
        transport ? 20 : NRSAFELEN(sizeof(transport_default) - 1),
        transport ? transport : transport_default, X_NEWRELIC_ID,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                      nr_strlen(X_NEWRELIC_ID))),
        X_NEWRELIC_TRANSACTION,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                                      nr_strlen(X_NEWRELIC_TRANSACTION))));
  }

  header_keys = nr_hashmap_keys(outbound_headers);
  header_count = nr_vector_size(header_keys);

  for (i = 0; i < header_count; i++) {
    header = nr_vector_get(header_keys, i);
    value = (char*)nr_hashmap_get(outbound_headers, header, nr_strlen(header));
    nr_php_add_assoc_string(return_value, header, value);
  }

  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}

#ifdef ENABLE_TESTING_API

PHP_FUNCTION(newrelic_get_hostname) {
  char* hostname = NULL;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;
  NR_UNUSED_TSRMLS;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  hostname = nr_system_get_hostname();
  nr_php_zval_str(return_value, hostname);
  nr_free(hostname);
}

/*
 * Purpose: Extend an array with the given metrics.
 *
 * The given metrics are, via a JSON representation, converted to a PHP array.
 * This array is merged with the array given as parameter.
 *
 * The array object pointed to by the array parameter will be replaced with the
 * merged array.
 */
static void add_metrics_to_array(zval** array,
                                 const nrmtable_t* metrics TSRMLS_DC) {
  char* json = NULL;
  zval* json_zv = NULL;
  zval* new_array = NULL;
  zval* merged_array = NULL;

  if (NULL == array || NULL == *array || NULL == metrics) {
    return;
  }

  json = nr_metric_table_to_daemon_json(metrics);

  if (NULL == json) {
    php_error(E_WARNING, "%s", "Cannot convert metric table to JSON");
    goto end;
  }

  json_zv = nr_php_zval_alloc();
  nr_php_zval_str(json_zv, json);

  new_array = nr_php_call(NULL, "json_decode", json_zv);
  if (!nr_php_is_zval_valid_array(new_array)) {
    php_error(E_WARNING, "json_decode() failed on data='%s'", json);
    goto end;
  }

  merged_array = nr_php_call(NULL, "array_merge", *array, new_array);
  if (!nr_php_is_zval_valid_array(merged_array)) {
    php_error(E_WARNING, "%s", "array_merge() failed");
    nr_php_zval_free(&merged_array);
    goto end;
  }

  nr_php_zval_free(array);
  *array = merged_array;

end:
  nr_php_zval_free(&json_zv);
  nr_php_zval_free(&new_array);
  nr_free(json);
}

typedef struct _saved_txn_metric_tables_t {
  nrmtable_t* scoped_metrics;
  nrmtable_t* unscoped_metrics;
} saved_txn_metric_tables_t;

static saved_txn_metric_tables_t save_txn_metric_tables(nrtxn_t* txn) {
  saved_txn_metric_tables_t saved = {
      .scoped_metrics = txn->scoped_metrics,
      .unscoped_metrics = txn->unscoped_metrics,
  };

  txn->scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn->unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  return saved;
}

static void restore_txn_metric_tables(nrtxn_t* txn,
                                      saved_txn_metric_tables_t* saved) {
  nrm_table_destroy(&txn->scoped_metrics);
  nrm_table_destroy(&txn->unscoped_metrics);

  txn->scoped_metrics = saved->scoped_metrics;
  txn->unscoped_metrics = saved->unscoped_metrics;
}

PHP_FUNCTION(newrelic_get_metric_table) {
  char* json = NULL;
  zval* json_zv = NULL;
  const nrmtable_t* metrics;
  zend_bool scoped = 0;
  zval* table = NULL;
  nrtxnfinal_t final_data;
  saved_txn_metric_tables_t saved;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  RETVAL_FALSE;

  if (!nr_php_recording(TSRMLS_C)) {
    goto end;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &scoped)) {
    goto end;
  }

  table = nr_php_zval_alloc();
  array_init(table);

  /* transaction metrics */
  metrics = scoped ? NRTXN(scoped_metrics) : NRTXN(unscoped_metrics);
  add_metrics_to_array(&table, metrics TSRMLS_CC);

  /* segment metrics */
  saved = save_txn_metric_tables(NRPRG(txn));
  final_data = nr_segment_tree_finalise(NRPRG(txn), 0, 0, NULL, NULL);
  metrics = scoped ? NRTXN(scoped_metrics) : NRTXN(unscoped_metrics);
  add_metrics_to_array(&table, metrics TSRMLS_CC);
  restore_txn_metric_tables(NRPRG(txn), &saved);

  RETVAL_ZVAL(table, 1, 0);

end:
  nr_free(json);
  nr_php_zval_free(&json_zv);
  nr_php_zval_free(&table);
  nr_txn_final_destroy_fields(&final_data);
}

PHP_FUNCTION(newrelic_get_slowsqls) {
  const int count = nr_slowsqls_saved(NRTXN(slowsqls));
  int i;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  array_init(return_value);
  for (i = 0; i < count; i++) {
    const nr_slowsql_t* slowsql = nr_slowsqls_at(NRTXN(slowsqls), i);
    zval* ss_zv;

    if (NULL == slowsql) {
      php_error(E_WARNING, "NULL slowsql at index %d of %d", i, count);
      RETURN_FALSE;
    }

    ss_zv = nr_php_zval_alloc();
    array_init(ss_zv);

    add_assoc_long(ss_zv, "id", (zend_long)nr_slowsql_id(slowsql));
    add_assoc_long(ss_zv, "count", (zend_long)nr_slowsql_count(slowsql));
    add_assoc_long(ss_zv, "min", (zend_long)nr_slowsql_min(slowsql));
    add_assoc_long(ss_zv, "max", (zend_long)nr_slowsql_max(slowsql));
    add_assoc_long(ss_zv, "total", (zend_long)nr_slowsql_total(slowsql));
    nr_php_add_assoc_string(ss_zv, "metric",
                            nr_remove_const(nr_slowsql_metric(slowsql)));
    nr_php_add_assoc_string(ss_zv, "query",
                            nr_remove_const(nr_slowsql_query(slowsql)));
    nr_php_add_assoc_string(ss_zv, "params",
                            nr_remove_const(nr_slowsql_params(slowsql)));

    add_next_index_zval(return_value, ss_zv);
  }
}

typedef struct _find_active_segments_metadata_t {
  nr_set_t* active_segments;
  nrtime_t stop_time;
} find_active_segments_metadata_t;

static nr_segment_iter_return_t find_active_segments(
    nr_segment_t* segment,
    find_active_segments_metadata_t* metadata) {
  if (NULL == segment || NULL == metadata) {
    nrl_error(NRL_API, "%s: unexpected NULL inputs; segment=%p; metadata=%p",
              __func__, segment, metadata);
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  if (0 == segment->stop_time) {
    nr_set_insert(metadata->active_segments, segment);
    segment->stop_time = metadata->stop_time;
  }

  return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
}

static nr_segment_iter_return_t reset_active_segments(
    nr_segment_t* segment,
    nr_set_t* active_segments) {
  if (NULL == segment || NULL == active_segments) {
    nrl_error(NRL_API,
              "%s: unexpected NULL inputs; segment=%p; active_segments=%p",
              __func__, segment, active_segments);
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  if (nr_set_contains(active_segments, segment)) {
    segment->stop_time = 0;
  }

  return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
}

PHP_FUNCTION(newrelic_get_trace_json) {
  find_active_segments_metadata_t fas_metadata;
  nrtxnfinal_t final_data;
  nrtime_t orig_tt_threshold;
  saved_txn_metric_tables_t saved;
  nrtxn_t* txn = NRPRG(txn);

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  /*
   * We have to make the transaction trace threshold 0 to ensure that a trace is
   * generated.
   */
  orig_tt_threshold = txn->options.tt_threshold;
  txn->options.tt_threshold = 0;

  /*
   * We can't generate a trace if there are active segments,
   * as their stop times will be 0 and therefore before the start time, which
   * fails the sanity check in trace assembly. We'll iterate over the tree, set
   * any segment with a stop time to the current time, and track which segments
   * we changed so we can put them back at the end.
   */
  fas_metadata.active_segments = nr_set_create();
  fas_metadata.stop_time
      = nr_time_duration(nr_txn_start_time(txn), nr_get_time());
  nr_segment_iterate(txn->segment_root, (nr_segment_iter_t)find_active_segments,
                     &fas_metadata);

  /*
   * The segment count is used when assembling the trace: in some cases, it's
   * possible that it may be zero at this point (because the segment count is
   * incremented only when a segment ends, not when it starts), which would
   * result in JSON not being generated.
   *
   * Since we know how many segments we just effectively "ended" by setting
   * their stop time above, we'll adjust the transaction's segment count
   * accordingly.
   */
  txn->segment_count += nr_set_size(fas_metadata.active_segments);

  saved = save_txn_metric_tables(txn);
  final_data = nr_segment_tree_finalise(txn, NR_MAX_SEGMENTS, 0, NULL, NULL);
  restore_txn_metric_tables(txn, &saved);
  nr_php_zval_str(return_value, final_data.trace_json);
  nr_txn_final_destroy_fields(&final_data);

  /*
   * Put things back how they were.
   */
  txn->options.tt_threshold = orig_tt_threshold;
  txn->segment_count -= nr_set_size(fas_metadata.active_segments);
  nr_segment_iterate(txn->segment_root,
                     (nr_segment_iter_t)reset_active_segments,
                     fas_metadata.active_segments);
  nr_set_destroy(&fas_metadata.active_segments);
}

PHP_FUNCTION(newrelic_get_error_json) {
  nrtxn_t* txn = NRPRG(txn);
  char* json;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  json = nr_txndata_error_to_json(txn);
  if (NULL == json) {
    RETURN_FALSE;
  }

  nr_php_zval_str(return_value, json);
  nr_free(json);
}

PHP_FUNCTION(newrelic_get_transaction_guid) {
  nrtxn_t* txn = NRPRG(txn);
  const char* guid;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  guid = nr_txn_get_guid(txn);
  if (NULL == guid) {
    RETURN_FALSE;
  }

  nr_php_zval_str(return_value, guid);
}

PHP_FUNCTION(newrelic_is_localhost) {
  char* host = NULL;
  nr_string_len_t host_len = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &host,
                               &host_len)) {
    RETURN_FALSE;
  }

  if (nr_datastore_instance_is_localhost(host)) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

PHP_FUNCTION(newrelic_is_recording) {
  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  if (nr_php_recording(TSRMLS_C)) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

PHP_FUNCTION(newrelic_get_all_ini_envvar_names) {
  zval* name_array = NULL;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  name_array = nr_php_get_all_ini_envvar_names();

  RETVAL_COPY(name_array);
}

#endif /* ENABLE_TESTING_API */
