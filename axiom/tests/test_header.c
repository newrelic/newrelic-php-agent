/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "nr_header.h"
#include "nr_header_private.h"
#include "nr_distributed_trace_private.h"
#include "util_base64.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_metrics_private.h"
#include "util_obfuscate.h"
#include "util_string_pool.h"
#include "util_strings.h"

#include "tlib_main.h"

/*
 * Encoding key used for functions that deal with obfuscated headers.
 * This key is local to these tests and a generic example of something
 * that would come from encoding_key.
 */
#define ENCODING_KEY "d67afc830dab717fd163bfcb0b8b88423e9a1a3b"

typedef struct _mock_txn {
  nrtxn_t txn;
  nr_status_t freeze_name_return;
  const char* fake_guid;
  nrtime_t fake_queue_time;
  int fake_trusted;
  nrtime_t unfinished_duration;
} mock_txn;

/*
 * Mock nr_txn.c functions.
 */

nr_status_t nr_txn_freeze_name_update_apdex(nrtxn_t* txn) {
  return ((const mock_txn*)txn)->freeze_name_return;
}

const char* nr_txn_get_cat_trip_id(const nrtxn_t* txn) {
  return txn->cat.trip_id ? txn->cat.trip_id
                          : ((const mock_txn*)txn)->fake_guid;
}

const char* nr_txn_get_guid(const nrtxn_t* txn) {
  return ((const mock_txn*)txn)->fake_guid;
}

char* nr_txn_get_current_trace_id(nrtxn_t* txn NRUNUSED) {
  return nr_strdup("abcdef01");
}

char* nr_txn_get_path_hash(nrtxn_t* txn NRUNUSED) {
  return nr_strdup("12345678");
}

nrtime_t nr_txn_queue_time(const nrtxn_t* txn) {
  return ((const mock_txn*)txn)->fake_queue_time;
}

int nr_txn_is_account_trusted(const nrtxn_t* txn, int account_id NRUNUSED) {
  return ((const mock_txn*)txn)->fake_trusted;
}

bool nr_txn_should_create_span_events(const nrtxn_t* txn NRUNUSED) {
  return true;
}

nrtime_t nr_txn_unfinished_duration(const nrtxn_t* txn) {
  return ((const mock_txn*)txn)->unfinished_duration;
}

char* nr_txn_create_distributed_trace_payload(nrtxn_t* txn,
                                              nr_segment_t* segment NRUNUSED) {
  return txn->options.distributed_tracing_exclude_newrelic_header
             ? NULL
             : nr_strdup("{ \"v\" : [0,1], \"d\" : {} }");
}

char* nr_txn_create_w3c_traceparent_header(nrtxn_t* txn NRUNUSED,
                                           nr_segment_t* segment NRUNUSED) {
  return nr_strdup("00-74be672b84ddc4e4b28be285632bbc0a-d6e4e06002e24189-01");
}

char* nr_txn_create_w3c_tracestate_header(const nrtxn_t* txn NRUNUSED,
                                          nr_segment_t* segment NRUNUSED) {
  return nr_strdup(
      "190@nr=0-0-212311-51424-d6e4e06002e24189-27856f70d3d314b7-1-0.421-"
      "1482959525577");
}

/*
 * For the mocking above to work, we need to ensure that there are no undefined
 * symbols that will cause nr_txn.o to be included from libaxiom.a, otherwise
 * we'll get a bunch of errors around multiply defined symbols. This includes
 * indirect dependencies: notably, nr_segment.o requires a few functions around
 * segment stack maintenance, so we need to mock them enough to compile.
 */
nr_segment_t* nr_txn_get_current_segment(nrtxn_t* txn NRUNUSED,
                                         const char* async_context NRUNUSED) {
  return NULL;
}

void nr_txn_set_current_segment(nrtxn_t* txn NRUNUSED,
                                nr_segment_t* segment NRUNUSED) {}

void nr_txn_retire_current_segment(nrtxn_t* txn NRUNUSED,
                                   nr_segment_t* segment NRUNUSED) {}

nrtime_t nr_txn_start_time(const nrtxn_t* txn NRUNUSED) {
  return 0;
}

#define test_metric_created(...) \
  test_header_test_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_header_test_metric_created_fn(const char* testname,
                                               nrmtable_t* metrics,
                                               nrtime_t val,
                                               uint32_t flags,
                                               const char* name,
                                               const char* file,
                                               int line) {
  nrmetric_t* m = nrm_find(metrics, name);
  const char* nm = nrm_get_name(metrics, m);

  test_pass_if_true(testname, 0 != m, "m=%p", m);
  test_pass_if_true(testname, 0 == nr_strcmp(nm, name), "nm=%s name=%s",
                    nm ? nm : "(NULL)", name ? name : "(NULL)");

  if (m) {
    test_pass_if_true(testname, flags == m->flags,
                      "name=%s flags=%u m->flags=%u", name, flags, m->flags);
    test_pass_if_true(testname, nrm_count(m) == 1,
                      "name=%s nrm_count (m)=" NR_TIME_FMT, name, nrm_count(m));
    test_pass_if_true(testname, nrm_total(m) == val,
                      "name=%s nrm_total (m)=" NR_TIME_FMT " val=" NR_TIME_FMT,
                      name, nrm_total(m), val);
    test_pass_if_true(testname, nrm_exclusive(m) == val,
                      "name=%s nrm_exclusive (m)=" NR_TIME_FMT
                      " val=" NR_TIME_FMT,
                      name, nrm_exclusive(m), val);
    test_pass_if_true(testname, nrm_min(m) == val,
                      "name=%s nrm_min (m)=" NR_TIME_FMT " val=" NR_TIME_FMT,
                      name, nrm_min(m), val);
    test_pass_if_true(testname, nrm_max(m) == val,
                      "name=%s nrm_max (m)=" NR_TIME_FMT " val=" NR_TIME_FMT,
                      name, nrm_max(m), val);
    test_pass_if_true(testname, nrm_sumsquares(m) == (val * val),
                      "name=%s nrm_sumsquares (m)=" NR_TIME_FMT
                      " val=" NR_TIME_FMT,
                      name, nrm_sumsquares(m), val);
  }
}

#define test_metrics_empty(T, M) \
  test_metrics_empty_fn((T), (M), __FILE__, __LINE__)

static void test_metrics_empty_fn(const char* testname,
                                  const nrmtable_t* table,
                                  const char* file,
                                  int line) {
  int table_size = nrm_table_size(table);

  test_pass_if_true(testname, 0 == table_size, "table_size=%d", table_size);
}

static void test_encode_decode(void) {
  nrtxn_t txn;
  const char* hello_encoded = "DFNbDQk=";
  nrobj_t* app_connect_reply = nro_new_hash();
  char* output;

  nro_set_hash_string(app_connect_reply, "encoding_key",
                      "d67afc830dab717fd163bfcb0b8b88423e9a1a3b");

  txn.app_connect_reply = app_connect_reply;
  txn.special_flags.debug_cat = 0;

  /*
   * Test : Bad Parameters
   */
  output = nr_header_encode(0, "hello");
  tlib_pass_if_str_equal("null txn", output, 0);
  output = nr_header_decode(0, hello_encoded);
  tlib_pass_if_str_equal("null txn", output, 0);

  output = nr_header_encode(&txn, 0);
  tlib_pass_if_str_equal("null string", output, 0);
  output = nr_header_decode(&txn, 0);
  tlib_pass_if_str_equal("null string", output, 0);

  txn.app_connect_reply = 0;
  output = nr_header_encode(&txn, "hello");
  tlib_pass_if_str_equal("no encoding key", output, 0);
  output = nr_header_decode(&txn, hello_encoded);
  tlib_pass_if_str_equal("no encoding key", output, 0);
  txn.app_connect_reply = app_connect_reply;

  output = nr_header_decode(&txn, "??????");
  tlib_pass_if_str_equal("bad encoded string", output, 0);

  /*
   * Test : Success
   */
  output = nr_header_encode(&txn, "hello");
  tlib_pass_if_str_equal("encode success", hello_encoded, output);
  nr_free(output);

  output = nr_header_decode(&txn, hello_encoded);
  tlib_pass_if_str_equal("decode success", "hello", output);
  nr_free(output);

  nro_delete(app_connect_reply);
}

static void test_validate_decoded_id(void) {
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;
  nrobj_t* app_connect_reply;
  nr_status_t rv;

  app_connect_reply = nro_create_from_json("{\"trusted_account_ids\":[12345]}");
  txn->app_connect_reply = app_connect_reply;
  txn->special_flags.debug_cat = 0;

  /*
   * Detect issues by always treating the account as being trusted, since the
   * below tests should never get as far as calling nr_txn_is_account_trusted.
   */
  txnv.fake_trusted = 1;

  rv = nr_header_validate_decoded_id(0, 0);
  tlib_pass_if_status_failure("zero params", rv);

  rv = nr_header_validate_decoded_id(0, "12345#6789");
  tlib_pass_if_status_failure("null txn", rv);

  rv = nr_header_validate_decoded_id(txn, 0);
  tlib_pass_if_status_failure("null id", rv);

  txn->app_connect_reply = 0;
  txnv.fake_trusted = 0;
  rv = nr_header_validate_decoded_id(txn, "12345#6789");
  tlib_pass_if_status_failure("no trusted_account_ids", rv);
  txnv.fake_trusted = 1;
  txn->app_connect_reply = app_connect_reply;

  rv = nr_header_validate_decoded_id(txn, "");
  tlib_pass_if_status_failure("empty decoded_id", rv);

  rv = nr_header_validate_decoded_id(txn, "     ");
  tlib_pass_if_status_failure("account_id missing", rv);

  rv = nr_header_validate_decoded_id(txn, "10000000000000000000000#1");
  tlib_pass_if_status_failure("account_id too big", rv);

  rv = nr_header_validate_decoded_id(
      txn,
      "100000000000000000000000000000000000000000000#"
      "100000000000000000000000000000000000000000000");
  tlib_pass_if_status_failure("decoded_id too big", rv);

  rv = nr_header_validate_decoded_id(txn, "12345");
  tlib_pass_if_status_failure("account_id does not end in #", rv);

  rv = nr_header_validate_decoded_id(
      txn, "0x3039#6789"); /* 0x3039 is 12345 in hex */
  tlib_pass_if_status_failure("account_id is not in base 10", rv);

  txnv.fake_trusted = 0;
  rv = nr_header_validate_decoded_id(txn, "6789#12345");
  tlib_pass_if_status_failure("account_id is not in trusted_account_ids", rv);
  txnv.fake_trusted = 1;

  rv = nr_header_validate_decoded_id(txn, "12345#6789");
  tlib_pass_if_status_success("success!", rv);

  nro_delete(app_connect_reply);
}

#define failed_inbound_response_testcase(...) \
  failed_inbound_response_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void failed_inbound_response_testcase_fn(const char* testname,
                                                const nrtxn_t* txn,
                                                const char* response,
                                                const char* file,
                                                int line) {
  test_pass_if_true(testname, 0 != txn, "txn=%p", txn);
  test_pass_if_true(testname, 0 == response, "response=%p", response);

  if (txn) {
    test_pass_if_true(
        testname, NR_STATUS_CROSS_PROCESS_START == txn->status.cross_process,
        "txn->status.cross_process=%d", (int)txn->status.cross_process);
    test_pass_if_true(testname, 0 == txn->status.has_inbound_record_tt,
                      "txn->status.has_inbound_record_tt=%d",
                      txn->status.has_inbound_record_tt);
    test_obj_as_json_fn(testname, txn->intrinsics, "{}", file, line);
    test_metrics_empty_fn(testname, txn->unscoped_metrics, file, line);
  }
}

static void test_inbound_response_internal(void) {
  nrobj_t* app_connect_reply;
  const char* guid = "FEDCBA9876543210";
  char* response;

  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;

  txnv.fake_trusted = 1;
  txnv.freeze_name_return = NR_SUCCESS;
  txn->status.recording = 1;
  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  txn->options.cross_process_enabled = 1;
  txnv.unfinished_duration = 123 * NR_TIME_DIVISOR;
  txnv.fake_queue_time = 1 * NR_TIME_DIVISOR;
  txn->status.has_inbound_record_tt = 0;
  txn->cat.client_cross_process_id = nr_strdup("12345#6789");
  txn->special_flags.debug_cat = 0;

  txn->unscoped_metrics = nrm_table_create(10);
  txn->intrinsics = nro_new_hash();

  txn->name = nr_strdup("txnname");
  txnv.fake_guid = guid;

  app_connect_reply = nro_create_from_json(
      "{\"cross_process_id\":\"1#1\",\"encoding_key\":"
      "\"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\",\"trusted_account_ids\":["
      "12345]}");
  txn->app_connect_reply = app_connect_reply;

  /*
   * Test : Bad Parameters: Bad Transaction State
   */
  response = nr_header_inbound_response_internal(0, -1);
  tlib_pass_if_str_equal("null txn", response, 0);

  txn->options.cross_process_enabled = 0;
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("cross process not enabled", txn, response);
  txn->options.cross_process_enabled = 1;

  txn->status.recording = 0;
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("not recording", txn, response);
  txn->status.recording = 1;

  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_DISABLED;
  response = nr_header_inbound_response_internal(txn, -1);
  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  failed_inbound_response_testcase("wrong cross_process status", txn, response);

  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_RESPONSE_CREATED;
  response = nr_header_inbound_response_internal(txn, -1);
  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  failed_inbound_response_testcase("wrong cross_process status", txn, response);

  txnv.freeze_name_return = NR_FAILURE;
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("freeze name failure", txn, response);
  txnv.freeze_name_return = NR_SUCCESS;

  txn->app_connect_reply = 0;
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("missing app_connect_reply", txn, response);
  txn->app_connect_reply = app_connect_reply;

  txnv.fake_guid = NULL;
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("missing guid", txn, response);
  txnv.fake_guid = guid;

  /*
   * Test : Non-cross-process transaction.
   */
  nr_free(txn->cat.client_cross_process_id);
  response = nr_header_inbound_response_internal(txn, -1);
  failed_inbound_response_testcase("not a cross process txn", txn, response);
  txn->cat.client_cross_process_id = nr_strdup("12345#6789");

  /*
   * Test : Success
   */
  response = nr_header_inbound_response_internal(txn, -1);
  tlib_pass_if_int_equal("no decoded_x_newrelic_transaction",
                         txn->status.cross_process,
                         NR_STATUS_CROSS_PROCESS_RESPONSE_CREATED);
  tlib_pass_if_str_equal(
      "no decoded_x_newrelic_transaction", response,
      "[\"1#1\",\"txnname\",1.00000,123.00000,-1,\"FEDCBA9876543210\",false]");
  tlib_pass_if_int_equal("no decoded_x_newrelic_transaction",
                         txn->status.has_inbound_record_tt, 0);
  test_obj_as_json("no decoded_x_newrelic_transaction", txn->intrinsics,
                   "{\"client_cross_process_id\":\"12345#6789\"}");
  test_metric_created("no decoded_x_newrelic_transaction",
                      txn->unscoped_metrics, 123000000, 0,
                      "ClientApplication/12345#6789/all");
  nro_delete(txn->intrinsics);
  txn->intrinsics = nro_new_hash();
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);
  nr_free(response);
  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;

  nr_free(txn->name);
  nro_delete(app_connect_reply);
  nrm_table_destroy(&txn->unscoped_metrics);
  nro_delete(txn->intrinsics);
  nr_free(txn->cat.client_cross_process_id);
}

static void test_inbound_response(void) {
  nrobj_t* app_connect_reply;
  char* response;
  char* decoded_response;
  const char* guid = "FEDCBA9876543210";
  nrobj_t* json;
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;

  txnv.freeze_name_return = NR_SUCCESS;
  txnv.fake_guid = guid;
  txnv.fake_queue_time = 1 * NR_TIME_DIVISOR;
  txnv.fake_trusted = 1;

  txn->status.recording = 1;
  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  txn->options.cross_process_enabled = 1;
  txnv.unfinished_duration = 123 * NR_TIME_DIVISOR;
  txn->status.has_inbound_record_tt = 0;
  txn->cat.client_cross_process_id = nr_strdup("12345#6789");
  txn->special_flags.debug_cat = 0;

  txn->unscoped_metrics = nrm_table_create(10);
  txn->intrinsics = nro_new_hash();

  txn->name = nr_strdup("txnname");

  app_connect_reply = nro_create_from_json(
      "{\"cross_process_id\":\"1#1\",\"encoding_key\":"
      "\"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\",\"trusted_account_ids\":["
      "12345]}");
  txn->app_connect_reply = app_connect_reply;

  /*
   * Test : Bad Parameters
   *
   * Most bad parameter situations are tested in test_inbound_response_internal.
   */
  response = nr_header_inbound_response(0, -1);
  tlib_pass_if_str_equal("null txn", response, 0);

  /*
   * Test : Success
   *
   * We cannot test the response string directly, since it has a variable
   * apptime in it.  Therefore, most of the testing occurs in
   * test_inbound_response_internal.
   */
  response = nr_header_inbound_response(txn, -1);
  tlib_pass_if_true("success", 0 != response, "response=%p", response);
  decoded_response = nr_header_decode(txn, response);
  tlib_pass_if_true("success", 0 != decoded_response, "decoded_response=%p",
                    decoded_response);
  json = nro_create_from_json(decoded_response);
  tlib_pass_if_true("decoded response is json", 0 != json, "json=%p", json);

  nr_free(response);
  nr_free(decoded_response);
  nro_delete(json);

  nr_free(txn->name);
  nro_delete(app_connect_reply);
  nrm_table_destroy(&txn->unscoped_metrics);
  nro_delete(txn->intrinsics);
  nr_free(txn->cat.client_cross_process_id);
}

#define failed_outbound_response_testcase(...) \
  failed_outbound_response_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void failed_outbound_response_testcase_fn(const char* testname,
                                                 const nrtxn_t* txn,
                                                 const char* id,
                                                 const char* txnname,
                                                 const char* guid,
                                                 const char* file,
                                                 int line) {
  test_pass_if_true(testname, 0 != txn, "txn=%p", txn);

  test_pass_if_true(testname, 0 == id, "id=%p", id);
  test_pass_if_true(testname, 0 == txnname, "txnname=%p", txnname);
  test_pass_if_true(testname, 0 == guid, "guid=%p", guid);

  if (txn) {
    test_pass_if_true(testname, 0 == txn->status.has_outbound_record_tt,
                      "txn->status.has_outbound_record_tt=%d",
                      txn->status.has_outbound_record_tt);
  }
}

static void test_outbound_response_decoded(void) {
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;
  char* id = 0;
  char* guid = 0;
  char* txnname = 0;
  nrobj_t* app_connect_reply;

  txnv.freeze_name_return = NR_SUCCESS;
  txnv.fake_queue_time = 0;
  txnv.fake_trusted = 1;

  app_connect_reply = nro_create_from_json(
      "{\"cross_process_id\":\"1#1\",\"encoding_key\":"
      "\"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\",\"trusted_account_ids\":["
      "12345]}");

  txn->app_connect_reply = app_connect_reply;
  txn->options.cross_process_enabled = 1;
  txn->status.has_outbound_record_tt = 0;
  txn->special_flags.debug_cat = 0;

  /*
   * Test : Bad Parameters
   */
  nr_header_outbound_response_decoded(
      0, "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1]", &id, &txnname, &guid);
  failed_outbound_response_testcase("null txn", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(txn, 0, &id, &txnname, &guid);
  failed_outbound_response_testcase("null response", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(txn, "{}", &id, &txnname, &guid);
  failed_outbound_response_testcase("not array", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(txn, "[]", &id, &txnname, &guid);
  failed_outbound_response_testcase("empty array", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(txn, "[\"12345#6789\", \"txnname\"]", &id,
                                      &txnname, &guid);
  failed_outbound_response_testcase("only two elements", txn, id, txnname,
                                    guid);

  nr_header_outbound_response_decoded(
      txn, "[\"12345#6789\", 123, 1.1, 2.2, -1]", &id, &txnname, &guid);
  failed_outbound_response_testcase("bad txnname", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(txn, "[123, \"txnname\", 1.1, 2.2, -1]",
                                      &id, &txnname, &guid);
  failed_outbound_response_testcase("bad id", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(
      txn, "[\"99999#6789\", 123, 1.1, 2.2, -1]", &id, &txnname, &guid);
  failed_outbound_response_testcase("id not in trusted list", txn, id, txnname,
                                    guid);

  txn->options.cross_process_enabled = 0;
  nr_header_outbound_response_decoded(
      txn, "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1]", &id, &txnname, &guid);
  failed_outbound_response_testcase("cross process disabled", txn, id, txnname,
                                    guid);
  txn->options.cross_process_enabled = 1;

  nr_header_outbound_response_decoded(
      txn, "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1, 123, false]", &id,
      &txnname, &guid);
  failed_outbound_response_testcase("bad guid", txn, id, txnname, guid);

  nr_header_outbound_response_decoded(
      txn,
      "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1, \"0123456789ABCDEF\", 123]",
      &id, &txnname, &guid);
  failed_outbound_response_testcase("bad record_tt", txn, id, txnname, guid);

  /*
   * Test : Success with 5 Element Array
   */
  nr_header_outbound_response_decoded(
      txn, "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1]", &id, &txnname, &guid);
  tlib_pass_if_str_equal("5 element success", "12345#6789", id);
  tlib_pass_if_str_equal("5 element success", "txnname", txnname);
  tlib_pass_if_str_equal("5 element success", guid, 0);
  tlib_pass_if_int_equal("5 element success",
                         txn->status.has_outbound_record_tt, 0);
  nr_free(id);
  nr_free(txnname);

  /*
   * Test : Success with 7 Element Array
   */
  nr_header_outbound_response_decoded(txn,
                                      "[\"12345#6789\", \"txnname\", 1.1, 2.2, "
                                      "-1, \"0123456789ABCDEF\", false]",
                                      &id, &txnname, &guid);
  tlib_pass_if_str_equal("7 element success", "12345#6789", id);
  tlib_pass_if_str_equal("7 element success", "txnname", txnname);
  tlib_pass_if_str_equal("7 element success", "0123456789ABCDEF", guid);
  tlib_pass_if_int_equal("7 element success",
                         txn->status.has_outbound_record_tt, 0);
  nr_free(id);
  nr_free(txnname);
  nr_free(guid);

  /*
   * Test : 7 Element Array Success with record_tt
   */
  txn->status.has_outbound_record_tt = 0;
  nr_header_outbound_response_decoded(
      txn,
      "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1, \"0123456789ABCDEF\", true]",
      &id, &txnname, &guid);
  tlib_pass_if_str_equal("true record_tt", "12345#6789", id);
  tlib_pass_if_str_equal("true record_tt", "txnname", txnname);
  tlib_pass_if_str_equal("true record_tt", "0123456789ABCDEF", guid);
  tlib_pass_if_int_equal("true record_tt", txn->status.has_outbound_record_tt,
                         1);
  nr_free(id);
  nr_free(txnname);
  nr_free(guid);

  /*
   * Test : record_tt without returning other elements
   */
  txn->status.has_outbound_record_tt = 0;
  nr_header_outbound_response_decoded(
      txn,
      "[\"12345#6789\", \"txnname\", 1.1, 2.2, -1, \"0123456789ABCDEF\", true]",
      0, 0, 0);
  tlib_pass_if_str_equal("only record_tt", id, 0);
  tlib_pass_if_str_equal("only record_tt", txnname, 0);
  tlib_pass_if_str_equal("only record_tt", guid, 0);
  tlib_pass_if_int_equal("only record_tt", txn->status.has_outbound_record_tt,
                         1);

  /*
   * Test : Success with 8 elements (to allow future extension)
   */
  txn->status.has_outbound_record_tt = 0;
  nr_header_outbound_response_decoded(txn,
                                      "[\"12345#6789\", \"txnname\", 1.1, 2.2, "
                                      "-1, \"0123456789ABCDEF\", true, "
                                      "\"FUTURISTIC\"]",
                                      &id, &txnname, &guid);
  tlib_pass_if_str_equal("future support", "12345#6789", id);
  tlib_pass_if_str_equal("future support", "txnname", txnname);
  tlib_pass_if_str_equal("future support", "0123456789ABCDEF", guid);
  tlib_pass_if_int_equal("future support", txn->status.has_outbound_record_tt,
                         1);
  nr_free(id);
  nr_free(txnname);
  nr_free(guid);

  txn->status.has_outbound_record_tt = 0;
  nr_header_outbound_response_decoded(
      txn,
      "[\"12345#6789\",\"Controller/admin/blogs/"
      "index\",0.0,0.0,-1,\"1a7b1067d671f6b3\"]",
      &id, &txnname, &guid);
  tlib_pass_if_str_equal("ruby agent", "12345#6789", id);
  tlib_pass_if_str_equal("ruby agent", "Controller/admin/blogs/index", txnname);
  tlib_pass_if_str_equal("ruby agent", "1a7b1067d671f6b3", guid);
  tlib_pass_if_int_equal("ruby agent", txn->status.has_outbound_record_tt, 0);
  nr_free(id);
  nr_free(txnname);
  nr_free(guid);

  nro_delete(app_connect_reply);
}

static void test_outbound_response(void) {
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;
  char* id = 0;
  char* guid = 0;
  char* txnname = 0;
  nrobj_t* app_connect_reply;

  /* Encoded: [\"12345#6789\", \"txnname\", 1.1, 2.2, -1] */
  const char* five_element_response
      = "PxQGU1VXDRAGU1lbFR0XRBBJWF0DCwZAHEIJTAkUFAAdVxVBHFBu";
  /* Encoded: [\"12345#6789\", \"txnname\", 1.1, 2.2, -1, \"0123456789ABCDEF\",
   * true]*/
  const char* seven_element_response
      = "PxQGU1VXDRAGU1lbFR0XRBBJWF0DCwZAHEIJTAkUFAAdVxVBHFAfQkYGBlNVVw0FB1xYI3"
        "VycyMiExoTFhQWB20=";

  txnv.freeze_name_return = NR_SUCCESS;
  txnv.fake_queue_time = 0;
  txnv.fake_trusted = 1;

  app_connect_reply = nro_create_from_json(
      "{\"cross_process_id\":\"1#1\",\"encoding_key\":"
      "\"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\",\"trusted_account_ids\":["
      "12345]}");

  txn->app_connect_reply = app_connect_reply;
  txn->options.cross_process_enabled = 1;
  txn->status.has_outbound_record_tt = 0;
  txn->special_flags.debug_cat = 0;

  /*
   * Test : Bad Params
   */
  nr_header_outbound_response(0, five_element_response, &id, &txnname, &guid);
  tlib_pass_if_str_equal("null txn", id, 0);
  tlib_pass_if_str_equal("null txn", txnname, 0);
  tlib_pass_if_str_equal("null txn", guid, 0);
  tlib_pass_if_int_equal("null txn", txn->status.has_outbound_record_tt, 0);

  nr_header_outbound_response(txn, 0, &id, &txnname, &guid);
  tlib_pass_if_str_equal("null response", id, 0);
  tlib_pass_if_str_equal("null response", txnname, 0);
  tlib_pass_if_str_equal("null response", guid, 0);
  tlib_pass_if_int_equal("null response", txn->status.has_outbound_record_tt,
                         0);

  txn->app_connect_reply = 0;
  nr_header_outbound_response(txn, five_element_response, &id, &txnname, &guid);
  tlib_pass_if_str_equal("no app_connect_reply", id, 0);
  tlib_pass_if_str_equal("no app_connect_reply", txnname, 0);
  tlib_pass_if_str_equal("no app_connect_reply", guid, 0);
  tlib_pass_if_int_equal("no app_connect_reply",
                         txn->status.has_outbound_record_tt, 0);
  txn->app_connect_reply = app_connect_reply;

  nr_header_outbound_response(txn, "", &id, &txnname, &guid);
  tlib_pass_if_str_equal("empty response", id, 0);
  tlib_pass_if_str_equal("empty response", txnname, 0);
  tlib_pass_if_str_equal("empty response", guid, 0);
  tlib_pass_if_int_equal("empty response", txn->status.has_outbound_record_tt,
                         0);

  nr_header_outbound_response(txn, "???????", &id, &txnname, &guid);
  tlib_pass_if_str_equal("junk response", id, 0);
  tlib_pass_if_str_equal("junk response", txnname, 0);
  tlib_pass_if_str_equal("junk response", guid, 0);
  tlib_pass_if_int_equal("junk response", txn->status.has_outbound_record_tt,
                         0);

  /*
   * Test : 5 Element Success
   */
  nr_header_outbound_response(txn, five_element_response, &id, &txnname, &guid);
  tlib_pass_if_str_equal("5 element success", "12345#6789", id);
  tlib_pass_if_str_equal("5 element success", "txnname", txnname);
  tlib_pass_if_str_equal("5 element success", guid, 0);
  tlib_pass_if_int_equal("5 element success",
                         txn->status.has_outbound_record_tt, 0);
  nr_free(id);
  nr_free(txnname);

  /*
   * Test : 7 Element Success
   */
  nr_header_outbound_response(txn, seven_element_response, &id, &txnname,
                              &guid);
  tlib_pass_if_str_equal("5 element success", "12345#6789", id);
  tlib_pass_if_str_equal("5 element success", "txnname", txnname);
  tlib_pass_if_str_equal("5 element success", "0123456789ABCDEF", guid);
  tlib_pass_if_int_equal("5 element success",
                         txn->status.has_outbound_record_tt, 1);
  nr_free(id);
  nr_free(txnname);
  nr_free(guid);

  nro_delete(app_connect_reply);
}

static void test_outbound_request(void) {
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;
  char* x_newrelic_id = NULL;
  char* x_newrelic_transaction = NULL;
  char* x_newrelic_synthetics = NULL;
  char* newrelic = NULL;
  char* traceparent = NULL;
  char* tracestate = NULL;
  char* decoded_x_newrelic_id;
  char* decoded_x_newrelic_transaction;
  char* decoded_x_newrelic_synthetics;
  const char* guid = "0123456789ABCDEF";
  nrobj_t* app_connect_reply;
  nr_hashmap_t* outbound_headers = NULL;

  txnv.fake_guid = guid;
  txnv.fake_trusted = 1;
  txnv.freeze_name_return = NR_SUCCESS;

  app_connect_reply = nro_new_hash();
  nro_set_hash_string(app_connect_reply, "cross_process_id", "12345#6789");
  nro_set_hash_string(app_connect_reply, "encoding_key",
                      "d67afc830dab717fd163bfcb0b8b88423e9a1a3b");
  txn->app_connect_reply = app_connect_reply;

  txn->cat.inbound_guid = NULL;
  txn->cat.referring_path_hash = NULL;
  txn->cat.trip_id = NULL;
  txn->options.cross_process_enabled = 1;
  txn->options.synthetics_enabled = 1;
  txn->special_flags.debug_cat = 0;
  txn->status.recording = 1;
  txn->synthetics = NULL;
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->unscoped_metrics = nrm_table_create(2);
  txn->segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn->abs_start_time = 0;
  txn->segment_root = nr_segment_start(txn, NULL, NULL);

  txn->distributed_trace = nr_distributed_trace_create();
  txn->distributed_trace->inbound.guid = nr_strdup("e10f");
  txn->distributed_trace->account_id = nr_strdup("931d");
  txn->distributed_trace->app_id = nr_strdup("01aa");
  txn->distributed_trace->inbound.raw_tracing_vendors
      = nr_strdup("other1=other1,22@nr=other2");
  txn->options.cross_process_enabled = 0;
  txn->options.distributed_tracing_enabled = 0;
  txn->options.distributed_tracing_exclude_newrelic_header = 0;

  /*
   * Test : Bad Parameters
   */
  outbound_headers = nr_header_outbound_request_create(NULL, NULL);
  tlib_pass_if_null("null txn and segment", outbound_headers);

  outbound_headers = nr_header_outbound_request_create(NULL, txn->segment_root);
  tlib_pass_if_null("null txn", outbound_headers);

  outbound_headers = nr_header_outbound_request_create(txn, NULL);
  tlib_pass_if_null("null segment", outbound_headers);

  /* Config : CAT disabled */
  txn->options.cross_process_enabled = 0;
  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 0,
                            nr_hashmap_count(outbound_headers));

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT enabled with no app_connect_reply */
  txn->options.cross_process_enabled = 1;
  txn->app_connect_reply = 0;
  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 0,
                            nr_hashmap_count(outbound_headers));

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT enabled with app_connect_reply and no guid */
  txn->app_connect_reply = app_connect_reply;
  txnv.fake_guid = NULL;
  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 0,
                            nr_hashmap_count(outbound_headers));

  nr_hashmap_destroy(&outbound_headers);
  txnv.fake_guid = guid;

  /*
   * Test : CAT/DT side-by-side
   */

  /* Config : Both CAT & DT disabled */
  txn->options.cross_process_enabled = 0;
  txn->options.distributed_tracing_enabled = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 0,
                            nr_hashmap_count(outbound_headers));

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT enabled & DT disabled */
  txn->options.cross_process_enabled = 1;
  txn->options.distributed_tracing_enabled = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_id = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                        nr_strlen(X_NEWRELIC_ID));
  x_newrelic_transaction
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                              nr_strlen(X_NEWRELIC_TRANSACTION));
  newrelic = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(NEWRELIC));
  traceparent
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACEPARENT));
  tracestate = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACESTATE));

  tlib_pass_if_not_null("CAT, no DT: x_newrelic_id", x_newrelic_id);
  tlib_pass_if_not_null("CAT, no DT: x_newrelic_transaction",
                        x_newrelic_transaction);
  tlib_pass_if_null("CAT, no DT: newrelic", newrelic);
  tlib_pass_if_null("CAT, no DT: traceparent", traceparent);
  tlib_pass_if_null("CAT, no DT: tracestate", tracestate);

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT disabled & DT enabled */
  txn->options.cross_process_enabled = 0;
  txn->options.distributed_tracing_enabled = 1;
  txn->options.distributed_tracing_exclude_newrelic_header = 0;
  txn->type = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 3,
                            nr_hashmap_count(outbound_headers));

  newrelic = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(NEWRELIC));
  traceparent
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACEPARENT));
  tracestate = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACESTATE));

  tlib_pass_if_true("no CAT, DT", txn->type | NR_TXN_TYPE_DT_OUTBOUND,
                    "txn->type=%d", txn->type);
  tlib_pass_if_not_null("no CAT, DT", newrelic);
  tlib_pass_if_not_null("no CAT, traceparent", traceparent);

  /* Test tracestate + raw tracing vendors is added correctly */
  tlib_pass_if_str_equal(
      "no CAT, tracestate and raw tracing vendors added",
      "190@nr=0-0-212311-51424-d6e4e06002e24189-27856f70d3d314b7-1-0.421-"
      "1482959525577,other1=other1,22@nr=other2",
      tracestate);

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT disabled, DT enabled, newrelic headers excluded */
  txn->options.cross_process_enabled = 0;
  txn->options.distributed_tracing_enabled = 1;
  txn->options.distributed_tracing_exclude_newrelic_header = 1;
  txn->type = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  newrelic = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(NEWRELIC));
  traceparent
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACEPARENT));
  tracestate = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACESTATE));

  tlib_pass_if_true("no CAT, DT only W3C", txn->type | NR_TXN_TYPE_DT_OUTBOUND,
                    "txn->type=%d", txn->type);
  tlib_pass_if_null("no CAT, DT only W3C", newrelic);
  tlib_pass_if_not_null("no CAT, traceparent", traceparent);

  /* Test tracestate + raw tracing vendors is added correctly */
  tlib_pass_if_str_equal(
      "no CAT, tracestate and raw tracing vendors added",
      "190@nr=0-0-212311-51424-d6e4e06002e24189-27856f70d3d314b7-1-0.421-"
      "1482959525577,other1=other1,22@nr=other2",
      tracestate);

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT & DT enabled */
  txn->options.cross_process_enabled = 1;
  txn->options.distributed_tracing_enabled = 1;
  txn->options.distributed_tracing_exclude_newrelic_header = 0;

  nr_free(txn->distributed_trace->inbound.raw_tracing_vendors);

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 3,
                            nr_hashmap_count(outbound_headers));

  newrelic = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(NEWRELIC));
  traceparent
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACEPARENT));
  tracestate = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACESTATE));
  x_newrelic_id
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(X_NEWRELIC_ID));
  x_newrelic_transaction = (char*)nr_hashmap_get(
      outbound_headers, NR_PSTR(X_NEWRELIC_TRANSACTION));

  tlib_pass_if_not_null("CAT & DT: newrelic", newrelic);
  tlib_pass_if_not_null("CAT & DT: traceparent", traceparent);
  tlib_pass_if_null("CAT & DT: x_newrelic_id", x_newrelic_id);
  tlib_pass_if_null("CAT & DT: x_newrelic_transaction", x_newrelic_transaction);

  /* Test tracestate + raw tracing vendors is added correctly */
  tlib_pass_if_str_equal(
      "no CAT, tracestate and raw tracing vendors added",
      "190@nr=0-0-212311-51424-d6e4e06002e24189-27856f70d3d314b7-1-0.421-"
      "1482959525577",
      tracestate);

  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT & DT enabled, newrelic headers excluded */
  txn->options.cross_process_enabled = 1;
  txn->options.distributed_tracing_enabled = 1;
  txn->options.distributed_tracing_exclude_newrelic_header = 1;

  nr_free(txn->distributed_trace->inbound.raw_tracing_vendors);

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  newrelic = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(NEWRELIC));
  traceparent
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACEPARENT));
  tracestate = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(W3C_TRACESTATE));
  x_newrelic_id
      = (char*)nr_hashmap_get(outbound_headers, NR_PSTR(X_NEWRELIC_ID));
  x_newrelic_transaction = (char*)nr_hashmap_get(
      outbound_headers, NR_PSTR(X_NEWRELIC_TRANSACTION));

  tlib_pass_if_null("CAT & DT (only W3C): newrelic", newrelic);
  tlib_pass_if_not_null("CAT & DT (only W3C): traceparent", traceparent);
  tlib_pass_if_null("CAT & DT (only W3C): x_newrelic_id", x_newrelic_id);
  tlib_pass_if_null("CAT & DT (only W3C): x_newrelic_transaction",
                    x_newrelic_transaction);

  /* Test tracestate + raw tracing vendors is added correctly */
  tlib_pass_if_str_equal(
      "no CAT, tracestate and raw tracing vendors added",
      "190@nr=0-0-212311-51424-d6e4e06002e24189-27856f70d3d314b7-1-0.421-"
      "1482959525577",
      tracestate);

  nr_hashmap_destroy(&outbound_headers);

  /*
   * Test : CAT
   */

  /* Config : CAT enabled & DT disabled */
  txn->options.cross_process_enabled = 1;
  txn->options.distributed_tracing_enabled = 0;
  txn->options.distributed_tracing_exclude_newrelic_header = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_id = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                        nr_strlen(X_NEWRELIC_ID));
  x_newrelic_transaction
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                              nr_strlen(X_NEWRELIC_TRANSACTION));

  tlib_pass_if_str_equal("success", "VQQEVVNADgQIXQ==", x_newrelic_id);
  tlib_pass_if_str_equal("success",
                         "PxQHUFRQDAYGU1lbdnN0IiF3FB8EBw8RVU4aUgkKBwYGUw5ZCCBxI"
                         "SBzcUNKQQkBA1BUVAAJFTs=",
                         x_newrelic_transaction);
  tlib_fail_if_int_equal("txn type", 0, NR_TXN_TYPE_CAT_OUTBOUND & txn->type);

  decoded_x_newrelic_id = nr_deobfuscate(
      x_newrelic_id, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);
  decoded_x_newrelic_transaction = nr_deobfuscate(
      x_newrelic_transaction, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);

  tlib_pass_if_str_equal("success", "12345#6789", decoded_x_newrelic_id);
  tlib_pass_if_str_equal(
      "success",
      "[\"0123456789ABCDEF\",false,\"0123456789ABCDEF\",\"12345678\"]",
      decoded_x_newrelic_transaction);

  nr_free(decoded_x_newrelic_id);
  nr_free(decoded_x_newrelic_transaction);
  nr_hashmap_destroy(&outbound_headers);

  /*
   * Test : Synthetics
   */

  /* Config : CAT enabled & DT disabled */
  txn->options.cross_process_enabled = 1;
  txn->options.distributed_tracing_enabled = 0;
  txn->synthetics = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 3,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_id = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                        nr_strlen(X_NEWRELIC_ID));
  x_newrelic_transaction
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                              nr_strlen(X_NEWRELIC_TRANSACTION));
  x_newrelic_synthetics
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_SYNTHETICS,
                              nr_strlen(X_NEWRELIC_SYNTHETICS));

  tlib_pass_if_str_equal("synthetics", "VQQEVVNADgQIXQ==", x_newrelic_id);
  tlib_pass_if_str_equal("synthetics",
                         "PxQHUFRQDAYGU1lbdnN0IiF3FB8EBw8RVU4aUgkKBwYGUw5ZCCBxI"
                         "SBzcUNKQQkBA1BUVAAJFTs=",
                         x_newrelic_transaction);
  tlib_pass_if_str_equal("synthetics",
                         "PwcbUFZTFBFRRk1AVRMbRAcTaw==", x_newrelic_synthetics);

  decoded_x_newrelic_id = nr_deobfuscate(
      x_newrelic_id, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);
  decoded_x_newrelic_transaction = nr_deobfuscate(
      x_newrelic_transaction, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);
  decoded_x_newrelic_synthetics = nr_deobfuscate(
      x_newrelic_synthetics, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);

  tlib_pass_if_str_equal("synthetics", "12345#6789", decoded_x_newrelic_id);
  tlib_pass_if_str_equal(
      "synthetics",
      "[\"0123456789ABCDEF\",false,\"0123456789ABCDEF\",\"12345678\"]",
      decoded_x_newrelic_transaction);
  tlib_pass_if_str_equal("synthetics", "[1,100,\"a\",\"b\",\"c\"]",
                         decoded_x_newrelic_synthetics);

  nr_free(decoded_x_newrelic_id);
  nr_free(decoded_x_newrelic_transaction);
  nr_free(decoded_x_newrelic_synthetics);
  nr_hashmap_destroy(&outbound_headers);

  /* Config : CAT enabled with Synthetics + DT disabled */
  txn->options.synthetics_enabled = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_id = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                        nr_strlen(X_NEWRELIC_ID));
  x_newrelic_transaction
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                              nr_strlen(X_NEWRELIC_TRANSACTION));

  tlib_pass_if_str_equal("synthetics", "VQQEVVNADgQIXQ==", x_newrelic_id);
  tlib_pass_if_str_equal("synthetics",
                         "PxQHUFRQDAYGU1lbdnN0IiF3FB8EBw8RVU4aUgkKBwYGUw5ZCCBxI"
                         "SBzcUNKQQkBA1BUVAAJFTs=",
                         x_newrelic_transaction);

  decoded_x_newrelic_id = nr_deobfuscate(
      x_newrelic_id, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);
  decoded_x_newrelic_transaction = nr_deobfuscate(
      x_newrelic_transaction, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);

  tlib_pass_if_str_equal("synthetics", "12345#6789", decoded_x_newrelic_id);
  tlib_pass_if_str_equal(
      "synthetics",
      "[\"0123456789ABCDEF\",false,\"0123456789ABCDEF\",\"12345678\"]",
      decoded_x_newrelic_transaction);

  nr_free(decoded_x_newrelic_id);
  nr_free(decoded_x_newrelic_transaction);
  nr_hashmap_destroy(&outbound_headers);

  /* Config : Synthetics enabled with CAT + DT disabled */
  txn->options.synthetics_enabled = 1;
  txn->options.cross_process_enabled = 0;

  outbound_headers = nr_header_outbound_request_create(txn, txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 1,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_synthetics
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_SYNTHETICS,
                              nr_strlen(X_NEWRELIC_SYNTHETICS));

  tlib_pass_if_str_equal("synthetics",
                         "PwcbUFZTFBFRRk1AVRMbRAcTaw==", x_newrelic_synthetics);

  decoded_x_newrelic_synthetics = nr_deobfuscate(
      x_newrelic_synthetics, "d67afc830dab717fd163bfcb0b8b88423e9a1a3b", 0);

  tlib_pass_if_str_equal("synthetics", "[1,100,\"a\",\"b\",\"c\"]",
                         decoded_x_newrelic_synthetics);

  nr_free(decoded_x_newrelic_synthetics);
  nr_hashmap_destroy(&outbound_headers);

  nro_delete(app_connect_reply);
  nr_segment_destroy_tree(txn->segment_root);
  nr_synthetics_destroy(&txn->synthetics);
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nrm_table_destroy(&txn->unscoped_metrics);
  nr_slab_destroy(&txn->segment_slab);
}

static void test_lifecycle(void) {
  mock_txn client_txnv;
  nrtxn_t* client_txn = &client_txnv.txn;
  mock_txn external_txnv;
  nrtxn_t* external_txn = &external_txnv.txn;

  char* x_newrelic_id = 0;
  char* x_newrelic_transaction = 0;
  char* x_newrelic_app_data = 0;
  char* external_id = 0;
  char* external_txnname = 0;
  char* external_guid = 0;
  nr_hashmap_t* outbound_headers = NULL;

  nrobj_t* shared_app_connect_reply;

  client_txnv.freeze_name_return = NR_SUCCESS;
  client_txnv.fake_guid = "CLIENT_GUID";
  client_txnv.fake_queue_time = 0;
  client_txnv.fake_trusted = 1;

  external_txnv.freeze_name_return = NR_SUCCESS;
  external_txnv.fake_guid = "EXTERNAL_GUID";
  external_txnv.fake_queue_time = 0;
  external_txnv.fake_trusted = 1;

  shared_app_connect_reply = nro_create_from_json(
      "{\"cross_process_id\":\"12345#6789\",\"encoding_key\":"
      "\"d67afc830dab717fd163bfcb0b8b88423e9a1a3b\",\"trusted_account_ids\":["
      "12345]}");

  client_txn->app_connect_reply = shared_app_connect_reply;
  client_txn->cat.inbound_guid = NULL;
  client_txn->cat.referring_path_hash = NULL;
  client_txn->cat.trip_id = NULL;
  client_txn->options.cross_process_enabled = 1;
  client_txn->options.distributed_tracing_enabled = 0;
  client_txn->options.synthetics_enabled = 1;
  client_txn->special_flags.debug_cat = 0;
  client_txn->status.recording = 1;
  client_txn->synthetics = NULL;
  client_txn->segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  client_txn->abs_start_time = 0;
  client_txn->segment_root = nr_segment_start(client_txn, NULL, NULL);

  external_txn->app_connect_reply = shared_app_connect_reply;
  external_txn->cat.inbound_guid = NULL;
  external_txn->cat.referring_path_hash = NULL;
  external_txn->cat.trip_id = NULL;
  external_txn->unscoped_metrics = nrm_table_create(10);
  external_txn->options.cross_process_enabled = 1;
  external_txn->options.distributed_tracing_enabled = 0;
  external_txn->name = nr_strdup("EXTERNAL_TXNNAME");
  external_txn->intrinsics = nro_new_hash();
  external_txn->status.recording = 1;
  external_txn->special_flags.debug_cat = 0;
  external_txnv.unfinished_duration = 123 * NR_TIME_DIVISOR;
  external_txnv.fake_queue_time = 1 * NR_TIME_DIVISOR;
  external_txn->status.cross_process = NR_STATUS_CROSS_PROCESS_START;
  external_txn->type = 0;
  external_txn->cat.client_cross_process_id = NULL;

  /*
   * Client Transaction: Create the outbound headers.
   */
  outbound_headers
      = nr_header_outbound_request_create(client_txn, client_txn->segment_root);
  tlib_pass_if_size_t_equal("outbound headers hashmap size", 2,
                            nr_hashmap_count(outbound_headers));

  x_newrelic_id = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                        nr_strlen(X_NEWRELIC_ID));
  x_newrelic_transaction
      = (char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                              nr_strlen(X_NEWRELIC_TRANSACTION));

  /*
   * External Transaction: Process inbound headers and create return header.
   */
  nr_header_set_cat_txn(external_txn, x_newrelic_id, x_newrelic_transaction);
  x_newrelic_app_data = nr_header_inbound_response(external_txn, -1);

  /*
   * Client Transaction: Process return header.
   */
  nr_header_outbound_response(client_txn, x_newrelic_app_data, &external_id,
                              &external_txnname, &external_guid);

  test_obj_as_json("full lifecycle", external_txn->intrinsics,
                   "{\"referring_transaction_guid\":\"CLIENT_GUID\",\"client_"
                   "cross_process_id\":\"12345#6789\"}");
  tlib_pass_if_str_equal("full lifecycle", external_id, "12345#6789");
  tlib_pass_if_str_equal("full lifecycle", external_txnname,
                         "EXTERNAL_TXNNAME");
  tlib_pass_if_str_equal("full lifecycle", external_guid, "EXTERNAL_GUID");

  nr_segment_destroy_tree(client_txn->segment_root);
  nr_slab_destroy(&client_txn->segment_slab);
  nr_free(external_txn->cat.client_cross_process_id);
  nr_free(external_txn->cat.inbound_guid);
  nr_free(external_txn->cat.referring_path_hash);
  nr_free(external_txn->cat.trip_id);
  nr_free(external_txn->name);
  nrm_table_destroy(&external_txn->unscoped_metrics);
  nro_delete(external_txn->intrinsics);

  nro_delete(shared_app_connect_reply);
  nr_free(external_id);
  nr_free(external_txnname);
  nr_free(external_guid);
  nr_free(x_newrelic_app_data);
  nr_hashmap_destroy(&outbound_headers);
}

#define extract_testcase(...) \
  extract_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void extract_testcase_fn(const char* testname,
                                const char* header_name,
                                const char* input,
                                const char* expected,
                                const char* file,
                                int line) {
  char* actual = nr_header_extract_encoded_value(header_name, input);

  test_pass_if_true(testname, 0 == nr_strcmp(expected, actual),
                    "input=%s expected=%s actual=%s header_name=%s",
                    NRSAFESTR(input), NRSAFESTR(expected), NRSAFESTR(actual),
                    NRSAFESTR(header_name));
  nr_free(actual);
}

static void test_extract_encoded_value(void) {
  extract_testcase("null params", 0, 0, 0);
  extract_testcase("null input", "App-Data", 0, 0);
  extract_testcase("null header name", 0, "_App-Data: 5555\n_", 0);
  extract_testcase("no match", "Zap-Data", "_App-Data: 5555\n_", 0);
  extract_testcase("no match", "App-Data", "p-Data: 5555\n_", 0);
  extract_testcase("no value", "App-Data", "App-Data: ", 0);
  extract_testcase("no value", "App-Data", "__App-Data", 0);

  extract_testcase("success", "App-Data", "_App-Data: 5555\n_", "5555");
  extract_testcase("case insensitive", "App-Data", "_APP-DaTa: 5555\n_",
                   "5555");
}

static void test_validate_encoded_string(void) {
  nr_status_t rv;

  rv = nr_header_validate_encoded_string(0);
  tlib_pass_if_true("empty string", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("");
  tlib_pass_if_true("empty string", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("0123456789");
  tlib_pass_if_true("numbers", NR_SUCCESS == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("abcdefghijklmnopqrstuvwxyz");
  tlib_pass_if_true("lowercase letters", NR_SUCCESS == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  tlib_pass_if_true("uppercase letters", NR_SUCCESS == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("=/+");
  tlib_pass_if_true("equals slash plus", NR_SUCCESS == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("-");
  tlib_pass_if_true("hypen", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("*");
  tlib_pass_if_true("star", NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_header_validate_encoded_string("[");
  tlib_pass_if_true("bracket", NR_FAILURE == rv, "rv=%d", (int)rv);
}

static void test_format_name_value(void) {
  char* hdr;

  hdr = nr_header_format_name_value(0, 0, 0);
  tlib_pass_if_true("zero params", 0 == hdr, "hdr=%s", NRSAFESTR(hdr));

  hdr = nr_header_format_name_value("my_name", 0, 0);
  tlib_pass_if_true("null value", 0 == hdr, "hdr=%s", NRSAFESTR(hdr));

  hdr = nr_header_format_name_value(0, "my_value", 0);
  tlib_pass_if_true("null name", 0 == hdr, "hdr=%s", NRSAFESTR(hdr));

  hdr = nr_header_format_name_value("my_name", "my_value", 0);
  tlib_pass_if_true("no suffix", 0 == nr_strcmp("my_name: my_value", hdr),
                    "hdr=%s", NRSAFESTR(hdr));
  nr_free(hdr);

  hdr = nr_header_format_name_value("my_name", "my_value", 1);
  tlib_pass_if_true("with suffix", 0 == nr_strcmp("my_name: my_value\r\n", hdr),
                    "hdr=%s", NRSAFESTR(hdr));
  nr_free(hdr);
}

static void test_bad_content_type(void) {
  tlib_pass_if_null("NULL content-type", nr_header_parse_content_type(0));
  tlib_pass_if_null("empty content-type", nr_header_parse_content_type(""));
  tlib_pass_if_null("missing header-name", nr_header_parse_content_type(":"));
  tlib_pass_if_null("all whitespace content-type",
                    nr_header_parse_content_type("     "));
  tlib_pass_if_null("missing media-type",
                    nr_header_parse_content_type("Content-Type: ; foo=bar"));
  tlib_pass_if_null("missing media-type type",
                    nr_header_parse_content_type("Content-Type: /html"));
  tlib_pass_if_null("missing media subtype",
                    nr_header_parse_content_type("Content-Type: text;"));
  tlib_pass_if_null("empty media subtype",
                    nr_header_parse_content_type("Content-Type: text/"));
  tlib_pass_if_null("invalid media subtype",
                    nr_header_parse_content_type("Content-Type: audio/mp3[]"));
  tlib_pass_if_null("leading colon",
                    nr_header_parse_content_type(":text/html"));
  tlib_pass_if_null("double colon",
                    nr_header_parse_content_type("Content-Type::text/html"));
}

static void test_extract_content_type(void) {
  char* mimetype;

  mimetype = nr_header_parse_content_type("Content-Type:");
  tlib_pass_if_str_equal("empty media-type", mimetype, "");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("Content-Type:text/html");
  tlib_pass_if_str_equal("content-type no whitespace", mimetype, "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("Content-Type: \t  text/html");
  tlib_pass_if_str_equal("content-type leading whitespace", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("Content-Type:text/html \t ");
  tlib_pass_if_str_equal("content-type trailing whitespace", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type(
      "Content-Type: text/html; charset=\"utf-8\"");
  tlib_pass_if_str_equal("content-type with charset", mimetype, "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type(
      "Content-Type:text/html;charset=\"utf-8\";foo=bar;");
  tlib_pass_if_str_equal("content-type with multiple parameters", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("Content-Type:TEXT/HTML");
  tlib_pass_if_str_equal("content-type all caps", mimetype, "TEXT/HTML");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("text/html; charset=\"utf-8\"");
  tlib_pass_if_str_equal("content-type with no name and with charset", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("text/html");
  tlib_pass_if_str_equal("content-type with only media-type", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("     text/html");
  tlib_pass_if_str_equal(
      "content-type with leading whitespace and only media-type", mimetype,
      "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("text/html; charset=\"utf-8\"");
  tlib_pass_if_str_equal("content-type with no name and with charset", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("text/html");
  tlib_pass_if_str_equal("content-type with only media-type", mimetype,
                         "text/html");
  nr_free(mimetype);

  mimetype = nr_header_parse_content_type("     text/html");
  tlib_pass_if_str_equal(
      "content-type with leading whitespace and only media-type", mimetype,
      "text/html");
  nr_free(mimetype);
}

static void test_set_cat_txn(void) {
  const char* trusted_id = "1#3";
  const char* trusted_transaction_v1 = "[\"guid\",false]";
  const char* trusted_transaction_v2 = "[\"guid\",false,\"trip\",\"01234567\"]";
  char* encoded_id;
  char* encoded_transaction_v1;
  char* encoded_transaction_v2;
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;

  encoded_id = nr_obfuscate(trusted_id, ENCODING_KEY, 0);
  encoded_transaction_v1
      = nr_obfuscate(trusted_transaction_v1, ENCODING_KEY, 0);
  encoded_transaction_v2
      = nr_obfuscate(trusted_transaction_v2, ENCODING_KEY, 0);

  txn->app_connect_reply = nro_create_from_json(
      "{\"trusted_account_ids\":[1,3],\"encoding_key\":\"" ENCODING_KEY "\"}");
  nr_memset((void*)&txn->cat, 0, sizeof(txn->cat));
  txn->intrinsics = nro_new_hash();
  txn->synthetics = NULL;
  txn->type = 0;
  txnv.fake_trusted = 1;
  txn->cat.client_cross_process_id = NULL;
  txn->special_flags.debug_cat = 0;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL txn",
      nr_header_set_cat_txn(NULL, encoded_id, encoded_transaction_v1));
  tlib_pass_if_status_failure(
      "NULL id header",
      nr_header_set_cat_txn(txn, NULL, encoded_transaction_v1));

  /*
   * Test : Invalid headers.
   */
  tlib_pass_if_status_failure(
      "invalid id header",
      nr_header_set_cat_txn(txn, trusted_id, encoded_transaction_v1));
  tlib_pass_if_int_equal("invalid id header", 0, txn->type);

  tlib_pass_if_status_failure(
      "invalid txn header",
      nr_header_set_cat_txn(txn, encoded_id, trusted_transaction_v1));
  tlib_pass_if_str_equal("invalid txn header", trusted_id,
                         txn->cat.client_cross_process_id);
  nr_free(txn->cat.client_cross_process_id);

  /*
   * Test : Untrusted account.
   */
  txnv.fake_trusted = 0;
  tlib_pass_if_status_failure(
      "untrusted",
      nr_header_set_cat_txn(txn, encoded_id, encoded_transaction_v1));
  tlib_pass_if_int_equal("untrusted", 0, txn->type);
  txnv.fake_trusted = 1;

  /*
   * Test : No txn header
   */
  tlib_pass_if_status_failure("Only X-NewRelic-ID present",
                              nr_header_set_cat_txn(txn, encoded_id, NULL));
  tlib_pass_if_str_equal("Only X-NewRelic-ID present", trusted_id,
                         txn->cat.client_cross_process_id);
  nr_free(txn->cat.client_cross_process_id);

  /*
   * Test : Good CATv1 headers.
   */
  txn->status.has_inbound_record_tt = 1;
  tlib_pass_if_status_success(
      "CATv1", nr_header_set_cat_txn(txn, encoded_id, encoded_transaction_v1));
  tlib_pass_if_str_equal("CATv1 type", trusted_id,
                         txn->cat.client_cross_process_id);
  tlib_pass_if_true("CATv1 type", NR_TXN_TYPE_CAT_INBOUND & txn->type,
                    "txn->type=%d", txn->type);
  tlib_pass_if_str_equal("CATv1 guid", "guid", txn->cat.inbound_guid);
  tlib_pass_if_int_equal("CATv1 record_tt", 0,
                         txn->status.has_inbound_record_tt);
  tlib_pass_if_null("CATv1 referring_path_hash", txn->cat.referring_path_hash);
  tlib_pass_if_null("CATv1 trip_id", txn->cat.trip_id);

  nr_free(txn->cat.client_cross_process_id);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);
  nr_memset((void*)&txn->cat, 0, sizeof(txn->cat));

  nro_delete(txn->intrinsics);
  txn->intrinsics = nro_new_hash();

  /*
   * Test : Good CATv2 headers.
   */
  txn->status.has_inbound_record_tt = 1;
  tlib_pass_if_status_success(
      "CATv2", nr_header_set_cat_txn(txn, encoded_id, encoded_transaction_v2));
  tlib_pass_if_str_equal("CATv2 type", trusted_id,
                         txn->cat.client_cross_process_id);
  tlib_pass_if_true("CATv2 type", NR_TXN_TYPE_CAT_INBOUND & txn->type,
                    "txn->type=%d", txn->type);
  tlib_pass_if_str_equal("CATv2 guid", "guid", txn->cat.inbound_guid);
  tlib_pass_if_int_equal("CATv2 record_tt", 0,
                         txn->status.has_inbound_record_tt);
  tlib_pass_if_str_equal("CATv2 referring_path_hash", "01234567",
                         txn->cat.referring_path_hash);
  tlib_pass_if_str_equal("CATv2 trip_id", "trip", txn->cat.trip_id);

  nr_free(txn->cat.client_cross_process_id);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);

  nr_free(encoded_id);
  nr_free(encoded_transaction_v1);
  nr_free(encoded_transaction_v2);
  nro_delete(txn->app_connect_reply);
  nro_delete(txn->intrinsics);
}

static void test_set_synthetics_txn(void) {
  const char* trusted_json = "[1,3,\"a\",\"b\",\"c\"]";
  char* encoded;
  mock_txn txnv;
  nrtxn_t* txn = &txnv.txn;

  encoded = nr_obfuscate(trusted_json, ENCODING_KEY, 0);

  txn->app_connect_reply = nro_create_from_json(
      "{\"trusted_account_ids\":[1,3],\"encoding_key\":\"" ENCODING_KEY "\"}");
  txn->special_flags.debug_cat = 0;
  txn->synthetics = NULL;
  txn->type = 0;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL txn",
                              nr_header_set_synthetics_txn(NULL, encoded));

  tlib_pass_if_status_failure("NULL header",
                              nr_header_set_synthetics_txn(txn, NULL));
  tlib_pass_if_null("NULL header", txn->synthetics);
  tlib_pass_if_int_equal("NULL header", 0, txn->type);

  /*
   * Test : Transaction has synthetics.
   */
  txn->synthetics = nr_synthetics_create(trusted_json);
  tlib_pass_if_status_failure("synthetics txn",
                              nr_header_set_synthetics_txn(txn, encoded));
  nr_synthetics_destroy(&txn->synthetics);

  /*
   * Test : Invalid header.
   */
  tlib_pass_if_status_failure("invalid header",
                              nr_header_set_synthetics_txn(txn, "foo"));
  tlib_pass_if_null("invalid header", txn->synthetics);
  tlib_pass_if_int_equal("invalid header", 0, txn->type);

  /*
   * Test : Untrusted account.
   */
  txnv.fake_trusted = 0;
  tlib_pass_if_status_failure("untrusted",
                              nr_header_set_synthetics_txn(txn, encoded));
  tlib_pass_if_null("untrusted", txn->synthetics);
  tlib_pass_if_int_equal("untrusted", 0, txn->type);

  /*
   * Test : Good header.
   */
  txnv.fake_trusted = 1;
  tlib_pass_if_status_success("valid",
                              nr_header_set_synthetics_txn(txn, encoded));
  tlib_pass_if_not_null("valid", txn->synthetics);
  tlib_pass_if_true("valid", NR_TXN_TYPE_SYNTHETICS & txn->type, "txn->type=%d",
                    txn->type);
  nr_synthetics_destroy(&txn->synthetics);

  nr_free(encoded);
  nro_delete(txn->app_connect_reply);
}

static void test_account_id_from_cross_process_id(void) {
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int64_t_equal("NULL string", -1,
                             nr_header_account_id_from_cross_process_id(NULL));
  tlib_pass_if_int64_t_equal("empty string", -1,
                             nr_header_account_id_from_cross_process_id(""));
  tlib_pass_if_int64_t_equal("malformed string", -1,
                             nr_header_account_id_from_cross_process_id("foo"));
  tlib_pass_if_int64_t_equal(
      "malformed string", -1,
      nr_header_account_id_from_cross_process_id("foo#bar"));
  tlib_pass_if_int64_t_equal(
      "missing hash", -1, nr_header_account_id_from_cross_process_id("01234"));
  tlib_pass_if_int64_t_equal(
      "overflow", -1,
      nr_header_account_id_from_cross_process_id("3000000000#1"));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_int64_t_equal(
      "normal operation", 10,
      nr_header_account_id_from_cross_process_id("10#10"));
}

static void test_nr_header_create_distributed_trace_map(void) {
  nr_hashmap_t* header_map = NULL;
  char* tracestate = "tracestate";
  char* traceparent = "traceparent";
  char* dt_payload = "newrelic";

  header_map = nr_header_create_distributed_trace_map(NULL, NULL, NULL);
  tlib_pass_if_null(
      "NULL payload and NULL traceparent should return NULL header map",
      header_map);

  header_map = nr_header_create_distributed_trace_map(NULL, NULL, tracestate);
  tlib_pass_if_null(
      "NULL payload and NULL traceparent should return NULL header map",
      header_map);

  header_map = nr_header_create_distributed_trace_map(dt_payload, NULL, NULL);
  tlib_pass_if_not_null("if valid dt_payload should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "1 header passed in so should expect headers hashmap size of 1", 1,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);

  header_map
      = nr_header_create_distributed_trace_map(dt_payload, traceparent, NULL);
  tlib_pass_if_not_null("if valid dt_payload should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "2 headers passed in so should expect headers hashmap size of 2", 2,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);

  header_map
      = nr_header_create_distributed_trace_map(dt_payload, NULL, tracestate);
  tlib_pass_if_not_null("if valid dt_payload should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "2 headers passed in so should expect headers hashmap size of 2", 2,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);

  header_map = nr_header_create_distributed_trace_map(dt_payload, traceparent,
                                                      tracestate);
  tlib_pass_if_not_null("if valid dt_payload should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "3 headers passed in so should expect headers hashmap size of 3", 3,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);

  header_map
      = nr_header_create_distributed_trace_map(NULL, traceparent, tracestate);
  tlib_pass_if_not_null("if valid traceparent should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "Two headers passed in so should expect headers hashmap size of 2", 2,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);

  header_map = nr_header_create_distributed_trace_map(NULL, traceparent, NULL);
  tlib_pass_if_not_null("if valid traceparent should return a header map",
                        header_map);
  tlib_pass_if_size_t_equal(
      "1 header passed in so should expect headers hashmap size of 1", 1,
      nr_hashmap_count(header_map));
  nr_hashmap_destroy(&header_map);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_encode_decode();
  test_validate_decoded_id();
  test_inbound_response_internal();
  test_inbound_response();
  test_outbound_response_decoded();
  test_outbound_response();
  test_outbound_request();
  test_lifecycle();
  test_extract_encoded_value();
  test_validate_encoded_string();
  test_format_name_value();
  test_bad_content_type();
  test_extract_content_type();
  test_set_cat_txn();
  test_set_synthetics_txn();
  test_account_id_from_cross_process_id();
  test_nr_header_create_distributed_trace_map();
}
