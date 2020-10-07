/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_header.h"
#include "nr_header_private.h"
#include "nr_synthetics.h"
#include "nr_synthetics_private.h"
#include "nr_txn_private.h"
#include "util_memory.h"
#include "util_obfuscate.h"
#include "util_object.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

nrapp_t* nr_app_verify_id(nrapplist_t* applist NRUNUSED,
                          const char* agent_run_id NRUNUSED) {
  return 0;
}

static void test_create(void) {
  nr_synthetics_t* synthetics;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL header", nr_synthetics_create(NULL));
  tlib_pass_if_null("empty header", nr_synthetics_create(""));
  tlib_pass_if_null("malformed JSON", nr_synthetics_create("foo"));

  /*
   * Test : Incorrect JSON types.
   */
  tlib_pass_if_null("boolean", nr_synthetics_create("true"));
  tlib_pass_if_null("number", nr_synthetics_create("42"));
  tlib_pass_if_null("string", nr_synthetics_create("\"foo\""));
  tlib_pass_if_null("hash", nr_synthetics_create("{\"foo\":\"bar\"}"));
  tlib_pass_if_null("null", nr_synthetics_create("null"));

  /*
   * Test : Unsupported versions.
   */
  tlib_pass_if_null("version 0", nr_synthetics_create("[0]"));
  tlib_pass_if_null("version 9", nr_synthetics_create("[9]"));

  /*
   * V1 parsing tests can be found in test_parse_v1: this includes testing
   * malformed v1 arrays and general parsing.
   */

  /*
   * Test : Supported version.
   */
  synthetics = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");
  tlib_pass_if_not_null("version 1", synthetics);
  nr_synthetics_destroy(&synthetics);
}

static void test_parse_v1_invalid_json(const char* message, const char* json) {
  nrobj_t* obj = NULL;
  nr_synthetics_t* synthetics = NULL;

  obj = nro_create_from_json(json);
  synthetics = (nr_synthetics_t*)nr_zalloc(sizeof(nr_synthetics_t));

  tlib_pass_if_status_failure(message, nr_synthetics_parse_v1(obj, synthetics));

  nro_delete(obj);
  nr_synthetics_destroy(&synthetics);
}

static void test_parse_v1(void) {
  nrobj_t* obj = NULL;
  nr_synthetics_t* synthetics = NULL;

  /*
   * Test : Bad parameters.
   */
  obj = nro_create_from_json("[1,100,\"a\",\"b\",\"c\"]");
  synthetics = (nr_synthetics_t*)nr_zalloc(sizeof(nr_synthetics_t));

  tlib_pass_if_status_failure("both NULL", nr_synthetics_parse_v1(NULL, NULL));
  tlib_pass_if_status_failure("NULL synth_obj",
                              nr_synthetics_parse_v1(NULL, synthetics));
  tlib_pass_if_status_failure("NULL out", nr_synthetics_parse_v1(obj, NULL));

  nro_delete(obj);
  nr_synthetics_destroy(&synthetics);

  /*
   * Test : Invalid input.
   */
  test_parse_v1_invalid_json("<5 elements", "[1,100,\"a\",\"b\"]");
  test_parse_v1_invalid_json("element 0 invalid", "[{},100,\"a\",\"b\",\"c\"]");
  test_parse_v1_invalid_json("element 1 invalid", "[1,{},\"a\",\"b\",\"c\"]");
  test_parse_v1_invalid_json("element 2 invalid", "[1,100,{},\"b\",\"c\"]");
  test_parse_v1_invalid_json("element 3 invalid", "[1,100,\"a\",{},\"c\"]");
  test_parse_v1_invalid_json("element 4 invalid", "[1,100,\"a\",\"b\",{}]");

  /*
   * Test : Valid input.
   */
  obj = nro_create_from_json("[1,100,\"a\",\"b\",\"c\"]");
  synthetics = (nr_synthetics_t*)nr_zalloc(sizeof(nr_synthetics_t));

  tlib_pass_if_status_success("valid JSON",
                              nr_synthetics_parse_v1(obj, synthetics));
  tlib_pass_if_int_equal("version", 1, synthetics->version);
  tlib_pass_if_int_equal("account id", 100, synthetics->account_id);
  tlib_pass_if_str_equal("resource id", "a", synthetics->resource_id);
  tlib_pass_if_str_equal("job id", "b", synthetics->job_id);
  tlib_pass_if_str_equal("monitor id", "c", synthetics->monitor_id);

  nro_delete(obj);
  nr_synthetics_destroy(&synthetics);
}

static void test_destroy(void) {
  nr_synthetics_t* null_synth = NULL;

  /*
   * We're just testing to ensure no crashes, basically.
   */
  nr_synthetics_destroy(NULL);
  nr_synthetics_destroy(&null_synth);
}

static void test_version(void) {
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL synthetics", 0, nr_synthetics_version(NULL));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_int_equal("valid synthetics", 1,
                         nr_synthetics_version(synthetics));

  nr_synthetics_destroy(&synthetics);
}

static void test_account_id(void) {
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL synthetics", 0, nr_synthetics_account_id(NULL));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_int_equal("valid synthetics", 100,
                         nr_synthetics_account_id(synthetics));

  nr_synthetics_destroy(&synthetics);
}

static void test_resource_id(void) {
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL synthetics", nr_synthetics_resource_id(NULL));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_str_equal("valid synthetics", "a",
                         nr_synthetics_resource_id(synthetics));

  nr_synthetics_destroy(&synthetics);
}

static void test_job_id(void) {
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL synthetics", nr_synthetics_job_id(NULL));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_str_equal("valid synthetics", "b",
                         nr_synthetics_job_id(synthetics));

  nr_synthetics_destroy(&synthetics);
}

static void test_monitor_id(void) {
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL synthetics", nr_synthetics_monitor_id(NULL));

  /*
   * Test : Good parameters.
   */
  tlib_pass_if_str_equal("valid synthetics", "c",
                         nr_synthetics_monitor_id(synthetics));

  nr_synthetics_destroy(&synthetics);
}

static void test_outbound_header(void) {
  const char* header = NULL;
  nr_synthetics_t* synthetics
      = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL synthetics", nr_synthetics_outbound_header(NULL));

  /*
   * Test : Good parameters.
   */
  header = nr_synthetics_outbound_header(synthetics);
  tlib_pass_if_str_equal("header", "[1,100,\"a\",\"b\",\"c\"]", header);
  tlib_pass_if_ptr_equal("header is cached", header,
                         nr_synthetics_outbound_header(synthetics));

  nr_synthetics_destroy(&synthetics);
}

#define TEST_SYNTHETICS_TEST_FILE \
  CROSS_AGENT_TESTS_DIR "/synthetics/synthetics.json"

static void test_synthetics_cross_agent_tests(void) {
  char* file_json;
  nrobj_t* arr;
  int i;

  file_json
      = nr_read_file_contents(TEST_SYNTHETICS_TEST_FILE, 10 * 1000 * 1000);
  tlib_pass_if_not_null("tests valid", file_json);

  if (NULL == file_json) {
    return;
  }
  arr = nro_create_from_json(file_json);
  nr_free(file_json);
  tlib_pass_if_not_null("tests valid", arr);
  if (NULL == arr) {
    return;
  }

  tlib_pass_if_int_equal("tests valid", nro_getsize(arr), 7);
  for (i = 1; i <= nro_getsize(arr); i++) {
    nr_status_t st;
    nrtxn_t* txn;
    const nrobj_t* hash = nro_get_array_hash(arr, i, 0);
    const char* testname = nro_get_hash_string(hash, "name", 0);
    const nrobj_t* settings = nro_get_hash_hash(hash, "settings", NULL);
    const nrobj_t* input_obfuscated_header
        = nro_get_hash_hash(hash, "inputObfuscatedHeader", NULL);
    const char* x_newrelic_synthetics = nro_get_hash_string(
        input_obfuscated_header, "X-NewRelic-Synthetics", NULL);
    const nrobj_t* input_header_payload
        = nro_get_hash_array(hash, "inputHeaderPayload", NULL);
    const nrobj_t* output_txn_trace
        = nro_get_hash_hash(hash, "outputTransactionTrace", NULL);
    const nrobj_t* output_txn_event
        = nro_get_hash_hash(hash, "outputTransactionEvent", NULL);
    const nrobj_t* output_request_header
        = nro_get_hash_hash(hash, "outputExternalRequestHeader", NULL);
    const char* synthetics_encoding_key
        = nro_get_hash_string(settings, "syntheticsEncodingKey", NULL);

    const char* expected_output_header = nro_get_hash_string(
        nro_get_hash_hash(
            nro_get_hash_hash(hash, "outputExternalRequestHeader", NULL),
            "expectedHeader", NULL),
        "X-NewRelic-Synthetics", NULL);

    txn = (nrtxn_t*)nr_zalloc(sizeof(*txn));
    txn->options.synthetics_enabled = 1;

    tlib_pass_if_not_null(testname, hash);
    tlib_pass_if_not_null(testname, testname);
    tlib_pass_if_not_null(testname, settings);
    tlib_pass_if_not_null(testname, input_header_payload);
    tlib_pass_if_not_null(testname, input_obfuscated_header);
    tlib_pass_if_not_null(testname, output_txn_trace);
    tlib_pass_if_not_null(testname, output_txn_event);
    tlib_pass_if_not_null(testname, output_request_header);
    tlib_pass_if_not_null(testname, synthetics_encoding_key);

    nr_txn_set_guid(txn,
                    nro_get_hash_string(settings, "transactionGuid", NULL));
    txn->app_connect_reply = nro_new_hash();
    nro_set_hash_string(
        txn->app_connect_reply, "encoding_key",
        nro_get_hash_string(settings, "agentEncodingKey", NULL));
    nro_set_hash(txn->app_connect_reply, "trusted_account_ids",
                 nro_get_hash_array(settings, "trustedAccountIds", NULL));

    if (x_newrelic_synthetics) {
      char* json = nro_to_json(input_header_payload);
      char* obfuscated_input_payload
          = nr_obfuscate(json, synthetics_encoding_key, 0);

      tlib_pass_if_str_equal(testname, obfuscated_input_payload,
                             x_newrelic_synthetics);
      nr_free(json);
      nr_free(obfuscated_input_payload);
    }

    st = nr_header_set_synthetics_txn(txn, x_newrelic_synthetics);

    if (NULL == expected_output_header) {
      /*
       * Failure Expected
       */
      tlib_pass_if_status_failure(testname, st);
      tlib_pass_if_null(testname, txn->synthetics);
    } else {
      char* outbound = nr_header_outbound_request_synthetics_encoded(txn);
      /*
       * Success Expected
       */
      tlib_pass_if_status_success(testname, st);
      tlib_pass_if_not_null(testname, txn->synthetics);
      tlib_pass_if_str_equal(testname, expected_output_header, outbound);
      nr_free(outbound);
    }

    nr_txn_destroy(&txn);
  }

  nro_delete(arr);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_create();
  test_parse_v1();
  test_destroy();
  test_version();
  test_account_id();
  test_resource_id();
  test_job_id();
  test_monitor_id();
  test_outbound_header();
  test_synthetics_cross_agent_tests();
}
