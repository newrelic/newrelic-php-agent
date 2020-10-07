/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <errno.h>
#include <glob.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_rum.h"
#include "nr_rum_private.h"
#include "nr_txn.h"
#include "util_errno.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

static void test_do_autorum(void) {
  int do_autorum;
  nrtxn_t txn;

  txn.status.background = 0;
  txn.options.autorum_enabled = 1;

  do_autorum = nr_rum_do_autorum(0);
  tlib_pass_if_true("null txn", 0 == do_autorum, "do_autorum=%d", do_autorum);

  txn.status.background = 1;
  do_autorum = nr_rum_do_autorum(&txn);
  tlib_pass_if_true("background txn", 0 == do_autorum, "do_autorum=%d",
                    do_autorum);
  txn.status.background = 0;

  txn.options.autorum_enabled = 0;
  do_autorum = nr_rum_do_autorum(&txn);
  tlib_pass_if_true("autorum disabled", 0 == do_autorum, "do_autorum=%d",
                    do_autorum);
  txn.options.autorum_enabled = 1;

  do_autorum = nr_rum_do_autorum(&txn);
  tlib_pass_if_true("success", 1 == do_autorum, "do_autorum=%d", do_autorum);
}

static void test_produce_header_bad_params(void) {
  char* hdr;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrobj_t* app_connect_reply
      = nro_create_from_json("{\"js_agent_loader\":\"LOADER\"}");
  int tags = 1;
  int autorum = 1;

  txn->status.ignore = 0;
  txn->options.autorum_enabled = 1;
  txn->status.rum_header = 0;
  txn->app_connect_reply = app_connect_reply;

  txn->status.ignore = 1;
  hdr = nr_rum_produce_header(txn, tags, autorum);
  tlib_pass_if_true("Header: Ignore txn", 0 == hdr, "hdr=%p", hdr);
  tlib_pass_if_true("Header: Ignore txn", 0 == txn->status.rum_header,
                    "status.rum_header=%d", txn->status.rum_header);
  txn->status.ignore = 0;

  txn->options.autorum_enabled = 0;
  hdr = nr_rum_produce_header(txn, tags, autorum);
  tlib_pass_if_true("Header: Auto-RUM disabled", 0 == hdr, "hdr=%p", hdr);
  tlib_pass_if_true("Header: Auto-RUM disabled", 0 == txn->status.rum_header,
                    "status.rum_header=%d", txn->status.rum_header);
  txn->options.autorum_enabled = 1;

  hdr = nr_rum_produce_header(0, tags, autorum);
  tlib_pass_if_true("Header: Null txn", 0 == hdr, "hdr=%p", hdr);

  txn->status.rum_header = 1;
  hdr = nr_rum_produce_header(txn, tags, autorum);
  tlib_pass_if_true("Header: Previous Header", 0 == hdr, "hdr=%p", hdr);
  tlib_pass_if_true("Header: Previous Header", 1 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  txn->status.rum_header = 0;

  txn->app_connect_reply = 0;
  hdr = nr_rum_produce_header(txn, tags, autorum);
  tlib_pass_if_true("Header: No loader", 0 == hdr, "hdr=%p", hdr);
  tlib_pass_if_true("Header: No loader", 0 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  txn->app_connect_reply = app_connect_reply;

  /* Verify that the inputs above were good when not altered. */
  hdr = nr_rum_produce_header(txn, tags, autorum);
  tlib_pass_if_true("Header: Good inputs", 0 != hdr, "hdr=%p", hdr);
  tlib_pass_if_true("Header: Good inputs", txn->status.rum_header > 1,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  nr_free(hdr);

  nro_delete(app_connect_reply);
}

static void test_produce_header(void) {
  char* hdr;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrobj_t* app_connect_reply
      = nro_create_from_json("{\"js_agent_loader\":\"LOADER\"}");

  txn->status.ignore = 0;
  txn->options.autorum_enabled = 1;
  txn->status.rum_header = 0;
  txn->app_connect_reply = app_connect_reply;

  txn->status.rum_header = 0;
  hdr = nr_rum_produce_header(txn, 1, 0);
  tlib_pass_if_true(
      "Header: manual tags",
      0 == nr_strcmp("<script type=\"text/javascript\">LOADER</script>", hdr),
      "hdr=%s", NRSAFESTR(hdr));
  tlib_pass_if_true("Header: manual tags", 1 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  nr_free(hdr);

  txn->options.autorum_enabled = 0;
  txn->status.rum_header = 0;
  hdr = nr_rum_produce_header(txn, 1, 0);
  tlib_pass_if_true(
      "Header: autorum disabled manual tags",
      0 == nr_strcmp("<script type=\"text/javascript\">LOADER</script>", hdr),
      "hdr=%s", NRSAFESTR(hdr));
  tlib_pass_if_true("Header: autorum disabled manual tags",
                    1 == txn->status.rum_header, "txn->status.rum_header=%d",
                    txn->status.rum_header);
  nr_free(hdr);
  txn->options.autorum_enabled = 1;

  txn->status.rum_header = 0;
  hdr = nr_rum_produce_header(txn, 0, 0);
  tlib_pass_if_true("Header: manual no tags", 0 == nr_strcmp("LOADER", hdr),
                    "hdr=%s", NRSAFESTR(hdr));
  tlib_pass_if_true("Header: manual no tags", 1 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  nr_free(hdr);

  txn->status.rum_header = 0;
  hdr = nr_rum_produce_header(txn, 0, 1);
  tlib_pass_if_true("Header: autorum no tags", 0 == nr_strcmp("LOADER", hdr),
                    "hdr=%s", NRSAFESTR(hdr));
  tlib_pass_if_true("Header: autorum no tags", 2 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  nr_free(hdr);

  txn->status.rum_header = 0;
  hdr = nr_rum_produce_header(txn, 1, 1);
  tlib_pass_if_true(
      "Header: autorum tags",
      0 == nr_strcmp("<script type=\"text/javascript\">LOADER</script>", hdr),
      "hdr=%s", NRSAFESTR(hdr));
  tlib_pass_if_true("Header: autorum tags", 2 == txn->status.rum_header,
                    "txn->status.rum_header=%d", txn->status.rum_header);
  nr_free(hdr);

  nro_delete(app_connect_reply);
}

typedef struct _rum_mock_txn_t {
  nrtxn_t txn;
  nr_status_t fake_freeze_name_return;
  nrtime_t fake_queue_time;
  nrtime_t unfinished_duration;
} rum_mock_txn_t;

nr_status_t nr_txn_freeze_name_update_apdex(nrtxn_t* txn) {
  return ((const rum_mock_txn_t*)txn)->fake_freeze_name_return;
}

nrtime_t nr_txn_queue_time(const nrtxn_t* txn) {
  return ((const rum_mock_txn_t*)txn)->fake_queue_time;
}

nrtime_t nr_txn_unfinished_duration(const nrtxn_t* txn) {
  return ((const rum_mock_txn_t*)txn)->unfinished_duration;
}

static void test_produce_footer_bad_params(void) {
  char* ftr;
  rum_mock_txn_t tnn;
  nrtxn_t* txn = &tnn.txn;
  int tags = 1;
  int autorum = 1;

  tnn.fake_queue_time = 3 * NR_TIME_DIVISOR_MS;
  tnn.unfinished_duration = 5 * NR_TIME_DIVISOR_MS;

  txn->status.ignore = 0;
  txn->options.autorum_enabled = 1;
  txn->status.rum_header = 1;
  txn->status.rum_footer = 0;
  tnn.fake_freeze_name_return = NR_SUCCESS;

  txn->attributes = 0;
  txn->license = nr_strdup("0123456789abcdefghijklmnopqrstuvwxyz1234");
  txn->app_connect_reply = 0;
  txn->name = nr_strdup("WebTransaction/brink/of/glory");

  txn->options.tt_threshold = 1 * NR_TIME_DIVISOR_MS;

  ftr = nr_rum_produce_footer(0, tags, autorum);
  tlib_pass_if_true("null txn", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("null txn", 0 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);

  txn->status.ignore = 1;
  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("ignore txn", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("ignore txn", 0 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);
  txn->status.ignore = 0;

  txn->options.autorum_enabled = 0;
  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("autorum disabled", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("autorum disabled", 0 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);
  txn->options.autorum_enabled = 1;

  txn->status.rum_header = 0;
  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("header not produced", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("header not produced", 0 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);
  txn->status.rum_header = 1;

  txn->status.rum_footer = 1;
  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("footer already produced", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("footer already produced", 1 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);
  txn->status.rum_footer = 0;

  tnn.fake_freeze_name_return = NR_FAILURE;
  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("freeze name failure", 0 == ftr, "ftr=%p", ftr);
  tlib_pass_if_true("freeze name failure", 0 == txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);
  tnn.fake_freeze_name_return = NR_SUCCESS;

  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("tests are valid", 0 != ftr, "ftr=%p", ftr);
  tlib_pass_if_true("tests are valid", 0 != txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);
  nr_free(ftr);

  nr_free(txn->license);
  nr_free(txn->name);
}

static nr_status_t obj_to_attributes_iter(const char* key,
                                          const nrobj_t* val,
                                          void* ptr) {
  nr_attributes_t* attributes = (nr_attributes_t*)ptr;

  nr_attributes_user_add_string(attributes, NR_ATTRIBUTE_DESTINATION_BROWSER,
                                key, nro_get_string(val, 0));

  return NR_SUCCESS;
}

static void test_produce_footer_testcases(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;

#define TEST_RUM_FOOTER_TEST_FILE \
  CROSS_AGENT_TESTS_DIR "/rum_client_config.json"
  json = nr_read_file_contents(TEST_RUM_FOOTER_TEST_FILE, 10 * 1000 * 1000);
  tlib_pass_if_true("tests valid", 0 != json, "json=%p", json);

  if (0 == json) {
    return;
  }

  array = nro_create_from_json(json);
  tlib_pass_if_true("tests valid", 0 != array, "array=%p", array);
  otype = nro_type(array);
  tlib_pass_if_true("tests valid", NR_OBJECT_ARRAY == otype, "otype=%d",
                    (int)otype);

  if (array && (NR_OBJECT_ARRAY == nro_type(array))) {
    for (i = 1; i <= nro_getsize(array); i++) {
      const nrobj_t* hash = nro_get_array_hash(array, i, 0);
      const char* testname = nro_get_hash_string(hash, "testname", 0);
      nrtime_t apptime_milliseconds
          = (nrtime_t)nro_get_hash_int(hash, "apptime_milliseconds", 0);
      nrtime_t queuetime_milliseconds
          = (nrtime_t)nro_get_hash_int(hash, "queuetime_milliseconds", 0);
      nrtime_t trace_threshold_microseconds
          = (nrtime_t)nro_get_hash_int(hash, "trace_threshold_microseconds", 0);
      int browser_monitoring_attributes_enabled = nro_get_hash_boolean(
          hash, "browser_monitoring.attributes.enabled", 0);
      const char* txnname = nro_get_hash_string(hash, "transaction_name", 0);
      const char* license_key = nro_get_hash_string(hash, "license_key", 0);
      const nrobj_t* connect_reply
          = nro_get_hash_hash(hash, "connect_reply", 0);
      const nrobj_t* user_attributes
          = nro_get_hash_hash(hash, "user_attributes", 0);
      const nrobj_t* expected = nro_get_hash_hash(hash, "expected", 0);

      tlib_pass_if_true("tests valid", 0 != hash, "hash=%p", hash);
      if (hash) {
        rum_mock_txn_t mock;
        nrtxn_t* txn = &mock.txn;
        char* actual;
        int tags = 0;
        int autorum = 0;
        nr_attributes_t* attributes;
        nr_attribute_config_t* config = nr_attribute_config_create();

        if (0 == browser_monitoring_attributes_enabled) {
          nr_attribute_config_disable_destinations(
              config, NR_ATTRIBUTE_DESTINATION_BROWSER);
        }

        attributes = nr_attributes_create(config);
        nr_attribute_config_destroy(&config);

        txn->options.autorum_enabled = 1;
        mock.fake_queue_time = NR_TIME_DIVISOR_MS * queuetime_milliseconds;
        mock.unfinished_duration = NR_TIME_DIVISOR_MS * apptime_milliseconds;
        txn->options.tt_threshold
            = NR_TIME_DIVISOR_MS * (trace_threshold_microseconds);
        txn->status.ignore = 0;
        txn->status.rum_header = 1;
        txn->status.rum_footer = 0;
        mock.fake_freeze_name_return = NR_SUCCESS;
        txn->attributes = attributes;

        nro_iteratehash(user_attributes, obj_to_attributes_iter, attributes);
        txn->app_connect_reply = nro_copy(connect_reply);
        txn->license = nr_strdup(license_key);
        txn->name = nr_strdup(txnname);

        actual = nr_rum_produce_footer(txn, tags, autorum);

        if (expected) {
          int j = 0;
          char* expected_json = nro_to_json(expected);

          /* Remove the prefix so as to only compare the config hash */
          if (actual) {
            for (j = 0; nr_rum_footer_prefix[j] && actual[j]; j++) {
              if (nr_rum_footer_prefix[j] == actual[j]) {
                continue;
              } else {
                break;
              }
            }
          }
          /*
           * This comparison assumes that the hash fields are in the
           * same order.
           */
          tlib_pass_if_true(testname ? testname : "unknown",
                            0 == nr_strcmp(actual + j, expected_json),
                            "j=%d"
                            "\n"
                            ">  actual_json=%s"
                            "\n"
                            ">expected_json=%s",
                            j, NRSAFESTR(actual + j), NRSAFESTR(expected_json));
          nr_free(expected_json);
        } else {
          tlib_pass_if_true(testname ? testname : "unknown", 0 == actual,
                            "actual=%p", actual);
        }

        nr_free(actual);
        nr_free(txn->license);
        nr_free(txn->name);
        nro_delete(txn->app_connect_reply);
        nr_attributes_destroy(&attributes);
      }
    }
  }

  nro_delete(array);
  nr_free(json);
}

static void test_produce_footer_all_fields(void) {
  char* ftr;
  rum_mock_txn_t tnn;
  nrtxn_t* txn = &tnn.txn;
  int tags = 1;
  int autorum = 1;

  tnn.fake_queue_time = 3 * NR_TIME_DIVISOR_MS;
  tnn.unfinished_duration = 5 * NR_TIME_DIVISOR_MS;

  txn->status.ignore = 0;
  txn->options.autorum_enabled = 1;
  txn->status.rum_header = 1;
  txn->status.rum_footer = 0;
  tnn.fake_freeze_name_return = NR_SUCCESS;

  txn->attributes = nr_attributes_create(0);
  nr_attributes_user_add_string(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_BROWSER, "user", "my/user");
  nr_attributes_user_add_string(txn->attributes,
                                NR_ATTRIBUTE_DESTINATION_BROWSER, "account",
                                "my/account");
  nr_attributes_user_add_string(txn->attributes,
                                NR_ATTRIBUTE_DESTINATION_BROWSER, "product",
                                "my/product");
  nr_attributes_agent_add_string(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_BROWSER, "zip", "zap");
  txn->license = nr_strdup("0123456789abcdefghijklmnopqrstuvwxyz1234");
  txn->app_connect_reply = nro_new_hash();
  nro_set_hash_string(txn->app_connect_reply, "beacon", "my_beacon");
  nro_set_hash_string(txn->app_connect_reply, "browser_key", "my_browser_key");
  nro_set_hash_string(txn->app_connect_reply, "application_id",
                      "my_application_id");
  nro_set_hash_string(txn->app_connect_reply, "error_beacon",
                      "my_error_beacon");
  nro_set_hash_string(txn->app_connect_reply, "js_agent_file",
                      "my_js_agent_file");

  txn->name = nr_strdup("WebTransaction/brink/of/glory");

  txn->options.tt_threshold = 1 * NR_TIME_DIVISOR_MS;

  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true(
      "footer with all fields",
      0
          == nr_strcmp(ftr,
                       "<script type=\"text/javascript\">"
                       "window.NREUM||(NREUM={});NREUM.info="
                       "{"
                       "\"beacon\":\"my_beacon\","
                       "\"licenseKey\":\"my_browser_key\","
                       "\"applicationID\":\"my_application_id\","
                       "\"transactionName\":"
                       "\"Z1RQZ0ZUWERZWhULDF4eUEFdW10YV19OBQ9fQ0s=\","
                       "\"queueTime\":3,"
                       "\"applicationTime\":5,"
                       "\"atts\":"
                       "\"SxNHEQ5OFEdKVgUXAEQTCBFZTGoYSEsOBhZTRRAfFlRVVFdMDxZB"
		       "ChNfSmgaV1RbVhQMFxIdEEZHUEQVAhsMGz8fREFWRhdLGxpYQ1gYEk"
		       "tbQxYPFE1ZSUMfHg==\","
                       "\"errorBeacon\":\"my_error_beacon\","
                       "\"agent\":\"my_js_agent_file\""
                       "}"
                       "</script>"),
      "ftr=%s", ftr);
  tlib_pass_if_true("footer with all fields", 0 != txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);

  nr_free(ftr);

  nr_attributes_destroy(&txn->attributes);
  nro_delete(txn->app_connect_reply);
  nr_free(txn->license);
  nr_free(txn->name);
}

static void test_produce_footer_no_fields(void) {
  char* ftr;
  rum_mock_txn_t tnn;
  nrtxn_t* txn = &tnn.txn;
  int tags = 1;
  int autorum = 1;

  /* Reverse chronological order */
  tnn.fake_queue_time = 0;
  tnn.unfinished_duration = 0;

  txn->status.ignore = 0;
  txn->options.autorum_enabled = 1;
  txn->status.rum_header = 1;
  txn->status.rum_footer = 0;
  tnn.fake_freeze_name_return = NR_SUCCESS;

  txn->attributes = 0;
  txn->app_connect_reply = 0;
  txn->name = 0;

  txn->options.tt_threshold = 1 * NR_TIME_DIVISOR_MS;

  ftr = nr_rum_produce_footer(txn, tags, autorum);
  tlib_pass_if_true("footer with no fields",
                    0
                        == nr_strcmp(ftr,
                                     "<script type=\"text/javascript\">"
                                     "window.NREUM||(NREUM={});NREUM.info="
                                     "{"
                                     "\"beacon\":\"\","
                                     "\"licenseKey\":\"\","
                                     "\"applicationID\":\"\","
                                     "\"transactionName\":\"\","
                                     "\"queueTime\":0,"
                                     "\"applicationTime\":0,"
                                     "\"atts\":\"\","
                                     "\"errorBeacon\":\"\","
                                     "\"agent\":\"\""
                                     "}"
                                     "</script>"),
                    "ftr=%s", ftr);
  tlib_pass_if_true("footer with no fields", 0 != txn->status.rum_footer,
                    "txn->status.rum_footer=%d", txn->status.rum_footer);

  nr_free(ftr);

  nro_delete(txn->app_connect_reply);
}

static void test_get_attributes(void) {
  nr_attributes_t* user;
  nr_attributes_t* agent;
  nr_attributes_t* user_and_agent;
  nr_attributes_t* empty;
  char* json;

  user = nr_attributes_create(0);
  agent = nr_attributes_create(0);
  user_and_agent = nr_attributes_create(0);
  empty = nr_attributes_create(0);

  nr_attributes_user_add_string(user, NR_ATTRIBUTE_DESTINATION_BROWSER,
                                "im_user", "zap");
  nr_attributes_user_add_string(
      user_and_agent, NR_ATTRIBUTE_DESTINATION_BROWSER, "im_user", "zap");

  nr_attributes_agent_add_string(agent, NR_ATTRIBUTE_DESTINATION_BROWSER,
                                 "im_agent", "zup");
  nr_attributes_agent_add_string(
      user_and_agent, NR_ATTRIBUTE_DESTINATION_BROWSER, "im_agent", "zup");

  json = nr_rum_get_attributes(0);
  tlib_pass_if_true("null attributes", 0 == json, "json=%p", json);

  json = nr_rum_get_attributes(empty);
  tlib_pass_if_true("empty attributes", 0 == json, "json=%p", json);

  json = nr_rum_get_attributes(user);
  tlib_pass_if_true("user",
                    0 == nr_strcmp("{\"u\":{\"im_user\":\"zap\"}}", json),
                    "json=%s", NRSAFESTR(json));
  nr_free(json);

  json = nr_rum_get_attributes(agent);
  tlib_pass_if_true("agent",
                    0 == nr_strcmp("{\"a\":{\"im_agent\":\"zup\"}}", json),
                    "json=%s", NRSAFESTR(json));
  nr_free(json);

  json = nr_rum_get_attributes(user_and_agent);
  tlib_pass_if_true(
      "user_and_agent",
      0
          == nr_strcmp(
                 "{\"u\":{\"im_user\":\"zap\"},\"a\":{\"im_agent\":\"zup\"}}",
                 json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_attributes_destroy(&user);
  nr_attributes_destroy(&agent);
  nr_attributes_destroy(&user_and_agent);
  nr_attributes_destroy(&empty);
}

static char* remove_substring(const char* str, const char* substring) {
  const char* begin;
  const char* end;
  char* new_str;

  begin = nr_strstr(str, substring);
  if (0 == begin) {
    return 0;
  }

  end = begin + nr_strlen(substring);
  new_str = (char*)nr_malloc(nr_strlen(str) + 1);
  new_str[0] = '\0';
  nr_strncat(new_str, str, begin - str);
  nr_strcat(new_str, end);

  return new_str;
}

static char* insert_substring_at_index(const char* str,
                                       const char* substring,
                                       int idx) {
  int len = nr_strlen(str) + nr_strlen(substring);
  char* s = (char*)nr_malloc(len + 1);

  s[0] = '\0';
  snprintf(s, len + 1, "%.*s%s%s", idx, str, substring, str + idx);

  return s;
}

#define test_scan_html_predicate(N, M, H)                                  \
  test_scan_html_predicate_f(N, M, H, nr_rum_scan_html_for_head, __FILE__, \
                             __LINE__)

static void test_scan_html_predicate_f(
    const char* name,
    const char* marker,
    const char* html_with_marker,
    const char* (*scan_html)(const char* input, const uint input_len),
    const char* file,
    int line) {
  int actual_offset = 0;
  int expected_offset = 0;
  char* html = 0;
  char* html_diff = 0;
  const char* actual_location = 0;
  const char* marker_location = 0;

  marker_location = nr_strstr(html_with_marker, marker);

  /*
   * If the marker is not present, no insertion should be performed.
   */
  if (0 == marker_location) {
    actual_location = scan_html(html_with_marker, nr_strlen(html_with_marker));
    test_pass_if_true(name, 0 == actual_location,
                      "(int)(actual_location - html_with_marker)=%d",
                      (int)(actual_location - html_with_marker));
    return;
  }

  /*
   * If a marker is present, RUM insertion should be performed. Remove
   * the marker and re-scan the html. Validate that insertion was performed
   * and the insertion point is at the same position as the marker.
   */
  html = remove_substring(html_with_marker, marker);
  actual_location = scan_html(html, nr_strlen(html));

  test_pass_if_true(name, 0 != actual_location, "insertion location not found");

  if (0 == actual_location) {
    nr_free(html);
    return;
  }

  expected_offset = marker_location - html_with_marker;
  actual_offset = actual_location - html;

  if (actual_offset < expected_offset) {
    /* Insertion occurred to before the marker. */
    char* tmp
        = insert_substring_at_index(html, "EXPECTED_HERE", expected_offset);

    html_diff = insert_substring_at_index(tmp, "FOUND_HERE", actual_offset);
    nr_free(tmp);
  } else if (actual_offset > expected_offset) {
    /* Insertion occurred after the marker. */
    char* tmp = insert_substring_at_index(html, "FOUND_HERE", actual_offset);

    html_diff
        = insert_substring_at_index(tmp, "EXPECTED_HERE", expected_offset);
    nr_free(tmp);
  }

  test_pass_if_true(
      name, actual_offset == expected_offset,
      "RUM insertion occured at the wrong offset: expected=%d, actual=%d\n%s",
      expected_offset, actual_offset, html_diff);

  nr_free(html_diff);
  nr_free(html);
}

/*
 * These tests show the behavior of the header scanning logic
 * on faulty and miscellaneous html fragments.  Note that the spec
 * does not properly handle situations involving comments and html embedded
 * within strings.
 */
static void test_scan_html(void) {
  test_scan_html_predicate("head null 0", "%HERE%", 0);
  test_scan_html_predicate("head null 1", "%HERE%", "");

  test_scan_html_predicate("head missing 0", "%HERE%", "foobar");
  test_scan_html_predicate("head missing 1", "%HERE%", "<html> foobar </html>");
  test_scan_html_predicate(
      "head missing 2", "%HERE%",
      "</head> foobat <head>%HERE%foobar"); /* that's right, at end of <head>
                                               tag */
  test_scan_html_predicate(
      "head missing 3", "%HERE%",
      "<html><head>%HERE% foobat foobar</html>"); /* hmm: does not have an
                                                     ending </head > */

  test_scan_html_predicate(
      "head basic 0", "%HERE%",
      "<head>%HERE% foobar </head></html>"); /* trailing space important */
  test_scan_html_predicate(
      "head basic 1", "%HERE%",
      "<html><head>%HERE% foobar </head></html>"); /* trailing space important
                                                    */
  test_scan_html_predicate("head basic 2", "%HERE%",
                           "<html><head>%HERE% foobar</head></html>");
  test_scan_html_predicate("head basic 2", "%HERE%",
                           "<html><head>%HERE% foobar foobat </head></html>");
  test_scan_html_predicate("head basic 3", "%HERE%",
                           "<head>%HERE% foobar</head></html>");

  test_scan_html_predicate(
      "head comment 0", "%HERE%",
      "<!-- comment start --><html><head>%HERE% foobar</head></html>");
  test_scan_html_predicate(
      "head comment 1", "%HERE%",
      "<html><!-- comment XX --> <head>%HERE% foobar</head></html>");
  test_scan_html_predicate(
      "head comment 2", "%HERE%",
      "<html><head>%HERE% <!-- comment XX -->foobar</head></html>");
  test_scan_html_predicate(
      "head comment 3", "%HERE%",
      "<html><head>%HERE% foobat<!-- comment XX -->foobar</head></html>");
  test_scan_html_predicate(
      "head comment 4", "%HERE%",
      "<html><head>%HERE% foobar</head><!-- comment XX --></html>");
  test_scan_html_predicate(
      "head comment 5", "%HERE%",
      "<html><head>%HERE% foobar</head></html><!-- comment XX -->");

  test_scan_html_predicate(
      "head comment 6", "%HERE%",
      "<html><!-- <head>%HERE% foobat</head> --> <head> foobar</head> </html>");
  test_scan_html_predicate("head comment 7", "%HERE%",
                           "<!--<html><head>%HERE% foobat</head> --> "
                           "<html><head> foobar</head> </html>");
  test_scan_html_predicate("head comment 8", "%HERE%",
                           "<!--<html><head>%HERE% foobat</head></html> "
                           "-->foobar"); /* no head to be found, so 0 return */

  test_scan_html_predicate("head mangled comment 0", "%HERE%",
                           "<!-><html><head>%HERE% foobat</head></html>foobar");
  test_scan_html_predicate(
      "head mangled comment 1", "%HERE%",
      "<!- --><html><head>%HERE% foobat</head></html>foobar");
  test_scan_html_predicate("head mangled comment 2", "%HERE%",
                           "--><html><head>%HERE% foobat</head></html>foobar");
  test_scan_html_predicate(
      "head mangled comment 3", "%HERE%",
      "<!-- -><html><head>%HERE% foobat</head></html>foobar"); /* unclosed
                                                                  comment */
  test_scan_html_predicate(
      "head mangled comment 4", "%HERE%",
      "<!X -><html><head>%HERE% foobat</head></html>foobar");
  test_scan_html_predicate(
      "head mangled comment 5", "%HERE%",
      "<!-- --><html><head>%HERE% foobat</head></html>foobar");

  test_scan_html_predicate("head mangled comment 6", "%HERE%",
                           "<html><head>%HERE% foobat</head></html>foobar");
  test_scan_html_predicate("head mangled comment 6", "%HERE%",
                           "< !-- -><html><head>%HERE% "
                           "foobat</head></html>foobar"); /* BUG: should be
                                                             "foobat"; that
                                                             really is not a
                                                             comment */

  test_scan_html_predicate(
      "head mangled comment 7", "%HERE%",
      "<!-- --X<html><head>%HERE% foobat</head></html>foobar"); /* unclosed
                                                                   comment */
  test_scan_html_predicate(
      "head mangled comment 8", "%HERE%",
      "<!-- -><html><head>%HERE% foobat</head></html>foobar"); /* unclosed
                                                                  comment */

  test_scan_html_predicate("inhead mangled comment 1", "%HERE%",
                           "<html><head>%HERE% blivet<!  comment XX "
                           "-->foobar</head></html>"); /* stops before unopened
                                                          comment */
  test_scan_html_predicate("inhead mangled comment 2", "%HERE%",
                           "<html><head>%HERE% blivet<!- comment XX "
                           "-->foobar</head></html>"); /* stops before unopened
                                                          comment */
  test_scan_html_predicate("inhead mangled comment 3", "%HERE%",
                           "<html><head>%HERE% blivet<!-X comment XX "
                           "-->foobar</head></html>"); /* stops before unopened
                                                          comment */
  test_scan_html_predicate("inhead mangled comment 4", "%HERE%",
                           "<html><head>%HERE%blavet<!-- comment XX  "
                           "->foobar</head></html>blivet"); /* unclosed comment,
                                                               but still in head
                                                             */
  test_scan_html_predicate("inhead mangled comment 5", "%HERE%",
                           "<html><head>%HERE%blavet<!-- comment XX  "
                           "-X>foobar</head></html>blivet"); /* unclosed
                                                                comment, but
                                                                still in head */
  test_scan_html_predicate("inhead mangled comment 6", "%HERE%",
                           "<html><head>%HERE%blavet <!-- comment XX  "
                           "X->foobar</head></html>"); /* unclosed comment */
  test_scan_html_predicate("inhead mangled comment 7", "%HERE%",
                           "<html><head>%HERE%blavet <!-- comment XX -- "
                           "foobar</head></html>"); /* unclosed comment */

  test_scan_html_predicate(
      "head meta 1", "%HERE%",
      "<html><head>%HERE%<meta name=\"foobat\"> foobar</head></html>");
  test_scan_html_predicate("head meta 1", "%HERE%",
                           "<html><head>%HERE%<meta name=\"foobat\"><meta "
                           "name=\"blivet\">foobar</head></html>");
  test_scan_html_predicate("unterminated head", "%HERE%",
                           "<html><head alpha=\"beta\"");
  test_scan_html_predicate("head with attribute", "%HERE%",
                           "<html><head alpha=\"beta\">%HERE%");
  test_scan_html_predicate(
      "close head > within string", "%HERE%",
      "<html><head alpha=\">%HERE%\">"); /* Expected but not optimal */

  test_scan_html_predicate("headline tag", "%HERE%", "<html><headline>");
  test_scan_html_predicate("bodysomething tag", "%HERE%",
                           "<html><bodysomething>");
}

#define cross_agent_header_testcase(N)                            \
  cross_agent_rum_testcase_f((N), "EXPECTED_RUM_LOADER_LOCATION", \
                             nr_rum_scan_html_for_head, __FILE__, __LINE__)

#define cross_agent_footer_testcase(N)                            \
  cross_agent_rum_testcase_f((N), "EXPECTED_RUM_FOOTER_LOCATION", \
                             nr_rum_scan_html_for_foot, __FILE__, __LINE__)

static void cross_agent_rum_testcase_f(
    const char* filename,
    const char* marker,
    const char* (*scan_html)(const char* input, const uint input_len),
    const char* file,
    int line) {
  char* text;

  text = nr_read_file_contents(filename, 10 * 1000 * 1000);
  test_scan_html_predicate_f(filename, marker, text, scan_html, file, line);
  nr_free(text);
}

static void test_scan_html_for_head_from_cross_agent_tests(void) {
  int rv = 0;
  char pattern[512];
  size_t i = 0;
  glob_t glob_buf;

  nr_memset(&glob_buf, 0, sizeof(glob_buf));
  pattern[0] = '\0';
  snprintf(pattern, sizeof(pattern), "%s/rum_loader_insertion_location/*.html",
           CROSS_AGENT_TESTS_DIR);
  rv = glob(pattern, 0, NULL, &glob_buf);
  tlib_pass_if_true(
      "cross agent header insertion", (0 == rv) || (GLOB_NOMATCH == rv),
      "failed to glob test files: errno=%s, glob=%s", nr_errno(errno), pattern);

  for (i = 0; i < glob_buf.gl_pathc; ++i) {
    cross_agent_header_testcase(glob_buf.gl_pathv[i]);
  }

  globfree(&glob_buf);
}

static char* nr_malloc_wrapper(int size) {
  return (char*)nr_malloc(size);
}

static char* produce_header_HEAD(nrtxn_t* txn NRUNUSED,
                                 int tags NRUNUSED,
                                 int autorum NRUNUSED) {
  return nr_strdup("HEAD");
}

static char* produce_header_null(nrtxn_t* txn NRUNUSED,
                                 int tags NRUNUSED,
                                 int autorum NRUNUSED) {
  return 0;
}

static char* produce_footer_TAIL(nrtxn_t* txn NRUNUSED,
                                 int tags NRUNUSED,
                                 int autorum NRUNUSED) {
  return nr_strdup("TAIL");
}

static char* produce_footer_null(nrtxn_t* txn NRUNUSED,
                                 int tags NRUNUSED,
                                 int autorum NRUNUSED) {
  return 0;
}

#define test_rum_injection_normal(...) \
  test_rum_injection_normal_f(__VA_ARGS__, __FILE__, __LINE__)

static void test_rum_injection_normal_f(int have_header,
                                        int have_footer,
                                        const char* stimulus,
                                        const char* expect_string,
                                        const char* file,
                                        int line) {
  nr_rum_control_block_t control_block;
  nrtxn_t* txn = 0;
  char* output = 0;
  size_t output_len = 0;
  char* handled_output = 0;
  size_t handled_output_len = 0;
  int debug_autorum = 1;
  int has_response_content_length = 0;

  nr_memset(&control_block, 0, sizeof(control_block));
  control_block.malloc_worker = nr_malloc_wrapper;
  control_block.produce_header = produce_header_HEAD;
  control_block.produce_footer = produce_footer_TAIL;

  txn = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));
  txn->options.autorum_enabled = 1;
  txn->status.ignore = 0;
  txn->status.rum_header = have_header;
  txn->status.rum_footer = have_footer;

  output = nr_strdup(stimulus);
  output_len = nr_strlen(output);

  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len,
                               has_response_content_length, "text/html",
                               debug_autorum);

  if (0 == expect_string) {
    test_pass_if_true("NULL expected", 0 == handled_output, "handled_output=%p",
                      handled_output);
    test_pass_if_true("NULL expected", 0 == handled_output_len,
                      "handled_output_len=%zu", handled_output_len);
  } else {
    test_pass_if_true("correct output length",
                      (int)handled_output_len == nr_strlen(handled_output),
                      "handled_output_len=%zu nr_strlen (handled_output)=%d",
                      handled_output_len, nr_strlen(handled_output));
    test_pass_if_true("correct output",
                      0 == nr_strcmp(expect_string, handled_output),
                      "expect={%s}\n    result={%s}", NRSAFESTR(expect_string),
                      NRSAFESTR(handled_output));
  }

  nr_free(output);
  nr_free(handled_output);
  nr_free(txn);
}

static void test_rum_injection_oddball(void) {
  nr_rum_control_block_t control_block;
  nrtxn_t* txn = 0;
  char* output = 0;
  size_t output_len = 0;
  char* handled_output = 0;
  size_t handled_output_len = 0;
  int debug_autorum = 1;

  nr_memset(&control_block, 0, sizeof(control_block));
  control_block.malloc_worker = nr_malloc_wrapper;
  control_block.produce_header = produce_header_HEAD;
  control_block.produce_footer = produce_footer_TAIL;

  nr_rum_output_handler_worker(0, 0, 0, 0, &handled_output, &handled_output_len,
                               0, "text/html", debug_autorum);
  tlib_pass_if_true("null handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");

  nr_rum_output_handler_worker(&control_block, 0, 0, 0, &handled_output,
                               &handled_output_len, 0, "text/html",
                               debug_autorum);
  tlib_pass_if_true("null handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");

  txn = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));
  txn->options.autorum_enabled = 0;
  txn->status.ignore = 0;

  nr_rum_output_handler_worker(&control_block, txn, 0, 0, &handled_output,
                               &handled_output_len, 0, "text/html",
                               debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);

  nr_rum_output_handler_worker(&control_block, txn, 0, 0, &handled_output,
                               &handled_output_len, 0, "text/html",
                               debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);

  txn->options.autorum_enabled = 1;

  txn->status.ignore = 1;
  nr_rum_output_handler_worker(&control_block, txn, 0, 0, &handled_output,
                               &handled_output_len, 0, "text/html",
                               debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);
  txn->status.ignore = 0;

  nr_rum_output_handler_worker(&control_block, txn, 0, 0, &handled_output,
                               &handled_output_len, 1, "text/html",
                               debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);

  output = nr_strdup("<html><head></head><body>body</body></html>");
  output_len = nr_strlen(output);

  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len, 0, 0,
                               debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);

  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len, 0,
                               "text/klingon", debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);

  txn->status.rum_header = 1;
  txn->status.rum_footer = 1;
  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len, 0,
                               "text/html", debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);
  txn->status.rum_header = 0;
  txn->status.rum_footer = 0;

  /*
   * A null head causes no rum injection at all to be done; the handled_output
   * comes back null.
   */
  control_block.produce_header = produce_header_null;
  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len, 0,
                               "text/html", debug_autorum);
  tlib_pass_if_true("handled_output",
                    (0 == handled_output) && (0 == handled_output_len),
                    "output and output_len non zero");
  nr_free(handled_output);
  control_block.produce_header = produce_header_HEAD;

  /*
   * Note, however, that a null footer causes RUM to be injected for the head.
   */
  control_block.produce_footer = produce_footer_null;
  nr_rum_output_handler_worker(&control_block, txn, output, output_len,
                               &handled_output, &handled_output_len, 0,
                               "text/html", debug_autorum);
  tlib_pass_if_true("handled_output",
                    0 != handled_output && 0 != handled_output_len,
                    "output and output_len zero");
  tlib_pass_if_true(
      "handled_output expected",
      0
          == nr_strcmp(handled_output,
                       "<html><head>HEAD</head><body>body</body></html>"),
      "handled_output=%s", handled_output);
  control_block.produce_footer = produce_footer_TAIL;
  nr_free(handled_output);

  nr_free(output);
  nr_free(txn);
}

static void test_rum_injection(void) {
  test_rum_injection_oddball();

  test_rum_injection_normal(
      0, 0, "<html> <head> head text </head> <body> body text </body> </html>",
      "<html> <head>HEAD head text </head> <body> body text TAIL</body> "
      "</html>");

  test_rum_injection_normal(
      0, 0, "<html> <head> head text </head> <BODY> BODY text </BODY> </html>",
      "<html> <head>HEAD head text </head> <BODY> BODY text TAIL</BODY> "
      "</html>");

  test_rum_injection_normal(
      0, 0, "<html> <head> head text </head> <Body> Body text </Body> </html>",
      "<html> <head>HEAD head text </head> <Body> Body text TAIL</Body> "
      "</html>");

  test_rum_injection_normal(0, 0, "<html><head></head><body></body></html>",
                            "<html><head>HEAD</head><body>TAIL</body></html>");

  test_rum_injection_normal(1, 0, "<html><head></head><body></body></html>",
                            "<html><head></head><body>TAIL</body></html>");

  test_rum_injection_normal(0, 1, "<html><head></head><body></body></html>",
                            "<html><head>HEAD</head><body></body></html>");

  test_rum_injection_normal(1, 1, "<html><head></head><body></body></html>", 0);

  test_rum_injection_normal(0, 0, "<html>", 0);
  test_rum_injection_normal(0, 0, "<a>", 0);
  test_rum_injection_normal(0, 0, "", 0);

  /*
   * Mangled or abbreviated html
   */
  test_rum_injection_normal(0, 0, "<html></html>", 0);
  test_rum_injection_normal(0, 0, "<head></head>", "<head>HEAD</head>");
  test_rum_injection_normal(0, 0, "<body></body>", "HEAD<body>TAIL</body>");

  test_rum_injection_normal(0, 0,
                            "<html> <body> body text </body> <head> head text "
                            "</head></html>",
                            "<html> <body> body text </body> <head>HEAD head "
                            "text </head></html>"); /* perhaps this is correct?
                                                     */
}

static void test_scan_html_for_foot_bad_params(void) {
  const char* foot;

  foot = nr_rum_scan_html_for_foot(0, 0);
  tlib_pass_if_true("zero params", 0 == foot, "foot=%p", foot);

  foot = nr_rum_scan_html_for_foot(0, 10);
  tlib_pass_if_true("zero input", 0 == foot, "foot=%p", foot);

  foot = nr_rum_scan_html_for_foot("</body>", 0);
  tlib_pass_if_true("zero len", 0 == foot, "foot=%p", foot);
}

#define scan_foot_testcase(...) \
  scan_foot_testcase_f(__VA_ARGS__, __FILE__, __LINE__)

static void scan_foot_testcase_f(const char* testname,
                                 const char* html,
                                 const char* expected,
                                 const char* file,
                                 int line) {
  const char* foot;

  foot = nr_rum_scan_html_for_foot(html, nr_strlen(html));
  test_pass_if_true(testname, foot == expected, "foot=%p expected=%p", foot,
                    expected);
}

static void test_scan_html_for_foot_failure(void) {
  scan_foot_testcase("missing front angle bracket", "/body>", 0);
  scan_foot_testcase("missing end angle bracket", "</body", 0);
  scan_foot_testcase("missing brackets", "body", 0);

  scan_foot_testcase("not foot", "<html><head> foobar</head></html>", 0);
  scan_foot_testcase("not foot",
                     "<html><head> <!-- comment XX -->foobar</head></html>", 0);
  scan_foot_testcase("not foot", "--><html><head> foobat</head></html>foobar",
                     0);
  scan_foot_testcase(
      "not foot", "<html><head> blivet<!-X comment XX -->foobar</head></html>",
      0);
}

static void test_scan_html_for_foot_success(void) {
  const char* html;

  html = "</BODY>";
  scan_foot_testcase("uppercase", html, html);
  html = "</body>";
  scan_foot_testcase("lowercase", html, html);
  html = "</Body>";
  scan_foot_testcase("capitalized", html, html);
  html = "</body><hello></body>";
  scan_foot_testcase("last found", html, html + 14);

  /* .123456789.123456789.*/
  html = "</p></footer></div></body>";
  scan_foot_testcase("normal use", html, html + 19);
}

static void test_scan_html_for_footer_cross_agent(void) {
  int rv = 0;
  char pattern[512];
  size_t i = 0;
  glob_t glob_buf;

  nr_memset(&glob_buf, 0, sizeof(glob_buf));
  pattern[0] = '\0';
  snprintf(pattern, sizeof(pattern), "%s/rum_footer_insertion_location/*.html",
           CROSS_AGENT_TESTS_DIR);
  rv = glob(pattern, 0, NULL, &glob_buf);
  tlib_pass_if_true(
      "cross agent footer insertion", (0 == rv) || (GLOB_NOMATCH == rv),
      "failed to glob test files: errno=%s, glob=%s", nr_errno(errno), pattern);

  for (i = 0; i < glob_buf.gl_pathc; ++i) {
    cross_agent_footer_testcase(glob_buf.gl_pathv[i]);
  }

  globfree(&glob_buf);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_do_autorum();
  test_scan_html();
  test_scan_html_for_head_from_cross_agent_tests();
  test_rum_injection();
  test_produce_header_bad_params();
  test_produce_header();
  test_get_attributes();
  test_produce_footer_bad_params();
  test_produce_footer_testcases();
  test_produce_footer_all_fields();
  test_produce_footer_no_fields();
  test_scan_html_for_foot_bad_params();
  test_scan_html_for_foot_failure();
  test_scan_html_for_foot_success();
  test_scan_html_for_footer_cross_agent();
}
