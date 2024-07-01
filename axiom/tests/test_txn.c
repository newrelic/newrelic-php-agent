/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "nr_analytics_events.h"
#include "nr_distributed_trace.h"
#include "nr_distributed_trace_private.h"
#include "nr_errors.h"
#include "nr_guid.h"
#include "nr_header.h"
#include "nr_header_private.h"
#include "nr_rules.h"
#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_segment_tree.h"
#include "nr_slowsqls.h"
#include "nr_txn.h"
#include "nr_txn_private.h"
#include "util_base64.h"
#include "util_flatbuffers.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_metrics_private.h"
#include "util_random.h"
#include "util_strings.h"
#include "util_text.h"
#include "util_url.h"
#include "util_vector.h"

#include "nr_commands_private.h"

#include "tlib_main.h"
#include "test_app_helpers.h"
#include "test_segment_helpers.h"

typedef struct _test_txn_state_t {
  nrapp_t* txns_app;
} test_txn_state_t;

/*
 * hash_is_subset_of is a callback that can be passed to nro_iteratehash. It
 * asserts that one hashmap is a subset of another hashmap:
 *
 *   nro_iteratehash(subset, hash_is_subset_of, fullset);
 *
 * The composite hash_is_subset_of_data_t is used to let the tlib
 * assertion print a valid test name. This is especially useful for
 * cross agent tests read from JSON definitions.
 */
typedef struct {
  const char* testname;
  nrobj_t* set;
  const char* file;
  int line;
} hash_is_subset_of_data_t;

static bool need_to_stringify(const nrobj_t* val, const nrobj_t* obj) {
  nrotype_t expected_type;
  nrotype_t found_type;

  if (NULL == val || NULL == obj) {
    return false;
  }

  expected_type = nro_type(val);
  found_type = nro_type(obj);

  if (NR_OBJECT_STRING != expected_type || NR_OBJECT_INVALID == found_type) {
    return false;
  }

  if (expected_type != found_type) {
    return true;
  }

  return false;
}

static nr_status_t hash_is_subset_of(const char* key,
                                     const nrobj_t* val,
                                     void* ptr) {
  hash_is_subset_of_data_t* data = (hash_is_subset_of_data_t*)ptr;
  /*
   * Comparing the JSON representation allows us to compare values of arbitrary
   * types.
   */
  char* expected = nro_to_json(val);
  char* found;
  const nrobj_t* found_obj = nro_get_hash_value(data->set, key, NULL);

  if (need_to_stringify(val, found_obj)) {
    found = nro_stringify(found_obj);
  } else {
    found = nro_to_json(found_obj);
  }

  test_pass_if_true_file_line(
      data->testname, 0 == nr_strcmp(expected, found), data->file, data->line,
      "key='%s' expected='%s' found='%s'", NRSAFESTR(key), NRSAFESTR(expected),
      NRSAFESTR(found));

  nr_free(expected);
  nr_free(found);

  return NR_SUCCESS;
}

#define TEST_DAEMON_ID 1357

nrapp_t* nr_app_verify_id(nrapplist_t* applist NRUNUSED,
                          const char* agent_run_id NRUNUSED) {
  nr_status_t rv;
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();

  if (0 == p->txns_app) {
    return 0;
  }

  rv = nrt_mutex_lock(&p->txns_app->app_lock);
  tlib_pass_if_true("app locked", NR_SUCCESS == rv, "rv=%d", (int)rv);
  return p->txns_app;
}

const char* nr_app_get_host_name(const nrapp_t* app) {
  if (NULL == app) {
    return NULL;
  }

  return app->host_name;
}

const char* nr_app_get_entity_guid(const nrapp_t* app) {
  if (NULL == app) {
    return NULL;
  }

  return app->entity_guid;
}

#define test_freeze_name(...) \
  test_freeze_name_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_freeze_name_fn(const char* testname,
                                nr_path_type_t path_type,
                                int background,
                                const char* path,
                                const char* rules,
                                const char* segment_terms,
                                const char* expected_name,
                                const char* file,
                                int line) {
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrapp_t appv = {.info = {0}};
  nrapp_t* app = &appv;
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();

  nrt_mutex_init(&app->app_lock, 0);
  txn->app_connect_reply = 0;
  p->txns_app = app;

  txn->status.ignore = 0;
  txn->name = 0;
  txn->options.apdex_t = 0;
  txn->options.tt_is_apdex_f = 0;
  txn->options.tt_threshold = 0;

  txn->status.path_is_frozen = 0;
  txn->status.path_type = path_type;
  txn->status.background = background;
  txn->path = nr_strdup(path);

  if (rules) {
    nrobj_t* ob = nro_create_from_json(rules);

    app->url_rules
        = nr_rules_create_from_obj(nro_get_hash_array(ob, "url_rules", 0));
    app->txn_rules
        = nr_rules_create_from_obj(nro_get_hash_array(ob, "txn_rules", 0));
    nro_delete(ob);
  } else {
    app->url_rules = 0;
    app->txn_rules = 0;
  }

  if (segment_terms) {
    nrobj_t* st_obj = nro_create_from_json(segment_terms);

    app->segment_terms = nr_segment_terms_create_from_obj(st_obj);

    nro_delete(st_obj);
  } else {
    app->segment_terms = 0;
  }

  rv = nr_txn_freeze_name_update_apdex(txn);

  /* Txn path should be frozen no matter the return value. */
  test_pass_if_true(testname, (0 != txn->status.path_is_frozen),
                    "txn->status.path_is_frozen=%d",
                    txn->status.path_is_frozen);

  /*
   * Since there are no key transactions (0 == txn->app_connect_reply), apdex
   * and threshold should be unchanged.
   */
  test_pass_if_true(
      testname, (0 == txn->options.tt_threshold) && (0 == txn->options.apdex_t),
      "txn->options.tt_threshold=" NR_TIME_FMT
      " txn->options.apdex_t=" NR_TIME_FMT,
      txn->options.tt_threshold, txn->options.apdex_t);

  if (0 == expected_name) {
    test_pass_if_true(testname, NR_FAILURE == rv, "rv=%d", (int)rv);
  } else {
    test_pass_if_true(testname, NR_SUCCESS == rv, "rv=%d", (int)rv);
    test_pass_if_true(testname, 0 == nr_strcmp(expected_name, txn->name),
                      "expected_name=%s actual_name=%s", expected_name,
                      NRSAFESTR(txn->name));
  }

  nr_free(txn->path);
  nr_free(txn->name);
  nr_rules_destroy(&app->url_rules);
  nr_rules_destroy(&app->txn_rules);
  nr_segment_terms_destroy(&app->segment_terms);
  nrt_mutex_destroy(&app->app_lock);
}

#define test_key_txns(...) test_key_txns_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_key_txns_fn(const char* testname,
                             const char* path,
                             int is_apdex_f,
                             nrtime_t expected_apdex_t,
                             nrtime_t expected_tt_threshold,
                             const char* rules,
                             const char* segment_terms,
                             nrobj_t* key_txns,
                             const char* file,
                             int line) {
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrapp_t appv = {.info = {0}};
  nrapp_t* app = &appv;
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();

  nrt_mutex_init(&app->app_lock, 0);
  txn->app_connect_reply = nro_new_hash();
  nro_set_hash(txn->app_connect_reply, "web_transactions_apdex", key_txns);
  p->txns_app = app;

  txn->status.ignore = 0;
  txn->name = 0;
  txn->options.apdex_t = 0;
  txn->options.tt_threshold = 0;

  txn->options.tt_is_apdex_f = is_apdex_f;
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_URI;
  txn->status.background = 0;
  txn->path = nr_strdup(path);

  if (rules) {
    nrobj_t* ob = nro_create_from_json(rules);

    app->url_rules
        = nr_rules_create_from_obj(nro_get_hash_array(ob, "url_rules", 0));
    app->txn_rules
        = nr_rules_create_from_obj(nro_get_hash_array(ob, "txn_rules", 0));
    nro_delete(ob);
  } else {
    app->url_rules = 0;
    app->txn_rules = 0;
  }

  if (segment_terms) {
    nrobj_t* st_obj = nro_create_from_json(segment_terms);

    app->segment_terms = nr_segment_terms_create_from_obj(st_obj);

    nro_delete(st_obj);
  } else {
    app->segment_terms = 0;
  }

  rv = nr_txn_freeze_name_update_apdex(txn);

  test_pass_if_true(testname, NR_SUCCESS == rv, "rv=%d", (int)rv);
  test_pass_if_true(testname, expected_apdex_t == txn->options.apdex_t,
                    "expected_apdex_t=" NR_TIME_FMT
                    " txn->options.apdex_t=" NR_TIME_FMT,
                    expected_apdex_t, txn->options.apdex_t);
  test_pass_if_true(testname,
                    expected_tt_threshold == txn->options.tt_threshold,
                    "expected_tt_threshold=" NR_TIME_FMT
                    " txn->options.tt_threshold=" NR_TIME_FMT,
                    expected_tt_threshold, txn->options.tt_threshold);

  nro_delete(txn->app_connect_reply);
  nr_free(txn->name);
  nr_free(txn->path);
  nr_rules_destroy(&app->url_rules);
  nr_rules_destroy(&app->txn_rules);
  nr_segment_terms_destroy(&app->segment_terms);
  nrt_mutex_destroy(&app->app_lock);
}

static void test_txn_cmp_options(void) {
  nrtxnopt_t o1 = {.custom_events_enabled = 1};
  nrtxnopt_t o2 = {.custom_events_enabled = 1};

  bool rv = false;

  rv = nr_txn_cmp_options(NULL, NULL);
  tlib_pass_if_true("NULL pointers are equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options(&o1, &o1);
  tlib_pass_if_true("Equal pointers are equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options(&o1, &o2);
  tlib_pass_if_true("Equal fields are equal", rv, "rv=%d", (int)rv);

  o2.custom_events_enabled = 0;

  rv = nr_txn_cmp_options(NULL, &o1);
  tlib_pass_if_false("NULL and other are not equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options(&o1, NULL);
  tlib_pass_if_false("Other and null are not equal", rv, "rv=%d", (int)rv);

  rv = nr_txn_cmp_options(&o1, &o2);
  tlib_pass_if_false("Inequal fields are not equal", rv, "rv=%d", (int)rv);
}

const char test_rules[]
    = "{\"url_rules\":[{\"match_expression\":\"what\",        "
      "\"replacement\":\"txn\"},"
      "{\"match_expression\":\"ignore_path\", \"ignore\":true}],"
      "\"txn_rules\":[{\"match_expression\":\"ignore_txn\",  \"ignore\":true},"
      "{\"match_expression\":\"rename_txn\",  \"replacement\":\"ok\"}]}";

const char test_segment_terms[]
    = "["
      "{\"prefix\":\"WebTransaction/Custom\",\"terms\":[\"white\",\"list\"]}"
      "]";

static void test_freeze_name_update_apdex(void) {
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();

  /*
   * Test : Bad input to nr_txn_freeze_name_update_apdex
   */
  {
    nr_status_t rv;
    nrtxn_t txnv;
    nrtxn_t* txn = &txnv;
    nrapp_t appv = {.info = {0}};
    nrapp_t* app = &appv;

    nrt_mutex_init(&app->app_lock, 0);
    txn->path = 0;
    txn->status.ignore = 0;
    txn->name = 0;
    txn->status.background = 0;
    txn->status.path_is_frozen = 0;
    txn->status.path_type = NR_PATH_TYPE_URI;
    txn->app_connect_reply = 0;
    app->url_rules = 0;
    app->txn_rules = 0;
    app->segment_terms = 0;
    p->txns_app = app;

    rv = nr_txn_freeze_name_update_apdex(0);
    tlib_pass_if_true("no txn", NR_FAILURE == rv, "rv=%d", (int)rv);

    p->txns_app = 0;
    rv = nr_txn_freeze_name_update_apdex(txn);
    tlib_pass_if_true("no app", NR_FAILURE == rv, "rv=%d", (int)rv);
    p->txns_app = app;

    txn->status.ignore = 1;
    rv = nr_txn_freeze_name_update_apdex(txn);
    tlib_pass_if_true("ignore txn", NR_FAILURE == rv, "rv=%d", (int)rv);
    txn->status.ignore = 0;

    txn->status.path_is_frozen = 1;
    txn->status.path_type = NR_PATH_TYPE_URI;
    rv = nr_txn_freeze_name_update_apdex(txn);
    tlib_pass_if_true("already frozen", (NR_SUCCESS == rv) && (0 == txn->name),
                      "rv=%d txn->name=%p", (int)rv, txn->name);
    txn->status.path_is_frozen = 0;
    txn->status.path_type = NR_PATH_TYPE_URI;

    rv = nr_txn_freeze_name_update_apdex(txn);
    tlib_pass_if_true(
        "no path",
        (NR_SUCCESS == rv)
            && (0 == nr_strcmp(txn->name, "WebTransaction/Uri/unknown")),
        "rv=%d txn->name=%s", (int)rv, NRSAFESTR(txn->name));

    nr_free(txn->name);
    nrt_mutex_destroy(&app->app_lock);
  }

  /*
   * Transaction Naming Tests
   *
   * url_rules should only be applied to URI non-background txns and CUSTOM
   * non-background txns.
   */

  /*
   * Test : URI Web Transaction Naming
   */
  test_freeze_name("URI WT", NR_PATH_TYPE_URI, 0, "/zap.php", 0, 0,
                   "WebTransaction/Uri/zap.php");
  test_freeze_name("URI WT no slash", NR_PATH_TYPE_URI, 0, "zap.php", 0, 0,
                   "WebTransaction/Uri/zap.php");
  test_freeze_name("URI WT url_rule change", NR_PATH_TYPE_URI, 0, "/what.php",
                   test_rules, 0, "WebTransaction/Uri/txn.php");
  test_freeze_name("URI WT url_rule ignore", NR_PATH_TYPE_URI, 0,
                   "/ignore_path.php", test_rules, 0, 0);
  test_freeze_name("URI WT url_rule and txn_rule change", NR_PATH_TYPE_URI, 0,
                   "/rename_what.php", test_rules, 0,
                   "WebTransaction/Uri/ok.php");
  test_freeze_name("URI WT url_rule change txn_rule ignore", NR_PATH_TYPE_URI,
                   0, "/ignore_what.php", test_rules, 0, 0);

  /*
   * Test : URI Background Naming
   */
  test_freeze_name("URI BG", NR_PATH_TYPE_URI, 1, "/zap.php", 0, 0,
                   "OtherTransaction/php/zap.php");
  test_freeze_name("URI BG no slash", NR_PATH_TYPE_URI, 1, "zap.php", 0, 0,
                   "OtherTransaction/php/zap.php");
  test_freeze_name("URI BG url_rule no change", NR_PATH_TYPE_URI, 1,
                   "/what.php", test_rules, 0, "OtherTransaction/php/what.php");
  test_freeze_name("URI BG url_rule no ignore", NR_PATH_TYPE_URI, 1,
                   "/ignore_path.php", test_rules, 0,
                   "OtherTransaction/php/ignore_path.php");
  test_freeze_name("URI BG txn_rule change", NR_PATH_TYPE_URI, 1,
                   "/rename_txn.php", test_rules, 0,
                   "OtherTransaction/php/ok.php");
  test_freeze_name("URI BG txn_rule ignore", NR_PATH_TYPE_URI, 1,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : Status code web transaction naming.
   */
  test_freeze_name("STATUS WT", NR_PATH_TYPE_STATUS_CODE, 0, "/404", 0, 0,
                   "WebTransaction/StatusCode/404");
  test_freeze_name("STATUS WT url_rule no change", NR_PATH_TYPE_STATUS_CODE, 0,
                   "/404", test_rules, 0, "WebTransaction/StatusCode/404");
  test_freeze_name("STATUS WT url_rule no ignore", NR_PATH_TYPE_STATUS_CODE, 0,
                   "/ignore_path", test_rules, 0,
                   "WebTransaction/StatusCode/ignore_path");
  test_freeze_name("STATUS WT txn_rule change", NR_PATH_TYPE_STATUS_CODE, 0,
                   "/rename_txn", test_rules, 0,
                   "WebTransaction/StatusCode/ok");
  test_freeze_name("STATUS WT txn_rule ignore", NR_PATH_TYPE_STATUS_CODE, 0,
                   "/ignore_txn", test_rules, 0, 0);

  /*
   * Test : Status code background transaction naming.
   */
  test_freeze_name("STATUS WT", NR_PATH_TYPE_STATUS_CODE, 1, "/404", 0, 0,
                   "OtherTransaction/StatusCode/404");
  test_freeze_name("STATUS WT url_rule no change", NR_PATH_TYPE_STATUS_CODE, 1,
                   "/404", test_rules, 0, "OtherTransaction/StatusCode/404");
  test_freeze_name("STATUS WT url_rule no ignore", NR_PATH_TYPE_STATUS_CODE, 1,
                   "/ignore_path", test_rules, 0,
                   "OtherTransaction/StatusCode/ignore_path");
  test_freeze_name("STATUS WT txn_rule change", NR_PATH_TYPE_STATUS_CODE, 1,
                   "/rename_txn", test_rules, 0,
                   "OtherTransaction/StatusCode/ok");
  test_freeze_name("STATUS WT txn_rule ignore", NR_PATH_TYPE_STATUS_CODE, 1,
                   "/ignore_txn", test_rules, 0, 0);

  /*
   * Test : ACTION Web Transaction Naming
   */
  test_freeze_name("ACTION WT", NR_PATH_TYPE_ACTION, 0, "/zap.php", 0, 0,
                   "WebTransaction/Action/zap.php");
  test_freeze_name("ACTION WT no slash", NR_PATH_TYPE_ACTION, 0, "zap.php", 0,
                   0, "WebTransaction/Action/zap.php");
  test_freeze_name("ACTION WT url_rule no change", NR_PATH_TYPE_ACTION, 0,
                   "/what.php", test_rules, 0,
                   "WebTransaction/Action/what.php");
  test_freeze_name("ACTION WT url_rule no ignore", NR_PATH_TYPE_ACTION, 0,
                   "/ignore_path.php", test_rules, 0,
                   "WebTransaction/Action/ignore_path.php");
  test_freeze_name("ACTION WT txn_rule change", NR_PATH_TYPE_ACTION, 0,
                   "/rename_txn.php", test_rules, 0,
                   "WebTransaction/Action/ok.php");
  test_freeze_name("ACTION WT txn_rule ignore", NR_PATH_TYPE_ACTION, 0,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : ACTION Background Naming
   */
  test_freeze_name("ACTION BG", NR_PATH_TYPE_ACTION, 1, "/zap.php", 0, 0,
                   "OtherTransaction/Action/zap.php");
  test_freeze_name("ACTION BG no slash", NR_PATH_TYPE_ACTION, 1, "zap.php", 0,
                   0, "OtherTransaction/Action/zap.php");
  test_freeze_name("ACTION BG url_rule no change", NR_PATH_TYPE_ACTION, 1,
                   "/what.php", test_rules, 0,
                   "OtherTransaction/Action/what.php");
  test_freeze_name("ACTION BG url_rule no ignore", NR_PATH_TYPE_ACTION, 1,
                   "/ignore_path.php", test_rules, 0,
                   "OtherTransaction/Action/ignore_path.php");
  test_freeze_name("ACTION BG txn_rule change", NR_PATH_TYPE_ACTION, 1,
                   "/rename_txn.php", test_rules, 0,
                   "OtherTransaction/Action/ok.php");
  test_freeze_name("ACTION BG txn_rule ignore", NR_PATH_TYPE_ACTION, 1,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : FUNCTION Web Transaction Naming
   */
  test_freeze_name("FUNCTION WT", NR_PATH_TYPE_FUNCTION, 0, "/zap.php", 0, 0,
                   "WebTransaction/Function/zap.php");
  test_freeze_name("FUNCTION WT no slash", NR_PATH_TYPE_FUNCTION, 0, "zap.php",
                   0, 0, "WebTransaction/Function/zap.php");
  test_freeze_name("FUNCTION WT url_rule no change", NR_PATH_TYPE_FUNCTION, 0,
                   "/what.php", test_rules, 0,
                   "WebTransaction/Function/what.php");
  test_freeze_name("FUNCTION WT url_rule no ignore", NR_PATH_TYPE_FUNCTION, 0,
                   "/ignore_path.php", test_rules, 0,
                   "WebTransaction/Function/ignore_path.php");
  test_freeze_name("FUNCTION WT txn_rule change", NR_PATH_TYPE_FUNCTION, 0,
                   "/rename_txn.php", test_rules, 0,
                   "WebTransaction/Function/ok.php");
  test_freeze_name("FUNCTION WT txn_rule ignore", NR_PATH_TYPE_FUNCTION, 0,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : FUNCTION Background Naming
   */
  test_freeze_name("FUNCTION BG", NR_PATH_TYPE_FUNCTION, 1, "/zap.php", 0, 0,
                   "OtherTransaction/Function/zap.php");
  test_freeze_name("FUNCTION BG no slash", NR_PATH_TYPE_FUNCTION, 1, "zap.php",
                   0, 0, "OtherTransaction/Function/zap.php");
  test_freeze_name("FUNCTION BG url_rule no change", NR_PATH_TYPE_FUNCTION, 1,
                   "/what.php", test_rules, 0,
                   "OtherTransaction/Function/what.php");
  test_freeze_name("FUNCTION BG url_rule no ignore", NR_PATH_TYPE_FUNCTION, 1,
                   "/ignore_path.php", test_rules, 0,
                   "OtherTransaction/Function/ignore_path.php");
  test_freeze_name("FUNCTION BG txn_rule change", NR_PATH_TYPE_FUNCTION, 1,
                   "/rename_txn.php", test_rules, 0,
                   "OtherTransaction/Function/ok.php");
  test_freeze_name("FUNCTION BG txn_rule ignore", NR_PATH_TYPE_FUNCTION, 1,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : CUSTOM Web Transaction Naming
   */
  test_freeze_name("CUSTOM WT", NR_PATH_TYPE_CUSTOM, 0, "/zap.php", 0, 0,
                   "WebTransaction/Custom/zap.php");
  test_freeze_name("CUSTOM WT no slash", NR_PATH_TYPE_CUSTOM, 0, "zap.php", 0,
                   0, "WebTransaction/Custom/zap.php");
  test_freeze_name("CUSTOM WT url_rule change", NR_PATH_TYPE_CUSTOM, 0,
                   "/what.php", test_rules, 0, "WebTransaction/Custom/txn.php");
  test_freeze_name("CUSTOM WT url_rule ignore", NR_PATH_TYPE_CUSTOM, 0,
                   "/ignore_path.php", test_rules, 0, 0);
  test_freeze_name("CUSTOM WT url_rule and txn_rule change",
                   NR_PATH_TYPE_CUSTOM, 0, "/rename_what.php", test_rules, 0,
                   "WebTransaction/Custom/ok.php");
  test_freeze_name("CUSTOM WT url_rule change txn_rule ignore",
                   NR_PATH_TYPE_CUSTOM, 0, "/ignore_what.php", test_rules, 0,
                   0);

  /*
   * Test : CUSTOM Background Naming
   */
  test_freeze_name("CUSTOM BG", NR_PATH_TYPE_CUSTOM, 1, "/zap.php", 0, 0,
                   "OtherTransaction/Custom/zap.php");
  test_freeze_name("CUSTOM BG no slash", NR_PATH_TYPE_CUSTOM, 1, "zap.php", 0,
                   0, "OtherTransaction/Custom/zap.php");
  test_freeze_name("CUSTOM BG url_rule no change", NR_PATH_TYPE_CUSTOM, 1,
                   "/what.php", test_rules, 0,
                   "OtherTransaction/Custom/what.php");
  test_freeze_name("CUSTOM BG url_rule no ignore", NR_PATH_TYPE_CUSTOM, 1,
                   "/ignore_path.php", test_rules, 0,
                   "OtherTransaction/Custom/ignore_path.php");
  test_freeze_name("CUSTOM BG txn_rule change", NR_PATH_TYPE_CUSTOM, 1,
                   "/rename_txn.php", test_rules, 0,
                   "OtherTransaction/Custom/ok.php");
  test_freeze_name("CUSTOM BG txn_rule ignore", NR_PATH_TYPE_CUSTOM, 1,
                   "/ignore_txn.php", test_rules, 0, 0);

  /*
   * Test : UNKNOWN Web Transaction Naming
   */
  test_freeze_name("UNKNOWN WT", NR_PATH_TYPE_UNKNOWN, 0, "/zap.php", 0, 0,
                   "WebTransaction/Uri/<unknown>");
  test_freeze_name("UNKNOWN WT no slash", NR_PATH_TYPE_UNKNOWN, 0, "zap.php", 0,
                   0, "WebTransaction/Uri/<unknown>");
  test_freeze_name("UNKNOWN WT url_rule no change", NR_PATH_TYPE_UNKNOWN, 0,
                   "/what.php", test_rules, 0, "WebTransaction/Uri/<unknown>");
  test_freeze_name("UNKNOWN WT url_rule no ignore", NR_PATH_TYPE_UNKNOWN, 0,
                   "/ignore_path.php", test_rules, 0,
                   "WebTransaction/Uri/<unknown>");
  test_freeze_name("UNKNOWN WT txn_rule no change", NR_PATH_TYPE_UNKNOWN, 0,
                   "/rename_txn.php", test_rules, 0,
                   "WebTransaction/Uri/<unknown>");
  test_freeze_name("UNKNOWN WT txn_rule no ignore", NR_PATH_TYPE_UNKNOWN, 0,
                   "/ignore_txn.php", test_rules, 0,
                   "WebTransaction/Uri/<unknown>");

  /*
   * Test : UNKNOWN Background Naming
   */
  test_freeze_name("UNKNOWN BG", NR_PATH_TYPE_UNKNOWN, 1, "/zap.php", 0, 0,
                   "OtherTransaction/php/<unknown>");
  test_freeze_name("UNKNOWN BG no slash", NR_PATH_TYPE_UNKNOWN, 1, "zap.php", 0,
                   0, "OtherTransaction/php/<unknown>");
  test_freeze_name("UNKNOWN BG url_rule no change", NR_PATH_TYPE_UNKNOWN, 1,
                   "/what.php", test_rules, 0,
                   "OtherTransaction/php/<unknown>");
  test_freeze_name("UNKNOWN BG url_rule no ignore", NR_PATH_TYPE_UNKNOWN, 1,
                   "/ignore_path.php", test_rules, 0,
                   "OtherTransaction/php/<unknown>");
  test_freeze_name("UNKNOWN BG txn_rule no change", NR_PATH_TYPE_UNKNOWN, 1,
                   "/rename_txn.php", test_rules, 0,
                   "OtherTransaction/php/<unknown>");
  test_freeze_name("UNKNOWN BG txn_rule no ignore", NR_PATH_TYPE_UNKNOWN, 1,
                   "/ignore_txn.php", test_rules, 0,
                   "OtherTransaction/php/<unknown>");

  /*
   * Test : Segment term application
   */
  test_freeze_name("Prefix does not match", NR_PATH_TYPE_ACTION, 0, "/zap.php",
                   0, test_segment_terms, "WebTransaction/Action/zap.php");
  test_freeze_name("Prefix matches; all whitelisted", NR_PATH_TYPE_CUSTOM, 0,
                   "/white/list", 0, test_segment_terms,
                   "WebTransaction/Custom/white/list");
  test_freeze_name("Prefix matches; none whitelisted", NR_PATH_TYPE_CUSTOM, 0,
                   "/black/foo", 0, test_segment_terms,
                   "WebTransaction/Custom/*");
  test_freeze_name("Prefix matches; some whitelisted", NR_PATH_TYPE_CUSTOM, 0,
                   "/black/list", 0, test_segment_terms,
                   "WebTransaction/Custom/*/list");

  /*
   * Test : Key Transactions
   */
  {
    nrobj_t* key_txns = nro_create_from_json(
        "{\"WebTransaction\\/Uri\\/key\":0.1,"
        "\"WebTransaction\\/Uri\\/ok\":0.1,"
        "\"WebTransaction\\/Uri\\/key_int\":2,"
        "\"WebTransaction\\/Uri\\/key_negative\":-0.1}");

    test_key_txns("not key txn", "/not", 1, 0, 0, test_rules, 0, key_txns);
    test_key_txns("key txn", "/key", 0, 100000, 0, test_rules, 0, key_txns);
    test_key_txns("key txn is_apdex_f", "/key", 1, 100000, 400000, test_rules,
                  0, key_txns);
    test_key_txns("key txn after rules", "/rename_what", 0, 100000, 0,
                  test_rules, 0, key_txns);
    test_key_txns("key txn after rules is_apdex_f", "/rename_what", 1, 100000,
                  400000, test_rules, 0, key_txns);
    test_key_txns("key txn apdex int", "/key_int", 0, 2000000, 0, test_rules, 0,
                  key_txns);
    test_key_txns("key txn apdex negative", "/key_negative", 0, 0, 0,
                  test_rules, 0, key_txns);

    nro_delete(key_txns);
  }
}

#define test_apdex_metric_created(...) \
  test_apdex_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_apdex_metric_created_fn(const char* testname,
                                         nrmtable_t* table,
                                         uint32_t flags,
                                         const char* name,
                                         nrtime_t satisfying,
                                         nrtime_t tolerating,
                                         nrtime_t failing,
                                         nrtime_t min,
                                         nrtime_t max,
                                         const char* file,
                                         int line) {
  const nrmetric_t* m = nrm_find(table, name);
  const char* nm = nrm_get_name(table, m);

  test_pass_if_true_file_line(testname, 0 != m, file, line, "m=%p", m);
  test_pass_if_true_file_line(testname, 0 == nr_strcmp(nm, name), file, line,
                              "nm=%s name=%s", nm, name);

  test_metric_values_are_fn(testname, m, flags | MET_IS_APDEX, satisfying,
                            tolerating, failing, min, max, 0, file, line);
}

#define test_apdex_metrics(...) \
  test_apdex_metrics_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_apdex_metrics_fn(const char* txn_name,
                                  int has_error,
                                  nrtime_t duration,
                                  nrtime_t apdex_t,
                                  const char* mname,
                                  nrtime_t satisfying,
                                  nrtime_t tolerating,
                                  nrtime_t failing,
                                  const char* file,
                                  int line) {
  int table_size;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->unscoped_metrics = nrm_table_create(0);
  txn->name = nr_strdup(txn_name);
  txn->options.apdex_t = apdex_t;
  txn->error = NULL;

  if (has_error) {
    int priority = 5;

    txn->error = nr_error_create(priority, "my/msg", "my/class", "my/span_id",
                                 "[\"my\\/stacktrace\"]", nr_get_time());
  }

  nr_txn_create_apdex_metrics(txn, duration);

  /*
   * Test : 'Apdex' metric created and is correct.
   */
  test_apdex_metric_created_fn(txn_name, txn->unscoped_metrics, MET_FORCED,
                               "Apdex", satisfying, tolerating, failing,
                               apdex_t, apdex_t, file, line);

  /*
   * Test : Specific apdex metric created and correct, and table size.
   */
  table_size = nrm_table_size(txn->unscoped_metrics);
  if (mname) {
    test_apdex_metric_created_fn(txn_name, txn->unscoped_metrics, 0, mname,
                                 satisfying, tolerating, failing, apdex_t,
                                 apdex_t, file, line);

    test_pass_if_true(txn_name, 2 == table_size, "table_size=%d", table_size);
  } else {
    test_pass_if_true(txn_name, 1 == table_size, "table_size=%d", table_size);
  }

  nr_free(txn->name);
  nrm_table_destroy(&txn->unscoped_metrics);
  nr_error_destroy(&txn->error);
}

static void test_create_apdex_metrics(void) {
  /* Should not blow up on NULL input */
  nr_txn_create_apdex_metrics(0, 0);

  /*
   * Test : Apdex value is properly calculated.
   */
  test_apdex_metrics(NULL, 0, 2, 4, NULL, 1, 0, 0);
  test_apdex_metrics("nope", 0, 2, 4, NULL, 1, 0, 0);
  test_apdex_metrics("OtherTransaction/php/path.php", 0, 2, 4,
                     "Apdex/php/path.php", 1, 0, 0);
  test_apdex_metrics("WebTransaction/Uri/path.php", 0, 2, 4,
                     "Apdex/Uri/path.php", 1, 0, 0);
  test_apdex_metrics("OtherTransaction/Action/path.php", 0, 5, 4,
                     "Apdex/Action/path.php", 0, 1, 0);
  test_apdex_metrics("WebTransaction/Action/path.php", 0, 17, 4,
                     "Apdex/Action/path.php", 0, 0, 1);
  test_apdex_metrics("OtherTransaction/Function/path.php", 1, 1, 4,
                     "Apdex/Function/path.php", 0, 0, 1);
  test_apdex_metrics("WebTransaction/Function/path.php", 0, 2, 4,
                     "Apdex/Function/path.php", 1, 0, 0);
  test_apdex_metrics("OtherTransaction/Custom/path.php", 0, 2, 4,
                     "Apdex/Custom/path.php", 1, 0, 0);
  test_apdex_metrics("OtherTransaction/php/<unknown>", 0, 2, 4,
                     "Apdex/php/<unknown>", 1, 0, 0);
  test_apdex_metrics("WebTransaction/Uri/<unknown>", 0, 2, 4,
                     "Apdex/Uri/<unknown>", 1, 0, 0);
}

static void test_create_error_metrics(void) {
  int table_size;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->status.background = 0;
  txn->trace_strings = 0;
  txn->unscoped_metrics = 0;
  txn->options.distributed_tracing_enabled = false;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_error_metrics(0, 0);
  nr_txn_create_error_metrics(0, "WebTransaction/Action/not_words");
  nr_txn_create_error_metrics(txn, 0);
  nr_txn_create_error_metrics(txn, "");
  nr_txn_create_error_metrics(
      txn, "WebTransaction/Action/not_words"); /* No metric table */

  /*
   * Test : Web Transaction
   */
  txn->trace_strings = nr_string_pool_create();
  txn->unscoped_metrics = nrm_table_create(2);

  nr_txn_create_error_metrics(txn, "WebTransaction/Action/not_words");

  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("three error metrics created", 3 == table_size,
                    "table_size=%d", table_size);
  test_txn_metric_is("rollup", txn->unscoped_metrics, MET_FORCED, "Errors/all",
                     1, 0, 0, 0, 0, 0);
  test_txn_metric_is("web rollup", txn->unscoped_metrics, MET_FORCED,
                     "Errors/allWeb", 1, 0, 0, 0, 0, 0);
  test_txn_metric_is("specific", txn->unscoped_metrics, MET_FORCED,
                     "Errors/WebTransaction/Action/not_words", 1, 0, 0, 0, 0,
                     0);

  /*
   * Test : Background Task
   */
  nr_string_pool_destroy(&txn->trace_strings);
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->trace_strings = nr_string_pool_create();
  txn->unscoped_metrics = nrm_table_create(2);

  txn->status.background = 1;
  nr_txn_create_error_metrics(txn, "OtherTransaction/Custom/zap");

  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("three error metrics created", 3 == table_size,
                    "table_size=%d", table_size);
  test_txn_metric_is("rollup", txn->unscoped_metrics, MET_FORCED, "Errors/all",
                     1, 0, 0, 0, 0, 0);
  test_txn_metric_is("background rollup", txn->unscoped_metrics, MET_FORCED,
                     "Errors/allOther", 1, 0, 0, 0, 0, 0);
  test_txn_metric_is("specific", txn->unscoped_metrics, MET_FORCED,
                     "Errors/OtherTransaction/Custom/zap", 1, 0, 0, 0, 0, 0);

  nr_string_pool_destroy(&txn->trace_strings);
  nrm_table_destroy(&txn->unscoped_metrics);
}

static void test_create_duration_metrics(void) {
  int table_size;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  const nrtime_t duration = 999;
  const nrtime_t total_time = 1999;

  nr_memset((void*)txn, 0, sizeof(txnv));

  txn->status.background = 0;
  txn->unscoped_metrics = 0;
  txn->status.recording = 1;
  txn->segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);

  txn->segment_root = nr_segment_start(txn, NULL, NULL);
  txn->segment_root->start_time = 0;
  txn->segment_root->stop_time = duration;
  txn->segment_root->exclusive_time = nr_exclusive_time_create(16, 0, duration);

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_duration_metrics(NULL, duration, total_time);
  nr_txn_create_duration_metrics(txn, duration, total_time);  // No metric table

  /*
   * Test : Web Transaction
   */
  nr_exclusive_time_add_child(txn->segment_root->exclusive_time, 0, 111);
  txn->unscoped_metrics = nrm_table_create(2);
  txn->name = "WebTransaction/Action/not_words";
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("web txn", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction", 1, 999, 888, 999, 999, 998001);
  test_txn_metric_is("web txn", txn->unscoped_metrics, MET_FORCED,
                     "HttpDispatcher", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction/Action/not_words", 1, 999, 888, 999, 999,
                     998001);
  test_txn_metric_is("web txn", txn->unscoped_metrics, MET_FORCED,
                     "WebTransactionTotalTime", 1, 1999, 1999, 1999, 1999,
                     3996001);
  test_txn_metric_is("web txn", txn->unscoped_metrics, MET_FORCED,
                     "WebTransactionTotalTime/Action/not_words", 1, 1999, 1999,
                     1999, 1999, 3996001);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("number of metrics created", 5, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Test : Web Transaction No Exclusive
   */
  nr_exclusive_time_add_child(txn->segment_root->exclusive_time, 0, 1000);
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "HttpDispatcher", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction/Action/not_words", 1, 999, 0, 999, 999,
                     998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransactionTotalTime", 1, 1999, 1999, 1999, 1999,
                     3996001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransactionTotalTime/Action/not_words", 1, 1999, 1999,
                     1999, 1999, 3996001);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("number of metrics created", 5, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Test : Web Transaction (no slash)
   */
  txn->unscoped_metrics = nrm_table_create(2);
  txn->name = "NoSlash";
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "HttpDispatcher", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "NoSlash", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "WebTransactionTotalTime", 1, 1999, 1999, 1999, 1999,
                     3996001);
  test_txn_metric_is("web txn no exclusive", txn->unscoped_metrics, MET_FORCED,
                     "NoSlashTotalTime", 1, 1999, 1999, 1999, 1999, 3996001);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("number of metrics created", 5, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Background Task
   */
  nr_exclusive_time_destroy(&txn->segment_root->exclusive_time);
  txn->segment_root->exclusive_time = nr_exclusive_time_create(16, 0, duration);
  nr_exclusive_time_add_child(txn->segment_root->exclusive_time, 0, 111);
  txn->status.background = 1;
  txn->name = "WebTransaction/Action/not_words";
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("background", txn->unscoped_metrics, MET_FORCED,
                     "OtherTransaction/all", 1, 999, 888, 999, 999, 998001);
  test_txn_metric_is("background", txn->unscoped_metrics, MET_FORCED,
                     "WebTransaction/Action/not_words", 1, 999, 888, 999, 999,
                     998001);
  test_txn_metric_is("background", txn->unscoped_metrics, MET_FORCED,
                     "OtherTransactionTotalTime", 1, 1999, 1999, 1999, 1999,
                     3996001);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("number of metrics created", 4, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Background Task No Exclusive
   */
  nr_exclusive_time_add_child(txn->segment_root->exclusive_time, 0, 1111);
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("background no exclusive", txn->unscoped_metrics,
                     MET_FORCED, "OtherTransaction/all", 1, 999, 0, 999, 999,
                     998001);
  test_txn_metric_is("background no exclusive", txn->unscoped_metrics,
                     MET_FORCED, "WebTransaction/Action/not_words", 1, 999, 0,
                     999, 999, 998001);
  test_txn_metric_is("background no exclusive", txn->unscoped_metrics,
                     MET_FORCED, "OtherTransactionTotalTime", 1, 1999, 1999,
                     1999, 1999, 3996001);

  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("number of metrics created", 4, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Background Task (no slash)
   */
  txn->unscoped_metrics = nrm_table_create(2);
  txn->name = "NoSlash";
  nr_txn_create_duration_metrics(txn, duration, total_time);
  test_txn_metric_is("background no slash", txn->unscoped_metrics, MET_FORCED,
                     "OtherTransaction/all", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("background no slash", txn->unscoped_metrics, MET_FORCED,
                     "NoSlash", 1, 999, 0, 999, 999, 998001);
  test_txn_metric_is("background no slash", txn->unscoped_metrics, MET_FORCED,
                     "NoSlashTotalTime", 1, 1999, 1999, 1999, 1999, 3996001);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_int_equal("four duration metrics created", 4, table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  nr_segment_destroy_tree(txn->segment_root);
  nr_hashmap_destroy(&txn->parent_stacks);
  nr_stack_destroy_fields(&txn->default_parent_stack);
  nr_slab_destroy(&txn->segment_slab);
}

static void test_create_queue_metric(void) {
  int table_size;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->unscoped_metrics = 0;
  txn->abs_start_time = 444;
  txn->status.http_x_start = 333;
  txn->status.background = 0;

  /*
   * Test : Bad Params.  Should not blow up.
   */
  nr_txn_create_queue_metric(0);
  nr_txn_create_queue_metric(txn); /* No metric table */

  /*
   * Test : Non-Zero Queue Time
   */
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_queue_metric(txn);
  test_txn_metric_is("non-zero queue time", txn->unscoped_metrics, MET_FORCED,
                     "WebFrontend/QueueTime", 1, 111, 111, 111, 111, 12321);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("non-zero queue time", 1 == table_size, "table_size=%d",
                    table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Test : Background tasks should not have queue metrics.
   */
  txn->status.background = 1;
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_queue_metric(txn);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("no queue metrics for background", 0 == table_size,
                    "table_size=%d", table_size);
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->status.background = 0;

  /*
   * Test : No queue start addded.
   */
  txn->status.http_x_start = 0;
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_queue_metric(txn);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("no queue start", 0 == table_size, "table_size=%d",
                    table_size);
  nrm_table_destroy(&txn->unscoped_metrics);

  /*
   * Test : Start time before queue start.
   */
  txn->status.http_x_start = nr_txn_start_time(txn) + 1;
  txn->unscoped_metrics = nrm_table_create(2);
  nr_txn_create_queue_metric(txn);
  test_txn_metric_is("txn start before queue start", txn->unscoped_metrics,
                     MET_FORCED, "WebFrontend/QueueTime", 1, 0, 0, 0, 0, 0);
  table_size = nrm_table_size(txn->unscoped_metrics);
  tlib_pass_if_true("txn start before queue start", 1 == table_size,
                    "table_size=%d", table_size);
  nrm_table_destroy(&txn->unscoped_metrics);
}

static void test_set_path(void) {
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->path = 0;
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path(0, 0, 0, NR_PATH_TYPE_UNKNOWN, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path null params", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path null params", 0 == txn->path,
                    "txn->path=%p", txn->path);

  rv = nr_txn_set_path(0, 0, "path_uri", NR_PATH_TYPE_URI,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path null txn", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path null txn", 0 == txn->path, "txn->path=%p",
                    txn->path);

  rv = nr_txn_set_path(0, txn, 0, NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path null path", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path null path", 0 == txn->path, "txn->path=%p",
                    txn->path);

  rv = nr_txn_set_path(0, txn, "", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path empty path", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path empty path", 0 == txn->path,
                    "txn->path=%p", txn->path);

  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_UNKNOWN,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path zero ptype", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path zero ptype", 0 == txn->path,
                    "txn->path=%p", txn->path);

  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_UNKNOWN,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path negative ptype", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path negative ptype", 0 == txn->path,
                    "txn->path=%p", txn->path);

  txn->status.path_is_frozen = 1;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;
  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_URI,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path frozen", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path frozen", 0 == txn->path, "txn->path=%p",
                    txn->path);
  txn->status.path_is_frozen = 0;
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path(0, txn, "path_uri000", NR_PATH_TYPE_URI,
                       NR_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_URI,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path succeeds", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path sets path and ptype",
                    NR_PATH_TYPE_URI == txn->status.path_type,
                    "txn->status.path_type=%d", (int)txn->status.path_type);
  tlib_pass_if_true("nr_txn_set_path sets path and ptype",
                    0 == nr_strcmp(txn->path, "path_uri000"), "txn->path=%s",
                    NRSAFESTR(txn->path));
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_URI,
                       NR_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  rv = nr_txn_set_path(0, txn, "path_uri0000", NR_PATH_TYPE_URI,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("nr_txn_set_path succeeds", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("nr_txn_set_path sets path and ptype",
                    NR_PATH_TYPE_URI == txn->status.path_type,
                    "txn->status.path_type=%d", (int)txn->status.path_type);
  tlib_pass_if_true("nr_txn_set_path sets path and ptype",
                    0 == nr_strcmp(txn->path, "path_uri"), "txn->path=%s",
                    NRSAFESTR(txn->path));

  rv = nr_txn_set_path(0, txn, "path_custom", NR_PATH_TYPE_CUSTOM,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("higher priority name", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("higher priority name",
                    NR_PATH_TYPE_CUSTOM == txn->status.path_type,
                    "txn->status.path_type=%d", (int)txn->status.path_type);
  tlib_pass_if_true("higher priority name",
                    0 == nr_strcmp("path_custom", txn->path), "txn->path=%s",
                    NRSAFESTR(txn->path));

  rv = nr_txn_set_path(0, txn, "path_uri", NR_PATH_TYPE_URI,
                       NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("lower priority name ignored", NR_FAILURE == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("lower priority name ignored",
                    NR_PATH_TYPE_CUSTOM == txn->status.path_type,
                    "txn->status.path_type=%d", (int)txn->status.path_type);
  tlib_pass_if_true("lower priority name ignored",
                    0 == nr_strcmp("path_custom", txn->path), "txn->path=%s",
                    NRSAFESTR(txn->path));

  nr_free(txn->path);
}

static void test_set_request_uri(void) {
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nr_attribute_config_t* attribute_config;

  attribute_config = nr_attribute_config_create();
  txn->attributes = nr_attributes_create(attribute_config);

  txn->request_uri = 0;

  nr_txn_set_request_uri(0, 0);
  tlib_pass_if_true("null params", 0 == txn->request_uri, "txn->request_uri=%p",
                    txn->request_uri);

  nr_txn_set_request_uri(0, "the_uri");
  tlib_pass_if_true("null txn", 0 == txn->request_uri, "txn->request_uri=%p",
                    txn->request_uri);

  nr_txn_set_request_uri(txn, 0);
  tlib_pass_if_true("null uri", 0 == txn->request_uri, "txn->request_uri=%p",
                    txn->request_uri);

  nr_txn_set_request_uri(txn, "");
  tlib_pass_if_true("empty uri", 0 == txn->request_uri, "txn->request_uri=%p",
                    txn->request_uri);

  nr_txn_set_request_uri(txn, "the_uri");
  tlib_pass_if_true("succeeds", 0 == nr_strcmp("the_uri", txn->request_uri),
                    "txn->request_uri=%s", NRSAFESTR(txn->request_uri));

  nr_txn_set_request_uri(txn, "alpha?zip=zap");
  tlib_pass_if_true("params removed ?",
                    0 == nr_strcmp("alpha", txn->request_uri),
                    "txn->request_uri=%s", NRSAFESTR(txn->request_uri));

  nr_txn_set_request_uri(txn, "beta;zip=zap");
  tlib_pass_if_true("params removed ;",
                    0 == nr_strcmp("beta", txn->request_uri),
                    "txn->request_uri=%s", NRSAFESTR(txn->request_uri));

  nr_txn_set_request_uri(txn, "gamma#zip=zap");
  tlib_pass_if_true("params removed #",
                    0 == nr_strcmp("gamma", txn->request_uri),
                    "txn->request_uri=%s", NRSAFESTR(txn->request_uri));

  nr_attribute_config_destroy(&attribute_config);
  nr_attributes_destroy(&txn->attributes);
  nr_free(txn->request_uri);
}

static void test_record_error_worthy(void) {
  nr_status_t rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->error = 0;
  txn->options.err_enabled = 1;
  txn->status.recording = 1;

  rv = nr_txn_record_error_worthy(0, 1);
  tlib_pass_if_true("nr_txn_record_error_worthy null txn", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  txn->options.err_enabled = 0;
  rv = nr_txn_record_error_worthy(txn, 1);
  tlib_pass_if_true("nr_txn_record_error_worthy no err_enabled",
                    NR_FAILURE == rv, "rv=%d", (int)rv);
  txn->options.err_enabled = 1;

  txn->status.recording = 0;
  rv = nr_txn_record_error_worthy(txn, 1);
  tlib_pass_if_true("nr_txn_record_error_worthy no recording", NR_FAILURE == rv,
                    "rv=%d", (int)rv);
  txn->status.recording = 1;

  /* No previous error */
  rv = nr_txn_record_error_worthy(txn, 1);
  tlib_pass_if_true("nr_txn_record_error_worthy succeeds", NR_SUCCESS == rv,
                    "rv=%d", (int)rv);

  /* Previous error exists */
  txn->error
      = nr_error_create(1, "msg", "class", "[]", "my/span_id", nr_get_time());

  rv = nr_txn_record_error_worthy(txn, 0);
  tlib_pass_if_true("nr_txn_record_error_worthy lower priority",
                    NR_FAILURE == rv, "rv=%d", (int)rv);

  rv = nr_txn_record_error_worthy(txn, 2);
  tlib_pass_if_true("nr_txn_record_error_worthy succeeds", NR_SUCCESS == rv,
                    "rv=%d", (int)rv);

  nr_error_destroy(&txn->error);
}

static void test_record_error(void) {
  nrtxn_t txnv = {0};
  nrtxn_t* txn = &txnv;

  txn->options.err_enabled = 1;
  txn->options.allow_raw_exception_messages = 1;
  txn->status.recording = 1;
  /*
   * Nothing to test after these calls since no txn is provided.
   * However, we want to ensure that the stack parameter is freed.
   */
  nr_txn_record_error(NULL, 0, true, NULL, NULL, NULL);
  nr_txn_record_error(0, 2, true, "msg", "class", "[\"A\",\"B\"]");

  txn->options.err_enabled = 0;
  nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error no err_enabled", 0 == txn->error,
                    "txn->error=%p", txn->error);
  txn->options.err_enabled = 1;

  txn->status.recording = 0;
  nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error no recording", 0 == txn->error,
                    "txn->error=%p", txn->error);
  txn->status.recording = 1;

  nr_txn_record_error(txn, 2, true, 0, "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error no errmsg", 0 == txn->error,
                    "txn->error=%p", txn->error);

  nr_txn_record_error(txn, 2, true, "msg", 0, "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error no class", 0 == txn->error,
                    "txn->error=%p", txn->error);

  nr_txn_record_error(txn, 2, true, "", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error empty errmsg", 0 == txn->error,
                    "txn->error=%p", txn->error);

  nr_txn_record_error(txn, 2, true, "msg", "", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error empty class", 0 == txn->error,
                    "txn->error=%p", txn->error);

  nr_txn_record_error(txn, 2, true, "msg", "class", 0);
  tlib_pass_if_true("nr_txn_record_error no stack", 0 == txn->error,
                    "txn->error=%p", txn->error);

  /* Success when no previous error */
  nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("no previous error", 0 != txn->error, "txn->error=%p",
                    txn->error);
  tlib_pass_if_true("no previous error", 2 == nr_error_priority(txn->error),
                    "nr_error_priority (txn->error)=%d",
                    nr_error_priority(txn->error));
  tlib_pass_if_true("no previous error",
                    0 == nr_strcmp("msg", nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  /* Failure with lower priority error than existing */
  nr_txn_record_error(txn, 1, true, "newmsg", "newclass", "[]");
  tlib_pass_if_true("lower priority", 0 != txn->error, "txn->error=%p",
                    txn->error);
  tlib_pass_if_true("lower priority", 2 == nr_error_priority(txn->error),
                    "nr_error_priority (txn->error)=%d",
                    nr_error_priority(txn->error));
  tlib_pass_if_true("lower priority",
                    0 == nr_strcmp("msg", nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  /* Replace error when higher priority than existing */
  nr_txn_record_error(txn, 3, true, "newmsg", "newclass", "[\"C\",\"D\"]");
  tlib_pass_if_true("higher priority", 0 != txn->error, "txn->error=%p",
                    txn->error);
  tlib_pass_if_true("higher priority", 3 == nr_error_priority(txn->error),
                    "nr_error_priority (txn->error)=%d",
                    nr_error_priority(txn->error));
  tlib_pass_if_true("higher priority",
                    0 == nr_strcmp("newmsg", nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  txn->high_security = 1;
  nr_txn_record_error(txn, 4, true, "don't show me", "high_security",
                      "[\"C\",\"D\"]");
  tlib_pass_if_true("high security error message stripped", 0 != txn->error,
                    "txn->error=%p", txn->error);
  tlib_pass_if_true("high security error message stripped",
                    4 == nr_error_priority(txn->error),
                    "nr_error_priority (txn->error)=%d",
                    nr_error_priority(txn->error));
  tlib_pass_if_true("high security error message stripped",
                    0
                        == nr_strcmp(NR_TXN_HIGH_SECURITY_ERROR_MESSAGE,
                                     nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));
  txn->high_security = 0;

  /* Error when no span_id but we expect it. First create the environment. */
  nr_error_destroy(&txn->error);
  txn->error = 0;
  txn->options.distributed_tracing_enabled = 1;
  txn->options.span_events_enabled = 1;
  txn->distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_null("nr_txn_record_error no span_id for error", txn->error);
  txn->options.distributed_tracing_enabled = 0;
  txn->options.span_events_enabled = 0;
  nr_distributed_trace_destroy(&txn->distributed_trace);

  /* Don't replace an existing error when higher priority error comes in but
   * then encounters an error with recording the error.
   *
   * This requires a few steps:
   * 1) Setup an environment with no previously existing errors.
   * 2) Record an error with a priority = 3.
   * 3) Change the environment to that nr_txn_record_error will encounter an
   * error condition.  In this case, we are forcing span_id to be NULL.
   * 4) Attempt to record another error with priority 5.  In normal cases, this
   * should overwrite the previous error with the lower priority.
   * 5) Check that the txn->error was not destroyed.
   * 6) Check that the txn->error is the error recorded in step 2 and verify it
   * wasn't overwritten by the attempted nr_txn_record_error in step 4.
   */
  nr_error_destroy(&txn->error);
  txn->error = 0;
  nr_txn_record_error(txn, 3, true, "oldmsg", "oldclass", "[\"C\",\"D\"]");
  /* Change the environment to create an error condition.  */
  txn->options.distributed_tracing_enabled = 1;
  txn->options.span_events_enabled = 1;
  txn->distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  /*Even though it is higher priority, it should not replace the existing error
   * because of the error condition.*/
  nr_txn_record_error(txn, 5, true, "newmsg", "newclass", "[\"A\",\"B\"]");
  tlib_pass_if_not_null("nr_txn_record_error previous error is not destroyed",
                        txn->error);
  tlib_pass_if_not_null("previous error is not destroyed", txn->error);
  tlib_pass_if_int_equal("previous priority is maintained", 3,
                         nr_error_priority(txn->error));
  tlib_pass_if_str_equal("previous message is maintained", "oldmsg",
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("previous class is maintained", "oldclass",
                         nr_error_get_klass(txn->error));
  txn->options.distributed_tracing_enabled = 0;
  txn->options.span_events_enabled = 0;
  nr_distributed_trace_destroy(&txn->distributed_trace);

  nr_error_destroy(&txn->error);
}

#define test_created_txn(...) \
  test_created_txn_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_created_txn_fn(const char* testname,
                                nrtxn_t* rv,
                                nrtxnopt_t* correct,
                                const char* file,
                                int line) {
  const char* guid = nr_txn_get_guid(rv);
  nrtxnopt_t* opts = &rv->options;

  /*
   * Test : GUID Created
   */
  tlib_pass_if_not_null(testname, guid);
  tlib_pass_if_int_equal(testname, NR_GUID_SIZE, nr_strlen(guid));

  /*
   * Test : Root segment.
   */
  tlib_pass_if_not_null(testname, rv->segment_root);
  tlib_pass_if_time_equal(testname, 0, rv->segment_root->start_time);
  tlib_pass_if_int_equal(testname, 0, rv->segment_root->async_context);

  /*
   * Test : Segment slab allocator.
   */
  tlib_pass_if_not_null(testname, rv->segment_slab);

  /*
   * Test : Structures allocated
   */
  test_pass_if_true(testname, 0 != rv->trace_strings, "rv->trace_strings=%p",
                    rv->trace_strings);
  test_pass_if_true(testname, 0 != rv->scoped_metrics, "rv->scoped_metrics=%p",
                    rv->scoped_metrics);
  test_pass_if_true(testname, 0 != rv->unscoped_metrics,
                    "rv->unscoped_metrics=%p", rv->unscoped_metrics);
  test_pass_if_true(testname, 0 != rv->intrinsics, "rv->intrinsics=%p",
                    rv->intrinsics);
  test_pass_if_true(testname, 0 != rv->attributes, "rv->attributes=%p",
                    rv->attributes);

  /*
   * Test : Status
   */
  test_pass_if_true(testname, 0 == rv->status.ignore_apdex,
                    "rv->status.ignore_apdex=%d", rv->status.ignore_apdex);
  test_pass_if_true(
      testname,
      rv->options.request_params_enabled == rv->options.request_params_enabled,
      "rv->options.request_params_enabled=%d "
      "rv->options.request_params_enabled=%d",
      rv->options.request_params_enabled, rv->options.request_params_enabled);
  test_pass_if_true(testname, 1 == rv->status.recording,
                    "rv->status.recording=%d", rv->status.recording);

  if (rv->options.cross_process_enabled) {
    test_pass_if_true(
        testname, NR_STATUS_CROSS_PROCESS_START == rv->status.cross_process,
        "rv->status.cross_process=%d", (int)rv->status.cross_process);
  } else {
    test_pass_if_true(
        testname, NR_STATUS_CROSS_PROCESS_DISABLED == rv->status.cross_process,
        "rv->status.cross_process=%d", (int)rv->status.cross_process);
  }

  /*
   * Test : Transaction type bits
   */
  tlib_pass_if_uint_equal(testname, 0, rv->type);

  /*
   * Test : Options
   */
  test_pass_if_true(
      testname,
      (bool)opts->analytics_events_enabled
          == (bool)correct->analytics_events_enabled,
      "opts->analytics_events_enabled=%d correct->analytics_events_enabled=%d",
      opts->analytics_events_enabled, correct->analytics_events_enabled);
  test_pass_if_true(
      testname,
      (bool)opts->custom_events_enabled == (bool)correct->custom_events_enabled,
      "opts->custom_events_enabled=%d correct->custom_events_enabled=%d",
      opts->custom_events_enabled, correct->custom_events_enabled);
  test_pass_if_true(
      testname,
      (bool)opts->error_events_enabled == (bool)correct->error_events_enabled,
      "opts->error_events_enabled=%d correct->error_events_enabled=%d",
      opts->error_events_enabled, correct->error_events_enabled);
  test_pass_if_true(
      testname,
      (bool)opts->span_events_enabled == (bool)correct->span_events_enabled,
      "opts->span_events_enabled=%d correct->span_events_enabled=%d",
      opts->span_events_enabled, correct->span_events_enabled);
  test_pass_if_true(
      testname, opts->synthetics_enabled == correct->synthetics_enabled,
      "opts->synthetics_enabled=%d correct->synthetics_enabled=%d",
      opts->synthetics_enabled, correct->synthetics_enabled);
  test_pass_if_true(testname, opts->err_enabled == correct->err_enabled,
                    "opts->err_enabled=%d correct->err_enabled=%d",
                    opts->err_enabled, correct->err_enabled);
  test_pass_if_true(
      testname, opts->request_params_enabled == correct->request_params_enabled,
      "opts->request_params_enabled=%d correct->request_params_enabled=%d",
      opts->request_params_enabled, correct->request_params_enabled);
  test_pass_if_true(testname, opts->autorum_enabled == correct->autorum_enabled,
                    "opts->autorum_enabled=%d correct->autorum_enabled=%d",
                    opts->autorum_enabled, correct->autorum_enabled);
  test_pass_if_true(testname, opts->tt_enabled == correct->tt_enabled,
                    "opts->tt_enabled=%d correct->tt_enabled=%d",
                    opts->tt_enabled, correct->tt_enabled);
  test_pass_if_true(testname, opts->ep_enabled == correct->ep_enabled,
                    "opts->ep_enabled=%d correct->ep_enabled=%d",
                    opts->ep_enabled, correct->ep_enabled);
  test_pass_if_true(testname, opts->tt_recordsql == correct->tt_recordsql,
                    "opts->tt_recordsql=%d correct->tt_recordsql=%d",
                    (int)opts->tt_recordsql, (int)correct->tt_recordsql);
  test_pass_if_true(testname, opts->tt_slowsql == correct->tt_slowsql,
                    "opts->tt_slowsql=%d correct->tt_slowsql=%d",
                    opts->tt_slowsql, correct->tt_slowsql);
  test_pass_if_true(testname, opts->apdex_t == correct->apdex_t,
                    "opts->apdex_t=" NR_TIME_FMT
                    " correct->apdex_t=" NR_TIME_FMT,
                    opts->apdex_t, correct->apdex_t);
  test_pass_if_true(testname, opts->tt_threshold == correct->tt_threshold,
                    "opts->tt_threshold=" NR_TIME_FMT
                    " correct->tt_threshold=" NR_TIME_FMT,
                    opts->tt_threshold, correct->tt_threshold);
  test_pass_if_true(testname, opts->tt_is_apdex_f == correct->tt_is_apdex_f,
                    "opts->tt_is_apdex_f=%d correct->tt_is_apdex_f=%d",
                    opts->tt_is_apdex_f, correct->tt_is_apdex_f);
  test_pass_if_true(testname, opts->ep_threshold == correct->ep_threshold,
                    "opts->ep_threshold=" NR_TIME_FMT
                    " correct->ep_threshold=" NR_TIME_FMT,
                    opts->ep_threshold, correct->ep_threshold);
  test_pass_if_true(testname, opts->ss_threshold == correct->ss_threshold,
                    "opts->ss_threshold=" NR_TIME_FMT
                    " correct->ss_threshold=" NR_TIME_FMT,
                    opts->ss_threshold, correct->ss_threshold);
  test_pass_if_true(
      testname, opts->cross_process_enabled == correct->cross_process_enabled,
      "opts->cross_process_enabled=%d correct->cross_process_enabled=%d",
      opts->cross_process_enabled, correct->cross_process_enabled);
  test_pass_if_true(testname, opts->max_segments == correct->max_segments,
                    "opts->max_segments=%zu correct->max_segments=%zu",
                    opts->max_segments, correct->max_segments);
}

static void test_default_trace_id(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  const char* txnid;
  char paddedid[33] = "0000000000000000";

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;
  nr_memset(&opts, 0, sizeof(opts));

  txn = nr_txn_begin(&app, &opts, NULL);
  txnid = nr_txn_get_guid(txn);

  tlib_fail_if_null("txnid", txnid);
  nr_strcat(paddedid, txnid);
  tlib_pass_if_str_equal(
      "txnid=traceid", paddedid,
      nr_distributed_trace_get_trace_id(txn->distributed_trace));

  nr_txn_destroy(&txn);
}

static void test_root_segment_priority(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  uint32_t priority;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;
  nr_memset(&opts, 0, sizeof(opts));

  txn = nr_txn_begin(&app, &opts, NULL);

  tlib_fail_if_null("txn", txn);
  tlib_fail_if_null("root segment", txn->segment_root);

  priority = txn->segment_root->priority;

  tlib_pass_if_true("root segment priority",
                    priority & NR_SEGMENT_PRIORITY_ROOT, "priority=0x%08x",
                    priority);

  nr_txn_destroy(&txn);
}

static void test_begin_bad_params(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  nr_attribute_config_t* config;

  config = nr_attribute_config_create();

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;
  nr_memset(&opts, 0, sizeof(opts));

  txn = nr_txn_begin(0, 0, config);
  tlib_pass_if_true("null params", 0 == txn, "txn=%p", txn);

  txn = nr_txn_begin(0, &opts, config);
  tlib_pass_if_true("null app", 0 == txn, "txn=%p", txn);

  app.state = NR_APP_INVALID;
  txn = nr_txn_begin(&app, &opts, config);
  tlib_pass_if_true("invalid app", 0 == txn, "txn=%p", txn);
  app.state = NR_APP_OK;

  txn = nr_txn_begin(&app, NULL, config);
  tlib_pass_if_true("NULL options", 0 == txn, "txn=%p", txn);

  txn = nr_txn_begin(&app, &opts, config);
  tlib_pass_if_true("tests valid", 0 != txn, "txn=%p", txn);

  nr_txn_destroy(&txn);
  nr_attribute_config_destroy(&config);
}

static void test_begin(void) {
  nrtxn_t* rv;
  nrtxnopt_t optsv;
  nrtxnopt_t* opts = &optsv;
  nrtxnopt_t correct;
  nrapp_t appv = {.info = {0}};
  nrapp_t* app = &appv;
  nr_attribute_config_t* attribute_config;
  char* json;

  attribute_config = nr_attribute_config_create();

  opts->custom_events_enabled = 109;
  opts->error_events_enabled = 27;
  opts->synthetics_enabled = 110;
  opts->analytics_events_enabled = 108;
  opts->span_events_enabled = 112;
  opts->err_enabled = 2;
  opts->request_params_enabled = 3;
  opts->autorum_enabled = 5;
  opts->tt_enabled = 7;
  opts->ep_enabled = 8;
  opts->tt_recordsql = NR_SQL_OBFUSCATED;
  opts->tt_slowsql = 10;
  opts->apdex_t = 11; /* Should be unused */
  opts->tt_threshold = 12;
  opts->tt_is_apdex_f = 13;
  opts->ep_threshold = 14;
  opts->ss_threshold = 15;
  opts->cross_process_enabled = 22;
  opts->max_segments = 0;
  opts->span_queue_batch_size = 1000;
  opts->span_queue_batch_timeout = 1 * NR_TIME_DIVISOR;

  app->rnd = nr_random_create();
  nr_random_seed(app->rnd, 345345);
  app->info.high_security = 0;
  app->connect_reply = nro_new_hash();
  app->security_policies = nro_new_hash();
  nro_set_hash_boolean(app->connect_reply, "collect_errors", 1);
  nro_set_hash_boolean(app->connect_reply, "collect_traces", 1);
  nro_set_hash_double(app->connect_reply, "apdex_t", 0.6);
  nro_set_hash_string(app->connect_reply, "js_agent_file",
                      "js-agent.newrelic.com\\/nr-213.min.js");
  nro_set_hash_string(app->connect_reply, "entity_guid", "00abcdef");
  app->state = NR_APP_OK;

  app->agent_run_id = nr_strdup("12345678");
  app->host_name = nr_strdup("host_name");
  app->entity_name = nr_strdup("App Name");
  app->info.license = nr_strdup("1234567890123456789012345678901234567890");
  app->info.host_display_name = nr_strdup("foo_host");
  app->info.security_policies_token = nr_strdup("");

  nr_memset(&app->harvest, 0, sizeof(nr_app_harvest_t));
  app->harvest.frequency = 60;
  app->harvest.target_transactions_per_cycle = 10;
  app->limits = default_app_limits();

  /*
   * Test : Options provided.
   */
  correct.custom_events_enabled = 109;
  correct.error_events_enabled = 27;
  correct.synthetics_enabled = 110;
  correct.err_enabled = 2;
  correct.request_params_enabled = 3;
  correct.autorum_enabled = 5;
  correct.analytics_events_enabled = 108;
  correct.span_events_enabled = 112;
  correct.tt_enabled = 7;
  correct.ep_enabled = 8;
  correct.tt_recordsql = NR_SQL_OBFUSCATED;
  correct.tt_slowsql = 10;
  correct.apdex_t = 600 * NR_TIME_DIVISOR_MS; /* From app */
  correct.tt_threshold = 4 * correct.apdex_t;
  correct.tt_is_apdex_f = 13;
  correct.ep_threshold = 14;
  correct.ss_threshold = 15;
  correct.cross_process_enabled = 22;
  correct.max_segments = 0;

  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("options provided", rv, &correct);
  json = nr_attributes_debug_json(rv->attributes);
  tlib_pass_if_str_equal("display host attribute created", json,
                         "{\"user\":[],\"agent\":["
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"host.displayName\",\"value\":\"foo_host\"}]}");
  nr_free(json);
  nr_txn_destroy(&rv);

  /*
   * Test : Options provided.  tt_is_apdex_f = 0
   */
  opts->tt_is_apdex_f = 0;
  correct.tt_threshold = 12;
  correct.tt_is_apdex_f = 0;

  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("tt is not apdex_f", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off traces
   */
  nro_set_hash_boolean(app->connect_reply, "collect_traces", 0);
  correct.tt_enabled = 0;
  correct.ep_enabled = 0;
  correct.tt_slowsql = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off traces", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off errors
   */
  nro_set_hash_boolean(app->connect_reply, "collect_errors", 0);
  correct.err_enabled = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off errors", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off analytics events
   */
  nro_set_hash_boolean(app->connect_reply, "collect_analytics_events", 0);
  correct.analytics_events_enabled = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off analytics events", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off custom events.
   */
  nro_set_hash_boolean(app->connect_reply, "collect_custom_events", 0);
  correct.custom_events_enabled = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off custom events", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off error events.
   */
  nro_set_hash_boolean(app->connect_reply, "collect_error_events", 0);
  correct.error_events_enabled = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off error events", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : App turns off span events
   */
  nro_set_hash_boolean(app->connect_reply, "collect_span_events", 0);
  correct.span_events_enabled = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  test_created_txn("app turns off span events", rv, &correct);
  nr_txn_destroy(&rv);

  /*
   * Test : High security off
   */
  app->info.high_security = 0;
  rv = nr_txn_begin(app, opts, attribute_config);
  tlib_pass_if_int_equal("high security off", 0, rv->high_security);
  nr_txn_destroy(&rv);

  /*
   * Test : High Security On
   */
  app->info.high_security = 1;
  rv = nr_txn_begin(app, opts, attribute_config);
  tlib_pass_if_int_equal("app local high security copied to txn", 1,
                         rv->high_security);
  nr_txn_destroy(&rv);
  app->info.high_security = 0;

  /*
   * Test : CPU usage populated on create
   */
  rv = nr_txn_begin(app, opts, attribute_config);
  /*
   * It is tempting to think that the process has already
   * incurred some user and system time at the start.
   * This may not be true if getrusage() is lieing to us,
   * or if the amount of time that has run is less than
   * the clock threshhold, or there are VM/NTP time issues, etc.
   *
   * However, since we haven't stopped the txn yet, the END
   * usage should definately be 0.
   */
  tlib_pass_if_true("user_cpu[1]", 0 == rv->user_cpu[NR_CPU_USAGE_END],
                    "user_cpu[1]=" NR_TIME_FMT, rv->user_cpu[NR_CPU_USAGE_END]);
  tlib_pass_if_true("sys_cpu[1]", 0 == rv->sys_cpu[NR_CPU_USAGE_END],
                    "sys_cpu[1]=" NR_TIME_FMT, rv->sys_cpu[NR_CPU_USAGE_END]);
  nr_txn_destroy(&rv);

  /*
   * Test : App name is populated in the new transaction.
   */
  rv = nr_txn_begin(app, opts, attribute_config);
  tlib_pass_if_str_equal("primary_app_name", "App Name", rv->primary_app_name);
  nr_txn_destroy(&rv);

  /*
   * Test : Connect reply for DT
   */
  nro_set_hash_string(app->connect_reply, "trusted_account_key", "1");
  nro_set_hash_string(app->connect_reply, "primary_application_id", "2");
  nro_set_hash_string(app->connect_reply, "account_id", "3");
  rv = nr_txn_begin(app, opts, attribute_config);
  tlib_pass_if_str_equal(
      "connect response", "1",
      nr_distributed_trace_get_trusted_key(rv->distributed_trace));
  tlib_pass_if_str_equal(
      "connect response", "2",
      nr_distributed_trace_get_app_id(rv->distributed_trace));
  tlib_pass_if_str_equal(
      "connect response", "3",
      nr_distributed_trace_get_account_id(rv->distributed_trace));
  nr_txn_destroy(&rv);

  /*
   * Test : Application disables events.
   */
  app->limits = (nr_app_limits_t){
      .analytics_events = 0,
      .custom_events = 0,
      .error_events = 0,
      .span_events = 0,
  };
  rv = nr_txn_begin(app, opts, attribute_config);
  tlib_pass_if_int_equal("analytics_events_enabled", 0,
                         rv->options.analytics_events_enabled);
  tlib_pass_if_int_equal("custom_events_enabled", 0,
                         rv->options.custom_events_enabled);
  tlib_pass_if_int_equal("error_events_enabled", 0,
                         rv->options.error_events_enabled);
  tlib_pass_if_int_equal("span_events_enabled", 0,
                         rv->options.span_events_enabled);
  nr_txn_destroy(&rv);

  nr_free(app->agent_run_id);
  nr_free(app->host_name);
  nr_free(app->entity_name);
  nr_free(app->info.appname);
  nr_free(app->info.license);
  nr_free(app->info.host_display_name);
  nr_free(app->info.security_policies_token);
  nro_delete(app->connect_reply);
  nro_delete(app->security_policies);
  nr_attribute_config_destroy(&attribute_config);
  nr_random_destroy(&app->rnd);
}

static int metric_exists(nrmtable_t* metrics, const char* name) {
  nrmetric_t* m = nrm_find(metrics, name);

  if ((0 == m) || (INT_MAX == nrm_min(m))) {
    return 0;
  }

  return 1;
}

static int metric_total_is_nonzero(nrmtable_t* metrics, const char* name) {
  nrmetric_t* m = nrm_find(metrics, name);

  if (NULL == m) {
    return -1;
  }

  return 0 != m->mdata[NRM_TOTAL];
}

static nrtxn_t* create_full_txn_and_reset(nrapp_t* app) {
  nrtxn_t* txn;

  /*
   * Create the Transaction
   */
  txn = nr_txn_begin(app, &nr_txn_test_options, 0);
  tlib_pass_if_not_null("nr_txn_begin succeeds", txn);
  if (0 == txn) {
    return txn;
  }

  txn->status.http_x_start = txn->abs_start_time - 100;
  txn->high_security = 0;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;

  txn->abs_start_time
      -= 5
         * (txn->options.tt_threshold + txn->options.ep_threshold
            + txn->options.ss_threshold);

  /*
   * Add an Error
   */
  nr_txn_record_error(txn, 1, true, "my_errmsg", "my_errclass",
                      "[\"Zink called on line 123 of script.php\","
                      "\"Zonk called on line 456 of hack.php\"]");
  tlib_pass_if_true("error added", 0 != txn->error, "txn->error=%p",
                    txn->error);

  /*
   * Add some segments.
   */
  {
    nr_segment_t* seg = nr_segment_start(txn, NULL, NULL);
    seg->start_time = 1 * NR_TIME_DIVISOR;
    seg->stop_time = 2 * NR_TIME_DIVISOR;
    seg->type = NR_SEGMENT_DATASTORE;
    seg->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
    seg->typed_attributes->datastore.sql = nr_strdup("SELECT * from TABLE;");
    seg->typed_attributes->datastore.component = nr_strdup("MySql");
    nr_segment_end(&seg);
  }

  {
    nr_segment_t* seg = nr_segment_start(txn, NULL, NULL);
    seg->start_time = 3 * NR_TIME_DIVISOR;
    seg->stop_time = 4 * NR_TIME_DIVISOR;
    seg->type = NR_SEGMENT_DATASTORE;
    seg->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
    nr_segment_end(&seg);
  }

  {
    nr_segment_t* seg = nr_segment_start(txn, NULL, NULL);
    seg->start_time = 5 * NR_TIME_DIVISOR;
    seg->stop_time = 6 * NR_TIME_DIVISOR;
    seg->type = NR_SEGMENT_DATASTORE;
    seg->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
    nr_segment_end(&seg);
  }

  {
    nr_segment_t* seg = nr_segment_start(txn, NULL, NULL);
    seg->start_time = 7 * NR_TIME_DIVISOR;
    seg->stop_time = 8 * NR_TIME_DIVISOR;
    seg->type = NR_SEGMENT_EXTERNAL;
    seg->typed_attributes = nr_zalloc(sizeof(nr_segment_typed_attributes_t));
    seg->typed_attributes->external.uri = nr_strdup("newrelic.com");
    nr_segment_end(&seg);
  }

  tlib_pass_if_size_t_equal("four segments added", 4, txn->segment_count);

  /*
   * Set the Path
   */
  nr_txn_set_path(0, txn, "zap.php", NR_PATH_TYPE_URI, NR_NOT_OK_TO_OVERWRITE);
  tlib_pass_if_true("path set", 0 == nr_strcmp("zap.php", txn->path),
                    "txn->path=%s", NRSAFESTR(txn->path));

  return txn;
}

#define test_end_testcase(...) \
  test_end_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_end_testcase_fn(const char* testname,
                                 const nrtxn_t* txn,
                                 int expected_apdex_metrics,
                                 int expected_error_metrics,
                                 int expected_queuetime_metric,
                                 int expected_nonzero_total_time,
                                 const char* file,
                                 int line) {
  int txndata_apdex_metrics = 0;
  int txndata_error_metrics = 0;
  int txndata_queuetime_metric = 0;
  nrtime_t txndata_root_stop_time_when = 0;

  tlib_pass_if_true_f(testname, 0 != txn, file, line, "0 != txn", "txn=%p",
                      txn);

  if (0 == txn) {
    return;
  }

  tlib_pass_if_true_f(testname, 0 == txn->status.recording, file, line,
                      "0 == txn->status.recording", "txn->status.recording=%d",
                      txn->status.recording);

  txndata_apdex_metrics = metric_exists(txn->unscoped_metrics, "Apdex");
  txndata_error_metrics = metric_exists(txn->unscoped_metrics, "Errors/all");
  txndata_queuetime_metric
      = metric_exists(txn->unscoped_metrics, "WebFrontend/QueueTime");
  txndata_root_stop_time_when = txn->segment_root->stop_time;

  if (txn->unscoped_metrics) {
    int metric_nonzero_code;
    int metric_exists_code;

    /*
     * Test : Duration Metric Created
     */
    if (1 == txn->status.background) {
      metric_exists_code
          = metric_exists(txn->unscoped_metrics, "OtherTransaction/all");

      /* If OtherTransactionTotalTime exists, make sure it's nonzero */
      metric_nonzero_code = metric_total_is_nonzero(
          txn->unscoped_metrics, "OtherTransactionTotalTime");

    } else {
      metric_exists_code
          = metric_exists(txn->unscoped_metrics, "WebTransaction");

      /* If WebTransactionTotalTime exists, make sure it's nonzero */
      metric_nonzero_code = metric_total_is_nonzero(txn->unscoped_metrics,
                                                    "WebTransactionTotalTime");
    }
    tlib_pass_if_false_f(testname, metric_nonzero_code == -1, file, line,
                         "metric_nonzero_code == -1",
                         "metric_nonzero_code=%d txn->status.background=%d",
                         metric_nonzero_code, txn->status.background);
    tlib_pass_if_true_f(
        testname, metric_nonzero_code == expected_nonzero_total_time, file,
        line, "metric_nonzero_code == expected_nonzero_total_time",
        "metric_nonzero_code=%d txn->status.background=%d", metric_nonzero_code,
        txn->status.background);
    tlib_pass_if_true_f(testname, 1 == metric_exists_code, file, line,
                        "1 == metric_exists_code",
                        "metric_exists_code=%d txn->status.background=%d",
                        metric_exists_code, txn->status.background);
  }
  tlib_pass_if_true_f(testname, txndata_apdex_metrics == expected_apdex_metrics,
                      file, line,
                      "txndata_apdex_metrics == expected_apdex_metrics",
                      "txndata_apdex_metrics=%d expected_apdex_metrics=%d",
                      txndata_apdex_metrics, expected_apdex_metrics);
  tlib_pass_if_true_f(testname, txndata_error_metrics == expected_error_metrics,
                      file, line,
                      "txndata_error_metrics == expected_error_metrics",
                      "txndata_error_metrics=%d expected_error_metrics=%d",
                      txndata_error_metrics, expected_error_metrics);
  tlib_pass_if_true_f(
      testname, txndata_queuetime_metric == expected_queuetime_metric, file,
      line, "txndata_queuetime_metric == expected_queuetime_metric",
      "txndata_queuetime_metric=%d expected_queuetime_metric=%d",
      txndata_queuetime_metric, expected_queuetime_metric);
  tlib_pass_if_true_f(testname, 0 != txndata_root_stop_time_when, file, line,
                      "0 != txndata_root_stop_time_when",
                      "txndata_root_stop_time_when=" NR_TIME_FMT,
                      txndata_root_stop_time_when);
}

static void test_end(void) {
  nrtxn_t* txn = 0;
  nrapp_t appv = {.info = {0}};
  nrapp_t* app = &appv;
  nrobj_t* rules_ob;
  nrtime_t duration;
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();

  app->rnd = nr_random_create();
  nr_random_seed(app->rnd, 345345);
  app->info.high_security = 0;
  app->state = NR_APP_OK;
  nrt_mutex_init(&app->app_lock, 0);
  rules_ob = nro_create_from_json(test_rules);
  app->url_rules
      = nr_rules_create_from_obj(nro_get_hash_array(rules_ob, "url_rules", 0));
  app->txn_rules
      = nr_rules_create_from_obj(nro_get_hash_array(rules_ob, "txn_rules", 0));
  nro_delete(rules_ob);
  app->segment_terms = 0;
  app->connect_reply = nro_new_hash();
  app->security_policies = nro_new_hash();
  nro_set_hash_boolean(app->connect_reply, "collect_traces", 1);
  nro_set_hash_boolean(app->connect_reply, "collect_errors", 1);
  nro_set_hash_double(app->connect_reply, "apdex_t", 0.5);
  app->agent_run_id = nr_strdup("12345678");
  app->info.appname = nr_strdup("App Name;Foo;Bar");
  app->info.license = nr_strdup("1234567890123456789012345678901234567890");
  app->info.host_display_name = nr_strdup("foo_host");
  app->info.security_policies_token = nr_strdup("");

  nr_memset(&app->harvest, 0, sizeof(nr_app_harvest_t));
  app->harvest.frequency = 60;
  app->harvest.target_transactions_per_cycle = 10;

  p->txns_app = app;

  /*
   * Test : Bad Parameters
   */
  nr_txn_end(0); /* Don't blow up */

  /*
   * Test : Ignore transaction situations
   */
  txn = create_full_txn_and_reset(app);
  txn->status.ignore = 1;
  nr_txn_end(txn);
  tlib_pass_if_true("txn->status.ignore", 0 == txn->status.recording,
                    "txn->status.recording=%d", txn->status.recording);
  nr_txn_destroy(&txn);

  txn = create_full_txn_and_reset(app);
  nr_txn_set_path(0, txn, "/ignore_path.php", NR_PATH_TYPE_CUSTOM,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_txn_end(txn);
  tlib_pass_if_true("ignored by rules", 0 == txn->status.recording,
                    "txn->status.recording=%d", txn->status.recording);
  tlib_pass_if_true("ignored by rules", 1 == txn->status.ignore,
                    "txn->status.ignore=%d", txn->status.ignore);
  nr_txn_destroy(&txn);

  /*
   * Test : Complete Transaction sent to cmd_txndata
   */
  txn = create_full_txn_and_reset(app);
  nr_txn_end(txn);

  test_end_testcase("full txn to cmd_txndata", txn, 1 /* apdex */, 1 /* error*/,
                    1 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Synthetics transaction
   */
  txn = create_full_txn_and_reset(app);
  txn->synthetics = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");
  nr_txn_end(txn);
  test_end_testcase("full txn to cmd_txndata", txn, 1 /* apdex */, 1 /* error*/,
                    1 /* queue */, 1 /* total time */);
  tlib_pass_if_str_equal(
      "synthetics intrinsics", "a",
      nro_get_hash_string(txn->intrinsics, "synthetics_resource_id", NULL));
  nr_txn_destroy(&txn);

  /*
   * Test : No error metrics when no error
   */
  txn = create_full_txn_and_reset(app);
  nr_error_destroy(&txn->error);
  nr_txn_end(txn);
  test_end_testcase("no error", txn, 1 /* apdex */, 0 /* error*/, 1 /* queue */,
                    1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Background taks means no apdex metrics and no queuetime metric
   */
  txn = create_full_txn_and_reset(app);
  txn->status.background = 1;
  nr_txn_end(txn);
  test_end_testcase("background task", txn, 0 /* apdex */, 1 /* error*/,
                    0 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Ignore Apdex
   */
  txn = create_full_txn_and_reset(app);
  txn->status.ignore_apdex = 1;
  nr_txn_end(txn);
  test_end_testcase("ignore apdex", txn, 0 /* apdex */, 1 /* error*/,
                    1 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : No Queue Time
   */
  txn = create_full_txn_and_reset(app);
  txn->status.http_x_start = 0;
  nr_txn_end(txn);
  test_end_testcase("no queue time", txn, 1 /* apdex */, 1 /* error*/,
                    0 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Start time in future
   */
  txn = create_full_txn_and_reset(app);
  txn->segment_root->start_time = nr_get_time() + 999999;
  nr_txn_end(txn);
  test_end_testcase("stop time in future", txn, 1 /* apdex */, 1 /* error*/,
                    1 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Txn Already Halted
   */
  txn = create_full_txn_and_reset(app);
  nr_txn_end(txn);
  nr_txn_end(txn);
  test_end_testcase("halted", txn, 1 /* apdex */, 1 /* error*/, 1 /* queue */,
                    1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Missing Path
   */
  txn = create_full_txn_and_reset(app);
  nr_free(txn->name);
  nr_txn_end(txn);
  test_end_testcase("missing path", txn, 1 /* apdex */, 1 /* error*/,
                    1 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : No Metric Table
   */
  txn = create_full_txn_and_reset(app);
  nrm_table_destroy(&txn->unscoped_metrics);
  nr_txn_end(txn);
  test_end_testcase("no metric table", txn, 0 /* apdex */, 0 /* error*/,
                    0 /* queue */, 1 /* total time */);
  nr_txn_destroy(&txn);

  /*
   * Test : Transaction is manually retimed
   */
  txn = create_full_txn_and_reset(app);
  nr_txn_set_timing(txn, 5000000, 1000000);
  nrm_table_destroy(&txn->unscoped_metrics);
  nr_txn_end(txn);
  test_end_testcase("manually retimed", txn, 0 /* apdex */, 0 /* error*/,
                    0 /* queue */, 1 /* total time */);
  duration = nr_txn_duration(txn);
  tlib_pass_if_time_equal("duration is manually retimed", duration, 1000000);
  nr_txn_destroy(&txn);

  nr_random_destroy(&app->rnd);
  nr_rules_destroy(&app->url_rules);
  nr_rules_destroy(&app->txn_rules);
  nrt_mutex_destroy(&app->app_lock);
  nro_delete(app->connect_reply);
  nro_delete(app->security_policies);
  nr_free(app->agent_run_id);
  nr_free(app->info.appname);
  nr_free(app->info.license);
  nr_free(app->info.host_display_name);
  nr_free(app->info.security_policies_token);
}

static void test_should_force_persist(void) {
  int should_force_persist;
  nrtxn_t txn;

  txn.status.has_inbound_record_tt = 0;
  txn.status.has_outbound_record_tt = 0;

  should_force_persist = nr_txn_should_force_persist(0);
  tlib_pass_if_true("null txn", 0 == should_force_persist,
                    "should_force_persist=%d", should_force_persist);

  should_force_persist = nr_txn_should_force_persist(&txn);
  tlib_pass_if_true("nope", 0 == should_force_persist,
                    "should_force_persist=%d", should_force_persist);

  txn.status.has_inbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist(&txn);
  tlib_pass_if_true("has_inbound_record_tt", 1 == should_force_persist,
                    "should_force_persist=%d", should_force_persist);
  txn.status.has_inbound_record_tt = 0;

  txn.status.has_outbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist(&txn);
  tlib_pass_if_true("has_outbound_record_tt", 1 == should_force_persist,
                    "should_force_persist=%d", should_force_persist);
  txn.status.has_outbound_record_tt = 0;

  txn.status.has_inbound_record_tt = 1;
  txn.status.has_outbound_record_tt = 1;
  should_force_persist = nr_txn_should_force_persist(&txn);
  tlib_pass_if_true("has everything", 1 == should_force_persist,
                    "should_force_persist=%d", should_force_persist);
}

static void test_set_as_background_job(void) {
  nrtxn_t txn;
  char* json;

  txn.status.path_is_frozen = 0;
  txn.status.background = 0;
  txn.unscoped_metrics = NULL;

  /* Don't blow up */
  nr_txn_set_as_background_job(0, 0);

  txn.status.path_is_frozen = 1;
  txn.unscoped_metrics = nrm_table_create(0);
  nr_txn_set_as_background_job(&txn, 0);
  tlib_pass_if_int_equal("can't change background after path frozen", 0,
                         txn.status.background);
  json = nr_metric_table_to_daemon_json(txn.unscoped_metrics);
  tlib_pass_if_str_equal(
      "supportability metric created", json,
      "[{\"name\":\"Supportability\\/background_status_change_prevented\","
      "\"data\":[1,0.00000,0.00000,0.00000,0.00000,0.00000],\"forced\":true}]");
  nr_free(json);
  nrm_table_destroy(&txn.unscoped_metrics);
  txn.status.path_is_frozen = 0;

  txn.unscoped_metrics = nrm_table_create(0);
  nr_txn_set_as_background_job(&txn, 0);
  tlib_pass_if_int_equal("change background status success", 1,
                         txn.status.background);
  tlib_pass_if_int_equal("no supportability metric created", 0,
                         nrm_table_size(txn.unscoped_metrics));
  nrm_table_destroy(&txn.unscoped_metrics);
}

static void test_set_as_web_transaction(void) {
  nrtxn_t txn;
  char* json;

  txn.status.path_is_frozen = 0;
  txn.status.background = 1;
  txn.unscoped_metrics = NULL;

  /* Don't blow up */
  nr_txn_set_as_web_transaction(0, 0);

  txn.status.path_is_frozen = 1;
  txn.unscoped_metrics = nrm_table_create(0);
  nr_txn_set_as_web_transaction(&txn, 0);
  tlib_pass_if_int_equal("can't change background after path frozen", 1,
                         txn.status.background);
  json = nr_metric_table_to_daemon_json(txn.unscoped_metrics);
  tlib_pass_if_str_equal(
      "supportability metric created", json,
      "[{\"name\":\"Supportability\\/background_status_change_prevented\","
      "\"data\":[1,0.00000,0.00000,0.00000,0.00000,0.00000],\"forced\":true}]");
  nr_free(json);
  nrm_table_destroy(&txn.unscoped_metrics);
  txn.status.path_is_frozen = 0;

  txn.unscoped_metrics = nrm_table_create(0);
  nr_txn_set_as_web_transaction(&txn, 0);
  tlib_pass_if_int_equal("change background status success", 0,
                         txn.status.background);
  tlib_pass_if_int_equal("no supportability metric created", 0,
                         nrm_table_size(txn.unscoped_metrics));
  nrm_table_destroy(&txn.unscoped_metrics);
}

static void test_set_http_status(void) {
  nrtxn_t txn;
  nrobj_t* obj;
  char* json;

  txn.status.background = 0;
  txn.attributes = nr_attributes_create(0);

  /*
   * Bad params, don't blow up!
   */
  nr_txn_set_http_status(0, 0);
  nr_txn_set_http_status(0, 503);

  nr_txn_set_http_status(&txn, 0);
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_true("zero http code", 0 == obj, "obj=%p", obj);

  txn.status.background = 1;
  nr_txn_set_http_status(&txn, 503);
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_true("background task", 0 == obj, "obj=%p", obj);
  txn.status.background = 0;

  nr_txn_set_http_status(&txn, 503);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "success",
      0
          == nr_strcmp("{"
                       "\"user\":[],"
                       "\"agent\":["
                       "{"
                       "\"dests\":[\"event\",\"trace\",\"error\"],"
                       "\"key\":\"http.statusCode\",\"value\":503"
                       "},"
                       "{"
                       "\"dests\":[\"event\",\"trace\",\"error\"],"
                       "\"key\":\"response.statusCode\",\"value\":503"
                       "},"
                       "{"
                       "\"dests\":[\"event\",\"trace\",\"error\"],"
                       "\"key\":\"httpResponseCode\",\"value\":\"503\""
                       "}"
                       "]"
                       "}",
                       json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_attributes_destroy(&txn.attributes);
}

static void test_add_user_custom_parameter(void) {
  nrtxn_t txn = {0};
  nrobj_t* obj = nro_new_int(123);
  nr_status_t st;
  nrobj_t* out;

  txn.attributes = nr_attributes_create(0);
  txn.options.custom_parameters_enabled = 1;
  txn.high_security = 0;

  st = nr_txn_add_user_custom_parameter(NULL, NULL, NULL);
  tlib_pass_if_status_failure("null params", st);

  st = nr_txn_add_user_custom_parameter(NULL, "my_key", obj);
  tlib_pass_if_status_failure("null txn", st);

  st = nr_txn_add_user_custom_parameter(&txn, NULL, obj);
  tlib_pass_if_status_failure("null key", st);

  st = nr_txn_add_user_custom_parameter(&txn, "my_key", NULL);
  tlib_pass_if_status_failure("null obj", st);

  txn.high_security = 1;
  st = nr_txn_add_user_custom_parameter(&txn, "my_key", obj);
  tlib_pass_if_status_failure("high_security", st);
  txn.high_security = 0;

  st = nr_txn_add_user_custom_parameter(&txn, "my_key", obj);
  tlib_pass_if_status_success("success", st);
  out = nr_attributes_user_to_obj(txn.attributes, NR_ATTRIBUTE_DESTINATION_ALL);
  test_obj_as_json("success", out, "{\"my_key\":123}");
  nro_delete(out);

  nro_delete(obj);
  nr_attributes_destroy(&txn.attributes);
}

static void test_add_request_parameter(void) {
  nrtxn_t txn;
  nr_attribute_config_t* config;
  int legacy_enable = 0;
  char* json;

  txn.high_security = 0;
  txn.lasp = 0;
  config = nr_attribute_config_create();
  nr_attribute_config_modify_destinations(
      config, "request.parameters.*", NR_ATTRIBUTE_DESTINATION_TXN_EVENT, 0);
  txn.attributes = nr_attributes_create(config);
  nr_attribute_config_destroy(&config);

  nr_txn_add_request_parameter(0, 0, 0, legacy_enable); /* Don't blow up */
  nr_txn_add_request_parameter(0, "key", "gamma", legacy_enable);

  nr_txn_add_request_parameter(&txn, "key", 0, legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true("no value",
                    0 == nr_strcmp("{\"user\":[],\"agent\":[]}", json),
                    "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_txn_add_request_parameter(&txn, 0, "gamma", legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true("no name",
                    0 == nr_strcmp("{\"user\":[],\"agent\":[]}", json),
                    "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_txn_add_request_parameter(&txn, "key", "gamma", legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "success",
      0
          == nr_strcmp(
              "{\"user\":[],\"agent\":[{\"dests\":[\"event\"],"
              "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
              json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  legacy_enable = 1;
  nr_txn_add_request_parameter(&txn, "key", "gamma", legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "legacy enable true",
      0
          == nr_strcmp(
              "{\"user\":[],\"agent\":[{\"dests\":[\"event\",\"trace\","
              "\"error\"],"
              "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
              json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);
  legacy_enable = 0;

  txn.high_security = 1;
  nr_txn_add_request_parameter(&txn, "zip", "zap", legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "high security prevents capture",
      0
          == nr_strcmp(
              "{\"user\":[],\"agent\":[{\"dests\":[\"event\",\"trace\","
              "\"error\"],"
              "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
              json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);
  txn.high_security = 0;

  txn.lasp = 1;
  nr_txn_add_request_parameter(&txn, "zip", "zap", legacy_enable);
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "LASP prevents capture",
      0
          == nr_strcmp(
              "{\"user\":[],\"agent\":[{\"dests\":[\"event\",\"trace\","
              "\"error\"],"
              "\"key\":\"request.parameters.key\",\"value\":\"gamma\"}]}",
              json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);
  txn.lasp = 0;

  nr_attributes_destroy(&txn.attributes);
}

static void test_set_request_referer(void) {
  nrtxn_t txn;
  char* json;

  txn.attributes = nr_attributes_create(0);

  /* Don't blow up! */
  nr_txn_set_request_referer(0, 0);
  nr_txn_set_request_referer(&txn, 0);
  nr_txn_set_request_referer(0, "zap");

  nr_txn_set_request_referer(&txn, "zap");
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "request referer added successfully with correct destinations",
      0
          == nr_strcmp(
              "{\"user\":[],\"agent\":[{\"dests\":[\"error\"],"
              "\"key\":\"request.headers.referer\",\"value\":\"zap\"}]}",
              json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  /*
   * authentication credentials, query strings and fragments should be removed
   */
  nr_txn_set_request_referer(&txn,
                             "http://user:pass@example.com/foo?q=bar#fragment");
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "request referer should be cleaned",
      0
          == nr_strcmp("{\"user\":[],\"agent\":[{\"dests\":[\"error\"],"
                       "\"key\":\"request.headers.referer\",\"value\":\"http:"
                       "\\/\\/example.com\\/foo\"}]}",
                       json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_attributes_destroy(&txn.attributes);
}

static void test_set_request_content_length(void) {
  nrtxn_t txn;
  nrobj_t* obj;
  char* json;

  txn.attributes = nr_attributes_create(0);

  /* Bad params, don't blow up! */
  nr_txn_set_request_content_length(NULL, NULL);
  nr_txn_set_request_content_length(NULL, "12");

  nr_txn_set_request_content_length(&txn, NULL);
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null("null request content length", obj);

  nr_txn_set_request_content_length(&txn, "");
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null("empty request content length", obj);

  nr_txn_set_request_content_length(&txn, "whomp");
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null("nonsense request content length", obj);

  nr_txn_set_request_content_length(&txn, "0");
  obj = nr_attributes_agent_to_obj(txn.attributes,
                                   NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_null("zero content length", obj);

  nr_txn_set_request_content_length(&txn, "42");
  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_str_equal(
      "request content length added successfully with correct destinations",
      "{"
      "\"user\":[],"
      "\"agent\":["
      "{"
      "\"dests\":[\"event\",\"trace\",\"error\"],"
      "\"key\":\"request.headers.contentLength\",\"value\":42"
      "}"
      "]"
      "}",
      json);
  nr_free(json);

  nr_attributes_destroy(&txn.attributes);
}

static void test_add_error_attributes(void) {
  nrtxn_t txn;
  char* json;

  /* Don't blow up! */
  nr_txn_add_error_attributes(NULL);
  txn.error = NULL;
  nr_txn_add_error_attributes(&txn);

  txn.error
      = nr_error_create(1, "the_msg", "the_klass", "[]", "my/span_id", 12345);
  txn.attributes = nr_attributes_create(0);

  nr_txn_add_error_attributes(&txn);

  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_true(
      "error attributes added successfully",
      0
          == nr_strcmp("{\"user\":[],\"agent\":[{\"dests\":[\"event\"],\"key\":"
                       "\"errorType\",\"value\":\"the_klass\"},"
                       "{\"dests\":[\"event\"],\"key\":\"errorMessage\","
                       "\"value\":\"the_msg\"}]}",
                       json),
      "json=%s", NRSAFESTR(json));
  nr_free(json);

  nr_attributes_destroy(&txn.attributes);
  nr_error_destroy(&txn.error);
}

static void test_duration(void) {
  nrtxn_t txn = {0};
  nrtime_t duration;
  nr_segment_t seg = {0};
  txn.segment_root = &seg;

  duration = nr_txn_duration(NULL);
  tlib_pass_if_true("null txn", 0 == duration, "duration=" NR_TIME_FMT,
                    duration);

  txn.segment_root->start_time = 1;
  txn.segment_root->stop_time = 0;
  duration = nr_txn_duration(&txn);
  tlib_pass_if_true("unfinished txn", 0 == duration, "duration=" NR_TIME_FMT,
                    duration);

  txn.segment_root->start_time = 1;
  txn.segment_root->stop_time = 2;
  duration = nr_txn_duration(&txn);
  tlib_pass_if_true("finished txn", 1 == duration, "duration=" NR_TIME_FMT,
                    duration);
}

static void test_duration_with_segment_retiming(void) {
  nrtxn_t* txn = new_txn(0);
  nr_segment_t* seg;
  nrtime_t duration;

  txn->segment_root->start_time = 0;
  txn->segment_root->stop_time = 1;

  seg = nr_segment_start(txn, NULL, NULL);
  nr_segment_set_timing(seg, 0, 500);
  nr_segment_end(&seg);

  duration = nr_txn_duration(txn);
  tlib_pass_if_time_equal(
      "a transaction with a retimed segment should not have its duration "
      "impacted",
      1, duration);

  nr_txn_destroy(&txn);
}

static void test_duration_with_txn_retiming(void) {
  nrtxn_t malformed_txn = {0};
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration;
  nr_segment_t* seg;

  /*
   * Test : Bad parameters
   */
  tlib_pass_if_bool_equal("retiming a NULL transaction must return false",
                          false, nr_txn_set_timing(NULL, 1000, 2000));
  tlib_pass_if_bool_equal(
      "retiming a transaction with a NULL segment_root must return false",
      false, nr_txn_set_timing(&malformed_txn, 1000, 2000));

  /*
   * Test : Normal operation
   */
  txn->abs_start_time = 1000;
  txn->segment_root->start_time = 0;
  txn->segment_root->stop_time = 2000;
  tlib_pass_if_bool_equal("retiming a well-formed transaction must return true",
                          true, nr_txn_set_timing(txn, 2000, 4000));

  duration = nr_txn_duration(txn);
  tlib_pass_if_time_equal(
      "a retimed transaction must reflect a change in its duration", 4000,
      duration);

  /*
   * Test : Retiming a transaction during an active segment
   */
  seg = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_bool_equal(
      "retiming a well-formed transaction while a segment is active must "
      "return true",
      true, nr_txn_set_timing(txn, 1000, 3000));

  duration = nr_txn_duration(txn);
  tlib_pass_if_time_equal(
      "a retimed transaction must reflect a change in its duration", 3000,
      duration);
  nr_segment_end(&seg);

  /*
   * Test : Retiming a transaction into the future and placing
   *        an active segment to before the beginning of time.
   *
   * This test case is mind-bending, so let's step through it:
   *
   * a) The transaction starts at absolute time = 1000.
   * b) The segment is started; its relative start time is 10, or 1010.
   * c) The transaction is retimed; it now starts at absolute time = 1015.
   * d) The segment is ended with a relative stop time of 5.
   * e) The duration of the segment is stop - start, or 5 - 10 => 0.
   */
  txn->abs_start_time = 1000;
  seg = nr_segment_start(txn, NULL, NULL);
  seg->start_time = 10;
  nr_txn_set_timing(txn, 1015, 5000);

  seg->stop_time = 5;

  test_segment_end_and_keep(&seg);

  duration = nr_time_duration(seg->start_time, seg->stop_time);
  tlib_pass_if_time_equal(
      "when a retimed transaction places a segment before the transaction's "
      "altered start time the segment must have a 0 duration",
      0, duration);

  duration = nr_txn_duration(txn);
  tlib_pass_if_time_equal(
      "a retimed transaction must reflect a change in its duration", 5000,
      duration);

  nr_txn_destroy(&txn);
}

static void test_queue_time(void) {
  nrtxn_t txn;
  nrtime_t queue_time;

  txn.status.http_x_start = 6 * NR_TIME_DIVISOR_MS;
  txn.abs_start_time = 10 * NR_TIME_DIVISOR_MS;

  queue_time = nr_txn_queue_time(&txn);
  tlib_pass_if_true("normal usage", 4 * NR_TIME_DIVISOR_MS == queue_time,
                    "queue_time=" NR_TIME_FMT, queue_time);

  queue_time = nr_txn_queue_time(0);
  tlib_pass_if_true("null txn", 0 == queue_time, "queue_time=" NR_TIME_FMT,
                    queue_time);

  txn.status.http_x_start = 0;
  queue_time = nr_txn_queue_time(&txn);
  tlib_pass_if_true("zero http_x_start", 0 == queue_time,
                    "queue_time=" NR_TIME_FMT, queue_time);
  txn.status.http_x_start = 6 * NR_TIME_DIVISOR_MS;

  txn.abs_start_time = 0;
  queue_time = nr_txn_queue_time(&txn);
  tlib_pass_if_true("zero start time", 0 == queue_time,
                    "queue_time=" NR_TIME_FMT, queue_time);
}

static void test_set_queue_start(void) {
  nrtxn_t txn;

  txn.status.http_x_start = 0;

  nr_txn_set_queue_start(NULL, 0); /* Don't blow up! */
  nr_txn_set_queue_start(&txn, 0);
  nr_txn_set_queue_start(NULL, "1368811467146000");

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "t");
  tlib_pass_if_time_equal("incomplete prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "t=");
  tlib_pass_if_time_equal("only prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "abc");
  tlib_pass_if_time_equal("bad value", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "t=abc");
  tlib_pass_if_time_equal("bad value with prefix", txn.status.http_x_start, 0);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "1368811467146000");
  tlib_pass_if_time_equal("success", txn.status.http_x_start,
                          1368811467146000ULL);

  txn.status.http_x_start = 0;
  nr_txn_set_queue_start(&txn, "t=1368811467146000");
  tlib_pass_if_time_equal("success with prefix", txn.status.http_x_start,
                          1368811467146000ULL);
}

static void test_create_rollup_metrics(void) {
  nrtxn_t txn;
  char* json;

  nr_txn_create_rollup_metrics(NULL); /* Don't blow up! */

  txn.status.background = 0;
  txn.unscoped_metrics = nrm_table_create(0);
  txn.datastore_products = nr_string_pool_create();
  nrm_force_add(txn.unscoped_metrics, "Datastore/all", 4 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "External/all", 1 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "Datastore/MongoDB/all",
                2 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "Datastore/SQLite/all",
                3 * NR_TIME_DIVISOR);
  nr_string_add(txn.datastore_products, "MongoDB");
  nr_string_add(txn.datastore_products, "SQLite");
  nr_txn_create_rollup_metrics(&txn);
  json = nr_metric_table_to_daemon_json(txn.unscoped_metrics);
  tlib_pass_if_str_equal("web txn rollups", json,
                         "[{\"name\":\"Datastore\\/"
                         "all\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16."
                         "00000],\"forced\":true},"
                         "{\"name\":\"External\\/"
                         "all\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/MongoDB\\/"
                         "all\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/SQLite\\/"
                         "all\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/"
                         "allWeb\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,"
                         "16.00000],\"forced\":true},"
                         "{\"name\":\"External\\/"
                         "allWeb\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,"
                         "1.00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/MongoDB\\/"
                         "allWeb\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,"
                         "4.00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/SQLite\\/"
                         "allWeb\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,"
                         "9.00000],\"forced\":true}]");
  nr_free(json);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.datastore_products);

  txn.status.background = 1;
  txn.unscoped_metrics = nrm_table_create(0);
  txn.datastore_products = nr_string_pool_create();
  nrm_force_add(txn.unscoped_metrics, "Datastore/all", 4 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "External/all", 1 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "Datastore/MongoDB/all",
                2 * NR_TIME_DIVISOR);
  nrm_force_add(txn.unscoped_metrics, "Datastore/SQLite/all",
                3 * NR_TIME_DIVISOR);
  nr_string_add(txn.datastore_products, "MongoDB");
  nr_string_add(txn.datastore_products, "SQLite");
  nr_txn_create_rollup_metrics(&txn);
  json = nr_metric_table_to_daemon_json(txn.unscoped_metrics);
  tlib_pass_if_str_equal("background rollups", json,
                         "[{\"name\":\"Datastore\\/"
                         "all\",\"data\":[1,4.00000,4.00000,4.00000,4.00000,16."
                         "00000],\"forced\":true},"
                         "{\"name\":\"External\\/"
                         "all\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/MongoDB\\/"
                         "all\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,4."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/SQLite\\/"
                         "all\",\"data\":[1,3.00000,3.00000,3.00000,3.00000,9."
                         "00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/"
                         "allOther\",\"data\":[1,4.00000,4.00000,4.00000,4."
                         "00000,16.00000],\"forced\":true},"
                         "{\"name\":\"External\\/"
                         "allOther\",\"data\":[1,1.00000,1.00000,1.00000,1."
                         "00000,1.00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/MongoDB\\/"
                         "allOther\",\"data\":[1,2.00000,2.00000,2.00000,2."
                         "00000,4.00000],\"forced\":true},"
                         "{\"name\":\"Datastore\\/SQLite\\/"
                         "allOther\",\"data\":[1,3.00000,3.00000,3.00000,3."
                         "00000,9.00000],\"forced\":true}]");
  nr_free(json);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_string_pool_destroy(&txn.datastore_products);
}

static void test_record_custom_event(void) {
  nrtxn_t txn;
  const char* json;
  nrtime_t now = 123 * NR_TIME_DIVISOR;
  const char* type = "my_event_type";
  nrobj_t* params = nro_create_from_json("{\"a\":\"x\",\"b\":\"z\"}");

  txn.status.recording = 1;
  txn.high_security = 0;
  txn.custom_events = nr_analytics_events_create(10);
  txn.options.custom_events_enabled = 1;

  /*
   * NULL parameters: don't blow up!
   */
  nr_txn_record_custom_event_internal(NULL, NULL, NULL, 0);
  nr_txn_record_custom_event_internal(NULL, type, params, now);

  txn.options.custom_events_enabled = 0;
  nr_txn_record_custom_event_internal(&txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn.custom_events, 0);
  tlib_pass_if_null("custom events disabled", json);
  txn.options.custom_events_enabled = 1;

  txn.status.recording = 0;
  nr_txn_record_custom_event_internal(&txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn.custom_events, 0);
  tlib_pass_if_null("not recording", json);
  txn.status.recording = 1;

  txn.high_security = 1;
  nr_txn_record_custom_event_internal(&txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn.custom_events, 0);
  tlib_pass_if_null("high security enabled", json);
  txn.high_security = 0;

  nr_txn_record_custom_event_internal(&txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn.custom_events, 0);
  tlib_pass_if_str_equal(
      "success", json,
      "[{\"type\":\"my_event_type\",\"timestamp\":123.00000},"
      "{\"b\":\"z\",\"a\":\"x\"},{}]");

  nr_analytics_events_destroy(&txn.custom_events);
  nro_delete(params);
}

static void test_is_account_trusted(void) {
  nrtxn_t txn;

  txn.app_connect_reply
      = nro_create_from_json("{\"trusted_account_ids\":[1,3]}");

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL txn", 0, nr_txn_is_account_trusted(NULL, 0));
  tlib_pass_if_int_equal("zero account", 0, nr_txn_is_account_trusted(&txn, 0));
  tlib_pass_if_int_equal("negative account", 0,
                         nr_txn_is_account_trusted(&txn, -1));

  /*
   * Test : Valid parameters.
   */
  tlib_pass_if_int_equal("untrusted account", 0,
                         nr_txn_is_account_trusted(&txn, 2));
  tlib_fail_if_int_equal("trusted account", 0,
                         nr_txn_is_account_trusted(&txn, 1));

  nro_delete(txn.app_connect_reply);
}

static void test_should_save_trace(void) {
  nrtxn_t txn = {0};

  txn.segment_count = 10;
  txn.options.tt_threshold = 100;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL txn", 0, nr_txn_should_save_trace(NULL, 0));

  /*
   * Test : Fast, synthetics transaction. (The speed shouldn't matter: that's
   *        the point.)
   */
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_fail_if_int_equal("synthetics", 0, nr_txn_should_save_trace(&txn, 0));

  /*
   * Test : Fast, non-synthetics transaction.
   */
  txn.type = 0;
  tlib_pass_if_int_equal("fast", 0, nr_txn_should_save_trace(&txn, 0));

  txn.segment_count = 0;
  tlib_pass_if_int_equal("zero nodes used", 0,
                         nr_txn_should_save_trace(&txn, 100));
  txn.segment_count = 10;

  /*
   * Test : Slow, non-synthetics transaction.
   */
  txn.type = 0;
  tlib_fail_if_int_equal("slow", 0, nr_txn_should_save_trace(&txn, 100));
}

static void test_event_should_add_guid(void) {
  nrtxn_t txn;

  tlib_pass_if_int_equal("null txn", 0, nr_txn_event_should_add_guid(NULL));
  txn.type = 0;
  tlib_pass_if_int_equal("zero type", 0, nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal("synthetics txn", 1,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_CAT_INBOUND;
  tlib_pass_if_int_equal("inbound cat txn", 1,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_CAT_OUTBOUND;
  tlib_pass_if_int_equal("outbound cat txn", 1,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_DT_INBOUND;
  tlib_pass_if_int_equal("inbound dt txn", 0,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_DT_OUTBOUND;
  tlib_pass_if_int_equal("outbound dt txn", 0,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_DT_INBOUND | NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal("inbound dt/synthetics txn", 0,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_DT_OUTBOUND | NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal("outbound dt/synthetics txn", 0,
                         nr_txn_event_should_add_guid(&txn));
  txn.type = NR_TXN_TYPE_CAT_OUTBOUND << 1;
  tlib_pass_if_int_equal("other txn type", 0,
                         nr_txn_event_should_add_guid(&txn));
}

static void test_unfinished_duration(void) {
  nrtxn_t txn;
  nrtime_t t;

  txn.abs_start_time = 0;
  t = nr_txn_unfinished_duration(&txn);
  tlib_pass_if_true("unfinished duration", t > 0, "t=" NR_TIME_FMT, t);

  txn.abs_start_time = nr_get_time() * 2;
  t = nr_txn_unfinished_duration(&txn);
  tlib_pass_if_time_equal("overflow check", t, 0);

  t = nr_txn_unfinished_duration(NULL);
  tlib_pass_if_time_equal("NULL txn", t, 0);
}

static void test_should_create_apdex_metrics(void) {
  nrtxn_t txn;

  tlib_pass_if_int_equal("null txn", 0,
                         nr_txn_should_create_apdex_metrics(NULL));

  txn.status.ignore_apdex = 0;
  txn.status.background = 0;
  tlib_pass_if_int_equal("success", 1,
                         nr_txn_should_create_apdex_metrics(&txn));

  txn.status.ignore_apdex = 0;
  txn.status.background = 1;
  tlib_pass_if_int_equal("background", 0,
                         nr_txn_should_create_apdex_metrics(&txn));

  txn.status.ignore_apdex = 1;
  txn.status.background = 0;
  tlib_pass_if_int_equal("ignore_apdex", 0,
                         nr_txn_should_create_apdex_metrics(&txn));

  txn.status.ignore_apdex = 1;
  txn.status.background = 1;
  tlib_pass_if_int_equal("ignore_apdex and background", 0,
                         nr_txn_should_create_apdex_metrics(&txn));
}

static void test_add_cat_analytics_intrinsics(void) {
  nrobj_t* bad_intrinsics = nro_new_array();
  nrobj_t* intrinsics = nro_new_hash();
  nrtxn_t* txn = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_cat_analytics_intrinsics(NULL, intrinsics);
  nr_txn_add_cat_analytics_intrinsics(txn, NULL);
  nr_txn_add_cat_analytics_intrinsics(txn, bad_intrinsics);
  tlib_pass_if_int_equal("bad parameters", 0, nro_getsize(intrinsics));

  nro_delete(bad_intrinsics);

  /*
   * Test : Non-CAT transaction.
   */
  txn->type = 0;
  nr_txn_add_cat_analytics_intrinsics(txn, intrinsics);
  tlib_pass_if_int_equal("non-cat txn", 0, nro_getsize(intrinsics));

  /*
   * Test : Inbound CAT transaction without alternate path hashes.
   */
  txn->primary_app_name = nr_strdup("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json("{\"ba2d6260\":null}");
  txn->cat.inbound_guid = nr_strdup("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup("01234567");
  txn->cat.trip_id = nr_strdup("abcdef12");
  nr_txn_set_guid(txn, "ffffffff");

  nr_txn_add_cat_analytics_intrinsics(txn, intrinsics);

  tlib_pass_if_str_equal("tripId", "abcdef12",
                         nro_get_hash_string(intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal("pathHash", "ba2d6260",
                         nro_get_hash_string(intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_str_equal(
      "referringPathHash", "01234567",
      nro_get_hash_string(intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_str_equal(
      "referringTransactionGuid", "eeeeeeee",
      nro_get_hash_string(intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_null(
      "alternatePathHashes",
      nro_get_hash_string(intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete(intrinsics);
  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_free(txn->primary_app_name);

  /*
   * Test : Inbound CAT transaction with alternate path hashes.
   */
  intrinsics = nro_new_hash();
  txn->primary_app_name = nr_strdup("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes
      = nro_create_from_json("{\"a\":null,\"b\":null}");
  txn->cat.inbound_guid = nr_strdup("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup("01234567");
  txn->cat.trip_id = nr_strdup("abcdef12");
  nr_txn_set_guid(txn, "ffffffff");

  nr_txn_add_cat_analytics_intrinsics(txn, intrinsics);

  tlib_pass_if_str_equal("tripId", "abcdef12",
                         nro_get_hash_string(intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal("pathHash", "ba2d6260",
                         nro_get_hash_string(intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_str_equal(
      "referringPathHash", "01234567",
      nro_get_hash_string(intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_str_equal(
      "referringTransactionGuid", "eeeeeeee",
      nro_get_hash_string(intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_str_equal(
      "alternatePathHashes", "a,b",
      nro_get_hash_string(intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete(intrinsics);
  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_free(txn->primary_app_name);

  /*
   * Test : Outbound CAT transaction without alternate path hashes.
   */
  intrinsics = nro_new_hash();
  txn->primary_app_name = nr_strdup("App");
  txn->type = NR_TXN_TYPE_CAT_OUTBOUND;
  txn->cat.alternate_path_hashes = nro_create_from_json("{\"b86be8ae\":null}");
  txn->cat.inbound_guid = NULL;
  txn->cat.referring_path_hash = NULL;
  txn->cat.trip_id = NULL;
  nr_txn_set_guid(txn, "ffffffff");

  nr_txn_add_cat_analytics_intrinsics(txn, intrinsics);

  tlib_pass_if_str_equal("tripId", "ffffffff",
                         nro_get_hash_string(intrinsics, "nr.tripId", NULL));
  tlib_pass_if_str_equal("pathHash", "b86be8ae",
                         nro_get_hash_string(intrinsics, "nr.pathHash", NULL));
  tlib_pass_if_null(
      "referringPathHash",
      nro_get_hash_string(intrinsics, "nr.referringPathHash", NULL));
  tlib_pass_if_null(
      "referringTransactionGuid",
      nro_get_hash_string(intrinsics, "nr.referringTransactionGuid", NULL));
  tlib_pass_if_null(
      "alternatePathHashes",
      nro_get_hash_string(intrinsics, "nr.alternatePathHashes", NULL));

  nro_delete(intrinsics);
  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);
  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_free(txn->primary_app_name);

  nr_free(txn);
}

static void test_add_cat_intrinsics(void) {
  nrobj_t* bad_intrinsics = nro_new_array();
  nrobj_t* intrinsics = nro_new_hash();
  nrtxn_t* txn = (nrtxn_t*)nr_zalloc(sizeof(nrtxn_t));

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_cat_intrinsics(NULL, intrinsics);
  nr_txn_add_cat_intrinsics(txn, NULL);
  nr_txn_add_cat_intrinsics(txn, bad_intrinsics);
  tlib_pass_if_int_equal("bad parameters", 0, nro_getsize(intrinsics));

  /*
   * Test : Non-CAT transaction.
   */
  txn->type = 0;
  nr_txn_add_cat_intrinsics(txn, intrinsics);
  tlib_pass_if_int_equal("non-cat txn", 0, nro_getsize(intrinsics));

  /*
   * Test : CAT transaction.
   */
  txn->primary_app_name = nr_strdup("App");
  txn->type = NR_TXN_TYPE_CAT_INBOUND;
  txn->cat.alternate_path_hashes
      = nro_create_from_json("{\"a\":null,\"b\":null}");
  txn->cat.inbound_guid = nr_strdup("eeeeeeee");
  txn->cat.referring_path_hash = nr_strdup("01234567");
  txn->cat.trip_id = nr_strdup("abcdef12");

  nr_txn_add_cat_intrinsics(txn, intrinsics);

  tlib_pass_if_str_equal("trip_id", "abcdef12",
                         nro_get_hash_string(intrinsics, "trip_id", NULL));
  tlib_pass_if_str_equal("path_hash", "ba2d6260",
                         nro_get_hash_string(intrinsics, "path_hash", NULL));

  nro_delete(bad_intrinsics);
  nro_delete(intrinsics);
  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.inbound_guid);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->cat.trip_id);
  nr_free(txn->primary_app_name);
  nr_free(txn);
}

static void test_alternate_path_hashes(void) {
  char* result;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  nr_memset((void*)txn, 0, sizeof(txnv));
  txn->cat.alternate_path_hashes = nro_new_hash();

  /*
   * Test : Bad parameters.
   */
  nr_txn_add_alternate_path_hash(NULL, "12345678");
  nr_txn_add_alternate_path_hash(txn, NULL);
  nr_txn_add_alternate_path_hash(txn, "");
  tlib_pass_if_int_equal("hash size", 0,
                         nro_getsize(txn->cat.alternate_path_hashes));

  tlib_pass_if_null("NULL txn", nr_txn_get_alternate_path_hashes(NULL));

  /*
   * Test : Empty path hashes.
   */
  result = nr_txn_get_alternate_path_hashes(txn);
  tlib_pass_if_null("empty path hashes", result);
  nr_free(result);

  /*
   * Test : Simple addition.
   */
  nr_txn_add_alternate_path_hash(txn, "12345678");
  tlib_pass_if_int_equal("hash size", 1,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "hash existence",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "12345678", NULL));

  nr_txn_add_alternate_path_hash(txn, "01234567");
  tlib_pass_if_int_equal("hash size", 2,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "hash existence",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "01234567", NULL));

  /*
   * Test : Duplicate.
   */
  nr_txn_add_alternate_path_hash(txn, "01234567");
  tlib_pass_if_int_equal("hash size", 2,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "hash existence",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "01234567", NULL));

  /*
   * Test : Retrieval.
   */
  result = nr_txn_get_alternate_path_hashes(txn);
  tlib_pass_if_str_equal("path hashes", "01234567,12345678", result);
  nr_free(result);

  nro_delete(txn->cat.alternate_path_hashes);
}

static void test_apdex_zone(void) {
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->error = NULL;
  txn->options.apdex_t = 10;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_char_equal("NULL txn", 'F',
                          nr_apdex_zone_label(nr_txn_apdex_zone(NULL, 0)));

  /*
   * Test : Normal transaction.
   */
  tlib_pass_if_char_equal("satisfying", 'S',
                          nr_apdex_zone_label(nr_txn_apdex_zone(txn, 10)));
  tlib_pass_if_char_equal("tolerating", 'T',
                          nr_apdex_zone_label(nr_txn_apdex_zone(txn, 30)));
  tlib_pass_if_char_equal("failing", 'F',
                          nr_apdex_zone_label(nr_txn_apdex_zone(txn, 50)));

  /*
   * Test : Error transaction.
   */
  txn->error = nr_error_create(0, "message", "class", "json", "span_id", 0);
  tlib_pass_if_char_equal("error", 'F',
                          nr_apdex_zone_label(nr_txn_apdex_zone(txn, 10)));
  nr_error_destroy(&txn->error);
}

static void test_get_cat_trip_id(void) {
  const char* guid = "GUID";
  char* trip_id = nr_strdup("Trip");
  nrtxn_t txnv = {.high_security = 0};
  nrtxn_t* txn = &txnv;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL txn", nr_txn_get_cat_trip_id(NULL));

  /*
   * Test : No trip ID or GUID.
   */
  txn->cat.trip_id = NULL;
  nr_txn_set_guid(txn, NULL);
  tlib_pass_if_null("NULL txn", nr_txn_get_cat_trip_id(txn));

  /*
   * Test : GUID only.
   */
  txn->cat.trip_id = NULL;
  nr_txn_set_guid(txn, guid);
  tlib_pass_if_str_equal("GUID only", guid, nr_txn_get_cat_trip_id(txn));

  /*
   * Test : Trip ID only.
   */
  txn->cat.trip_id = trip_id;
  nr_txn_set_guid(txn, NULL);
  tlib_pass_if_str_equal("Trip only", trip_id, nr_txn_get_cat_trip_id(txn));

  /*
   * Test : Trip ID and GUID.
   */
  txn->cat.trip_id = trip_id;
  nr_txn_set_guid(txn, guid);
  tlib_pass_if_str_equal("both", trip_id, nr_txn_get_cat_trip_id(txn));

  nr_distributed_trace_destroy(&txn->distributed_trace);
  nr_free(trip_id);
}

static void test_get_guid(void) {
  nrtxn_t txnv = {.high_security = 0};
  nrtxn_t* txn = &txnv;

  nr_memset((void*)txn, 0, sizeof(txnv));

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL txn", nr_txn_get_guid(NULL));

  /*
   * Test : NULL distributed trace.
   */
  tlib_pass_if_null("NULL distributed trace", nr_txn_get_guid(txn));

  /*
   * Test : NULL GUID.
   */
  txnv.distributed_trace = nr_distributed_trace_create();
  tlib_pass_if_null("NULL GUID", nr_txn_get_guid(txn));

  /*
   * Test : Valid GUID.
   */
  nr_distributed_trace_set_txn_id(txnv.distributed_trace, "foo");
  tlib_pass_if_str_equal("valid GUID", "foo", (nr_txn_get_guid(txn)));
  nr_distributed_trace_destroy(&txnv.distributed_trace);
}

static void test_get_path_hash(void) {
  char* result;
  nrtxn_t txnv = {.high_security = 0};
  nrtxn_t* txn = &txnv;

  nr_memset((void*)txn, 0, sizeof(txnv));
  txn->cat.alternate_path_hashes = nro_new_hash();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL txn", nr_txn_get_path_hash(NULL));

  /*
   * Test : Empty primary app name.
   */
  tlib_pass_if_null("NULL primary app name", nr_txn_get_path_hash(txn));

  /*
   * Test : Empty transaction name.
   */
  txn->primary_app_name = nr_strdup("App Name");
  result = nr_txn_get_path_hash(txn);
  tlib_pass_if_str_equal("empty transaction name", "2838559b", result);
  tlib_pass_if_int_equal("empty transaction name", 1,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "empty transaction name",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "2838559b", NULL));
  nr_free(result);

  /*
   * Test : Non-empty transaction name.
   */
  txn->name = nr_strdup("txn");
  result = nr_txn_get_path_hash(txn);
  tlib_pass_if_str_equal("transaction name", "e7e6b10a", result);
  tlib_pass_if_int_equal("transaction name", 2,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "transaction name",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "e7e6b10a", NULL));
  nr_free(result);

  /*
   * Test : Referring path hash.
   */
  txn->cat.referring_path_hash = nr_strdup("e7e6b10a");
  result = nr_txn_get_path_hash(txn);
  tlib_pass_if_str_equal("referring path hash", "282bd31f", result);
  tlib_pass_if_int_equal("referring path hash", 3,
                         nro_getsize(txn->cat.alternate_path_hashes));
  tlib_pass_if_not_null(
      "referring path hash",
      nro_get_hash_value(txn->cat.alternate_path_hashes, "282bd31f", NULL));
  nr_free(result);

  nro_delete(txn->cat.alternate_path_hashes);
  nr_free(txn->cat.referring_path_hash);
  nr_free(txn->name);
  nr_free(txn->primary_app_name);
}

static void test_is_synthetics(void) {
  nrtxn_t txn;

  tlib_pass_if_int_equal("null txn", 0, nr_txn_is_synthetics(NULL));
  txn.type = 0;
  tlib_pass_if_int_equal("zero type", 0, nr_txn_is_synthetics(&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS;
  tlib_pass_if_int_equal("only synthetics", 1, nr_txn_is_synthetics(&txn));
  txn.type = NR_TXN_TYPE_SYNTHETICS | NR_TXN_TYPE_CAT_INBOUND;
  tlib_pass_if_int_equal("synthetics and cat", 1, nr_txn_is_synthetics(&txn));
}

static void test_start_time_secs(void) {
  nrtxn_t txn;

  txn.abs_start_time = 123456789 * NR_TIME_DIVISOR_US;

  tlib_pass_if_double_equal("NULL txn", nr_txn_start_time_secs(NULL), 0.0);
  tlib_pass_if_uint_equal(
      "A transaction with a well-formed timestamp must yield a correct start "
      "time measured in seconds ",
      nr_txn_start_time_secs(&txn), (unsigned int)123.456789);
}

static void test_start_time(void) {
  nrtxn_t txn = {0};

  txn.abs_start_time = 123 * NR_TIME_DIVISOR;

  tlib_pass_if_uint_equal("NULL txn", nr_txn_start_time(NULL), 0);
  tlib_pass_if_uint_equal(
      "A transaction with a well-formed timestamp must yield a correct start "
      "time",
      nr_txn_start_time(&txn), 123 * NR_TIME_DIVISOR);
}

static void test_rel_to_abs(void) {
  nrtxn_t txn = {0};

  /*
   *  Test : Bad parameters
   */
  tlib_pass_if_uint_equal(
      "A NULL transaction must yield the original relative time",
      nr_txn_time_rel_to_abs(NULL, 246 * NR_TIME_DIVISOR),
      246 * NR_TIME_DIVISOR);

  tlib_pass_if_uint_equal(
      "A transaction with a malformed timestamp must yield the original "
      "relative time",
      nr_txn_time_rel_to_abs(&txn, 246 * NR_TIME_DIVISOR),
      246 * NR_TIME_DIVISOR);

  /*
   * Test : Normal operation
   */
  txn.abs_start_time = 123 * NR_TIME_DIVISOR;
  tlib_pass_if_uint_equal(
      "A transaction with a well-formed timestamp must yield a correct "
      "absolute start time",
      nr_txn_time_rel_to_abs(&txn, 246 * NR_TIME_DIVISOR),
      369 * NR_TIME_DIVISOR);
}

static void test_abs_to_rel(void) {
  nrtxn_t txn = {0};

  /*
   *  Test : Bad parameters
   */
  tlib_pass_if_uint_equal(
      "A NULL transaction must yield the original absolute time",
      nr_txn_time_abs_to_rel(NULL, 246 * NR_TIME_DIVISOR),
      246 * NR_TIME_DIVISOR);

  tlib_pass_if_uint_equal(
      "A transaction with a malformed timestamp must yield the original "
      "absolute time",
      nr_txn_time_abs_to_rel(&txn, 246 * NR_TIME_DIVISOR),
      246 * NR_TIME_DIVISOR);

  /*
   * Test : Normal operation
   */
  txn.abs_start_time = 100 * NR_TIME_DIVISOR;
  tlib_pass_if_uint_equal(
      "A transaction with a well-formed timestamp must yield a correct "
      "absolute start time",
      nr_txn_time_abs_to_rel(&txn, 123 * NR_TIME_DIVISOR),
      23 * NR_TIME_DIVISOR);

  tlib_pass_if_uint_equal(
      "A transaction should return 0 instead of a negative time result",
      nr_txn_time_abs_to_rel(&txn, 50 * NR_TIME_DIVISOR), 0);
}

static void test_now_rel(void) {
  nrtime_t now;
  nrtxn_t txn = {.abs_start_time = nr_get_time()};

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_time_equal("a NULL transaction must yield 0", 0,
                          nr_txn_now_rel(NULL));

  /*
   * Test : Normal operation.
   */
  now = nr_txn_now_rel(&txn);
  tlib_pass_if_true(
      "a valid transaction must return a value less than the absolute time",
      now < txn.abs_start_time,
      "abs_start_time=" NR_TIME_FMT " now=" NR_TIME_FMT, txn.abs_start_time,
      now);
}

static nrtxn_t* test_namer_with_app_and_expressions_and_return_txn(
    const char* test_name,
    const char* test_pattern,
    const char* test_filename,
    const char* expected_match) {
  nrtxn_t* txn;
  nrapp_t simple_test_app;

  nr_memset(&simple_test_app, 0, sizeof(simple_test_app));
  simple_test_app.state = NR_APP_OK;

  txn = nr_txn_begin(&simple_test_app, &nr_txn_test_options, NULL);
  tlib_pass_if_not_null("nr_txn_begin succeeds", txn);

  nr_txn_add_match_files(txn, test_pattern);
  nr_txn_match_file(txn, test_filename);
  tlib_pass_if_str_equal(test_name, expected_match, txn->path);

  return txn;
}

static void test_namer_with_app_and_expressions(const char* test_name,
                                                const char* test_pattern,
                                                const char* test_filename,
                                                const char* expected_match) {
  nrtxn_t* txn;

  txn = test_namer_with_app_and_expressions_and_return_txn(
      test_name, test_pattern, test_filename, expected_match);

  nr_txn_destroy(&txn);
  tlib_pass_if_null("Failed to destroy txn?", txn);
}

static void test_namer(void) {
  nrtxn_t* txn = NULL;
  nrapp_t simple_test_app;

  /* apparently named initializers don't work properly in C++. */
  nr_memset(&simple_test_app, 0, sizeof(simple_test_app));
  simple_test_app.state = NR_APP_OK;

  /* Mostly just exercising code paths and checking for segfaults. */
  nr_txn_match_file(NULL, "");
  nr_txn_match_file(NULL, NULL);
  nr_txn_add_file_naming_pattern(NULL, "");

  txn = nr_txn_begin(&simple_test_app, &nr_txn_test_options, NULL);
  nr_txn_add_file_naming_pattern(txn, NULL);
  nr_txn_add_file_naming_pattern(txn, "");

  nr_txn_match_file(txn, "pattern/pattern-pattern");
  tlib_pass_if_null("No match with no matchers", txn->path);
  nr_txn_add_match_files(txn, "");
  tlib_pass_if_null("Empty string doesn't add to txn namers",
                    txn->match_filenames);
  nr_txn_match_file(txn, NULL);
  tlib_pass_if_null("Doesn't match NULL", txn->path);
  nr_txn_match_file(txn, "");
  tlib_pass_if_null("Nothing in matcher doesn't match empty string", txn->path);

  nr_txn_add_match_files(txn, "pattern");
  nr_txn_match_file(txn, "");
  tlib_pass_if_null("No match with empty string", txn->path);
  nr_txn_match_file(txn, NULL);
  tlib_pass_if_null("No match with NULL", txn->path);

  nr_txn_destroy(&txn);

  /* regexes. */
  test_namer_with_app_and_expressions("All nulls doesn't match.", NULL, NULL,
                                      NULL);
  test_namer_with_app_and_expressions("No pattern to match doesn't match", NULL,
                                      "include/foo.php", NULL);
  test_namer_with_app_and_expressions("No pattern doesn't match empty string",
                                      NULL, "", NULL);
  test_namer_with_app_and_expressions("Last expression matches first",
                                      "foo,bar,f.", "foo", "fo");
  test_namer_with_app_and_expressions("Matches in path", "include",
                                      "var/include/bar/foo", "include");
  test_namer_with_app_and_expressions("Directory matching", "include/",
                                      "include/.", "include/.");
  test_namer_with_app_and_expressions("Directory matching", "include/",
                                      "include/..", "include/..");
  /* vvv  this is the weird one. Old behavior. vvv */
  test_namer_with_app_and_expressions("Directory matching", "include/",
                                      "include/...", "include/...");
  test_namer_with_app_and_expressions("Directory matching", "include",
                                      "include/...", "include");
  test_namer_with_app_and_expressions("Basic regex 0", "f[a-z]+\\d{2}", "fee23",
                                      "fee23");
  test_namer_with_app_and_expressions("Basic regex 1", "f[a-z]+.*5", "fee23954",
                                      "fee2395");
  test_namer_with_app_and_expressions("Basic regex 2", "f[a-z]+\\d{2}",
                                      "f23954", NULL);
  test_namer_with_app_and_expressions("Basic regex 3", "f[a-z]+\\d*/bee",
                                      "file99/bee/honey.php", "file99/bee");

  /* Mostly introspection. */
  txn = test_namer_with_app_and_expressions_and_return_txn(
      "Look inside the txn after setting", "p.,bla,pkg/", "pkg/./bla/pip.php",
      "pkg/.");
  nr_txn_match_file(txn, "blabulous.php");
  tlib_pass_if_str_equal("Match freezes transaction", "pkg/.", txn->path);
  nr_txn_match_file(txn, "park");
  tlib_pass_if_str_equal("Match freezes transaction", "pkg/.", txn->path);
  nr_txn_destroy(&txn);

  txn = nr_txn_begin(&simple_test_app, &nr_txn_test_options, NULL);

  txn->status.recording = 0;
  nr_txn_add_match_files(txn, "pattern");
  nr_txn_match_file(txn, "pattern/pattern-pattern");
  tlib_pass_if_null("status.recording == 0 causes name freeze", txn->path);
  txn->status.recording = 1;

  txn->status.path_type = NR_PATH_TYPE_ACTION;
  nr_txn_match_file(txn, "pattern/pattern-pattern");
  tlib_pass_if_null(
      "status.path_type == NR_PATH_TYPE_ACTION causes name freeze", txn->path);
  txn->status.path_type = NR_PATH_TYPE_UNKNOWN;

  txn->status.path_is_frozen = 1;
  nr_txn_match_file(txn, "pattern/pattern-pattern");
  tlib_pass_if_null("Setting path_is_frozen causes path not to be updated",
                    txn->path);
  txn->status.path_is_frozen = 0;

  nr_txn_destroy(&txn);
}

static void test_error_to_event(void) {
  nrtxn_t txn = {.high_security = 0};
  nr_analytics_event_t* event;
  nr_segment_t seg = {0};

  txn.cat.inbound_guid = NULL;
  txn.error = nr_error_create(1, "the_msg", "the_klass", "[]", "the_span_id",
                              123 * NR_TIME_DIVISOR);
  nr_txn_set_guid(&txn, "abcd");
  txn.name = nr_strdup("my_txn_name");
  txn.options.analytics_events_enabled = 1;
  txn.options.error_events_enabled = 1;
  txn.options.distributed_tracing_enabled = 0;
  txn.options.apdex_t = 10;
  txn.segment_root = &seg;
  txn.abs_start_time = 123 * NR_TIME_DIVISOR;
  txn.segment_root->start_time = 0;
  txn.segment_root->stop_time = 987 * NR_TIME_DIVISOR_MS;
  txn.status.background = 0;
  txn.status.ignore_apdex = 0;
  txn.synthetics = NULL;
  txn.type = 0;
  txn.unscoped_metrics = nrm_table_create(100);

  txn.attributes = nr_attributes_create(0);
  nr_attributes_user_add_long(txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR,
                              "user_long", 1);
  nr_attributes_agent_add_long(txn.attributes, NR_ATTRIBUTE_DESTINATION_ERROR,
                               "agent_long", 2);
  nr_attributes_user_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE",
      1);
  nr_attributes_agent_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_ERROR, "NOPE",
      2);

  event = nr_error_to_event(0);
  tlib_pass_if_null("null txn", event);

  txn.options.error_events_enabled = 0;
  event = nr_error_to_event(&txn);
  tlib_pass_if_null("error events disabled", event);
  txn.options.error_events_enabled = 1;

  event = nr_error_to_event(&txn);
  tlib_pass_if_str_equal("no metric parameters",
                         "["
                         "{"
                         "\"type\":\"TransactionError\","
                         "\"timestamp\":123.00000,"
                         "\"error.class\":\"the_klass\","
                         "\"error.message\":\"the_msg\","
                         "\"transactionName\":\"my_txn_name\","
                         "\"duration\":0.98700,"
                         "\"nr.transactionGuid\":\"abcd\","
                         "\"guid\":\"abcd\""
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]",
                         nr_analytics_event_json(event));
  nr_analytics_event_destroy(&event);

  nrm_add(txn.unscoped_metrics, "Datastore/all", 1 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "External/all", 2 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "WebFrontend/QueueTime", 3 * NR_TIME_DIVISOR);

  event = nr_error_to_event(&txn);
  tlib_pass_if_str_equal("all metric parameters",
                         nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"TransactionError\","
                         "\"timestamp\":123.00000,"
                         "\"error.class\":\"the_klass\","
                         "\"error.message\":\"the_msg\","
                         "\"transactionName\":\"my_txn_name\","
                         "\"duration\":0.98700,"
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"externalCallCount\":1,"
                         "\"nr.transactionGuid\":\"abcd\","
                         "\"guid\":\"abcd\""
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  txn.synthetics = nr_synthetics_create("[1,100,\"a\",\"b\",\"c\"]");
  txn.cat.inbound_guid = nr_strdup("foo_guid");
  event = nr_error_to_event(&txn);
  tlib_pass_if_str_equal("synthetics txn", nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"TransactionError\","
                         "\"timestamp\":123.00000,"
                         "\"error.class\":\"the_klass\","
                         "\"error.message\":\"the_msg\","
                         "\"transactionName\":\"my_txn_name\","
                         "\"duration\":0.98700,"
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"externalCallCount\":1,"
                         "\"nr.transactionGuid\":\"abcd\","
                         "\"guid\":\"abcd\","
                         "\"nr.referringTransactionGuid\":\"foo_guid\","
                         "\"nr.syntheticsResourceId\":\"a\","
                         "\"nr.syntheticsJobId\":\"b\","
                         "\"nr.syntheticsMonitorId\":\"c\""
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_free(txn.name);
  nr_free(txn.cat.inbound_guid);
  nr_error_destroy(&txn.error);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_attributes_destroy(&txn.attributes);
  nr_synthetics_destroy(&txn.synthetics);
}

static void test_create_event(void) {
  nrtxn_t txn = {.high_security = 0};
  nr_analytics_event_t* event;
  nr_segment_t seg = {0};

  txn.error = NULL;
  txn.status.background = 0;
  txn.status.ignore_apdex = 0;
  txn.options.analytics_events_enabled = 1;
  txn.options.apdex_t = 10;
  txn.options.distributed_tracing_enabled = 0;
  nr_txn_set_guid(&txn, "abcd");
  txn.name = nr_strdup("my_txn_name");
  txn.abs_start_time = 123 * NR_TIME_DIVISOR;

  txn.segment_root = &seg;
  txn.segment_root->start_time = 0;
  txn.segment_root->stop_time = 987 * NR_TIME_DIVISOR_MS;
  txn.unscoped_metrics = nrm_table_create(100);
  txn.synthetics = NULL;
  txn.type = 0;

  txn.attributes = nr_attributes_create(0);
  nr_attributes_user_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "user_long", 1);
  nr_attributes_agent_add_long(
      txn.attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT, "agent_long", 2);
  nr_attributes_user_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_EVENT,
      "NOPE", 1);
  nr_attributes_agent_add_long(
      txn.attributes,
      NR_ATTRIBUTE_DESTINATION_ALL & ~NR_ATTRIBUTE_DESTINATION_TXN_EVENT,
      "NOPE", 2);

  txn.final_data = nr_segment_tree_finalise(
      &txn, NR_MAX_SEGMENTS, NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED,
      nr_txn_handle_total_time, NULL);

  event = nr_txn_to_event(0);
  tlib_pass_if_null("null txn", event);

  txn.options.analytics_events_enabled = 0;
  event = nr_txn_to_event(&txn);
  tlib_pass_if_null("analytics event disabled", event);
  txn.options.analytics_events_enabled = 1;

  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("no metric parameters", nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":0.98700,"
                         "\"nr.apdexPerfZone\":\"F\","
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  nrm_add(txn.unscoped_metrics, "Datastore/all", 1 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "External/all", 2 * NR_TIME_DIVISOR);
  nrm_add(txn.unscoped_metrics, "WebFrontend/QueueTime", 3 * NR_TIME_DIVISOR);

  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("all metric parameters",
                         nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":0.98700,"
                         "\"nr.apdexPerfZone\":\"F\","
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  txn.status.background = 1;
  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("background tasks also make events",
                         nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":0.98700,"
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);
  txn.status.background = 0;

  txn.type = NR_TXN_TYPE_SYNTHETICS;
  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("synthetics txn (note guid!)",
                         nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":0.98700,"
                         "\"nr.guid\":\"abcd\","
                         "\"nr.apdexPerfZone\":\"F\","
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);
  txn.type = 0;

  txn.final_data.total_time = (987 + 333) * NR_TIME_DIVISOR_MS;
  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("totalTime > duration", nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":1.32000,"
                         "\"nr.apdexPerfZone\":\"F\","
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  nr_txn_set_timing(&txn, 456 * NR_TIME_DIVISOR, 789 * NR_TIME_DIVISOR_MS);
  event = nr_txn_to_event(&txn);
  tlib_pass_if_str_equal("retimed transaction", nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":456.00000,"
                         "\"duration\":0.78900,"
                         "\"totalTime\":1.32000,"
                         "\"nr.apdexPerfZone\":\"F\","
                         "\"queueDuration\":3.00000,"
                         "\"externalDuration\":2.00000,"
                         "\"databaseDuration\":1.00000,"
                         "\"databaseCallCount\":1,"
                         "\"error\":false"
                         "},"
                         "{\"user_long\":1},"
                         "{\"agent_long\":2}"
                         "]");
  nr_analytics_event_destroy(&event);

  nr_txn_final_destroy_fields(&txn.final_data);
  nr_segment_destroy_fields(txn.segment_root);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_free(txn.name);
  nr_attributes_destroy(&txn.attributes);
  nrm_table_destroy(&txn.unscoped_metrics);
}

static void test_create_event_with_retimed_segments(void) {
  nr_segment_t* seg;
  nrtxn_t* txn = new_txn(0);
  nr_analytics_event_t* event;

  txn->abs_start_time = 123 * NR_TIME_DIVISOR;
  txn->segment_root->start_time = 0;
  txn->segment_root->stop_time = 987 * NR_TIME_DIVISOR_MS;
  txn->name = nr_strdup("my_txn_name");

  /*
   * Test : A retimed segment does impact totalTime.
   */
  seg = nr_segment_start(txn, NULL, NULL);
  nr_segment_set_timing(seg, 0, 10000 * NR_TIME_DIVISOR_MS);
  nr_segment_end(&seg);

  txn->final_data = nr_segment_tree_finalise(
      txn, NR_MAX_SEGMENTS, NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED,
      nr_txn_handle_total_time, NULL);

  event = nr_txn_to_event(txn);
  tlib_pass_if_str_equal("retimed segments", nr_analytics_event_json(event),
                         "["
                         "{"
                         "\"type\":\"Transaction\","
                         "\"name\":\"my_txn_name\","
                         "\"timestamp\":123.00000,"
                         "\"duration\":0.98700,"
                         "\"totalTime\":10.00000,"
                         "\"nr.apdexPerfZone\":\"T\","
                         "\"error\":false"
                         "},"
                         "{},"
                         "{}"
                         "]");
  nr_analytics_event_destroy(&event);

  nr_txn_destroy(&txn);
}

static void test_name_from_function(void) {
  nrtxn_t txn = {.high_security = 0};

  txn.status.path_is_frozen = 0;
  txn.status.path_type = NR_PATH_TYPE_UNKNOWN;
  txn.path = NULL;

  /* Bad params */
  nr_txn_name_from_function(NULL, NULL, NULL);
  nr_txn_name_from_function(NULL, "my_func", "my_class");
  nr_txn_name_from_function(&txn, NULL, "my_class");
  tlib_pass_if_null("bad params", txn.path);
  tlib_pass_if_int_equal("bad params", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_UNKNOWN);

  /* only function name */
  nr_txn_name_from_function(&txn, "my_func", NULL);
  tlib_pass_if_str_equal("only function name", txn.path, "my_func");
  tlib_pass_if_int_equal("only function name", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_FUNCTION);
  nr_txn_name_from_function(&txn, "other_func", NULL);
  tlib_pass_if_str_equal("not replaced", txn.path, "my_func");
  tlib_pass_if_int_equal("not replaced", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_FUNCTION);

  nr_free(txn.path);
  txn.status.path_type = NR_PATH_TYPE_UNKNOWN;

  /* with class name */
  nr_txn_name_from_function(&txn, "my_func", "my_class");
  tlib_pass_if_str_equal("with class name", txn.path, "my_class::my_func");
  tlib_pass_if_int_equal("with class name", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_FUNCTION);
  nr_txn_name_from_function(&txn, "other_func", NULL);
  tlib_pass_if_str_equal("not replaced", txn.path, "my_class::my_func");
  tlib_pass_if_int_equal("not replaced", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_FUNCTION);

  /* doesnt override higher priority name */
  nr_txn_set_path(NULL, &txn, "api", NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
  nr_txn_name_from_function(&txn, "my_func", "my_class");
  tlib_pass_if_str_equal("higher priority name", txn.path, "api");
  tlib_pass_if_int_equal("higher priority name", (int)txn.status.path_type,
                         (int)NR_PATH_TYPE_CUSTOM);

  nr_free(txn.path);
}

static void test_txn_ignore(void) {
  nrtxn_t txn;

  nr_txn_ignore(NULL);

  txn.status.ignore = 0;
  txn.status.recording = 1;

  nr_txn_ignore(&txn);

  tlib_pass_if_int_equal("nr_txn_ignore sets ignore", txn.status.ignore, 1);
  tlib_pass_if_int_equal("nr_txn_ignore sets recording", txn.status.recording,
                         0);
}

static void test_add_custom_metric(void) {
  nrtxn_t txn;
  double value_ms = 123.45;
  char* json;

  txn.unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  txn.status.recording = 1;

  tlib_pass_if_status_failure("null params",
                              nr_txn_add_custom_metric(NULL, NULL, value_ms));
  tlib_pass_if_status_failure("null name",
                              nr_txn_add_custom_metric(&txn, NULL, value_ms));
  tlib_pass_if_status_failure(
      "null txn", nr_txn_add_custom_metric(NULL, "my_metric", value_ms));

  tlib_pass_if_status_failure("NAN",
                              nr_txn_add_custom_metric(&txn, "my_metric", NAN));
  tlib_pass_if_status_failure(
      "INFINITY", nr_txn_add_custom_metric(&txn, "my_metric", INFINITY));

  txn.status.recording = 0;
  tlib_pass_if_status_failure(
      "not recording", nr_txn_add_custom_metric(&txn, "my_metric", value_ms));
  txn.status.recording = 1;

  tlib_pass_if_status_success(
      "custom metric success",
      nr_txn_add_custom_metric(&txn, "my_metric", value_ms));
  json = nr_metric_table_to_daemon_json(txn.unscoped_metrics);
  tlib_pass_if_str_equal("custom metric success", json,
                         "[{\"name\":\"my_metric\",\"data\":[1,0.12345,0.12345,"
                         "0.12345,0.12345,0.01524]}]");
  nr_free(json);

  nrm_table_destroy(&txn.unscoped_metrics);
}

#define test_txn_cat_map_cross_agent_testcase(...) \
  test_txn_cat_map_cross_agent_testcase_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_txn_cat_map_cross_agent_testcase_fn(nrapp_t* app,
                                                     const nrobj_t* hash,
                                                     const char* file,
                                                     int line) {
  int i;
  int size;
  nrtxn_t* txn;
  const char* testname = nro_get_hash_string(hash, "name", 0);
  const char* appname = nro_get_hash_string(hash, "appName", 0);
  const char* txnname = nro_get_hash_string(hash, "transactionName", 0);
  const char* guid = nro_get_hash_string(hash, "transactionGuid", 0);
  const nrobj_t* inbound_x_newrelic_txn
      = nro_get_hash_value(hash, "inboundPayload", NULL);
  const nrobj_t* outbound = nro_get_hash_array(hash, "outboundRequests", NULL);
  const nrobj_t* expected_intrinsics
      = nro_get_hash_hash(hash, "expectedIntrinsicFields", NULL);
  const nrobj_t* missing_intrinsics
      = nro_get_hash_array(hash, "nonExpectedIntrinsicFields", NULL);
  nrobj_t* intrinsics;

  nr_free(app->info.appname);
  app->info.appname = nr_strdup(appname);
  nr_free(app->entity_name);
  app->entity_name = nr_strdup(appname);

  txn = nr_txn_begin(app, &nr_txn_test_options, NULL);
  test_pass_if_true_file_line("tests valid", NULL != txn, file, line, "txn=%p",
                              txn);
  if (NULL == txn) {
    return;
  }

  nr_txn_set_guid(txn, guid);

  nr_header_process_x_newrelic_transaction(txn, inbound_x_newrelic_txn);

  size = nro_getsize(outbound);
  for (i = 1; i <= size; i++) {
    const nrobj_t* outbound_request = nro_get_array_hash(outbound, i, NULL);
    const char* outbound_txn_name
        = nro_get_hash_string(outbound_request, "outboundTxnName", NULL);
    const nrobj_t* payload
        = nro_get_hash_value(outbound_request, "expectedOutboundPayload", NULL);

    nr_free(txn->path);
    txn->path = nr_strdup(outbound_txn_name);

    {
      char* expected = nro_to_json(payload);
      char* decoded_x_newrelic_id = NULL;
      char* decoded_x_newrelic_txn = NULL;

      nr_header_outbound_request_decoded(txn, &decoded_x_newrelic_id,
                                         &decoded_x_newrelic_txn);

      tlib_check_if_str_equal_f(testname, expected, expected,
                                decoded_x_newrelic_txn, decoded_x_newrelic_txn,
                                true, file, line);

      nr_free(expected);
      nr_free(decoded_x_newrelic_id);
      nr_free(decoded_x_newrelic_txn);
    }
  }

  txn->status.path_is_frozen = 1;
  nr_free(txn->name);
  txn->name = nr_strdup(txnname);

  intrinsics = nr_txn_event_intrinsics(txn);

  /*
   * Test absence of non-expected intrinsic fields.
   */
  size = nro_getsize(missing_intrinsics);
  for (i = 1; i <= size; i++) {
    const char* key = nro_get_array_string(missing_intrinsics, i, NULL);
    const nrobj_t* val = nro_get_hash_value(intrinsics, key, NULL);

    test_pass_if_true_file_line(testname, 0 == val, file, line, "key='%s'",
                                NRSAFESTR(key));
  }

  /*
   * Test presence of expected intrinsics.
   */
  {
    hash_is_subset_of_data_t data = {
        .testname = testname,
        .set = intrinsics,
        .file = file,
        .line = line,
    };
    nro_iteratehash(expected_intrinsics, hash_is_subset_of, &data);
  }

  nro_delete(intrinsics);

  nr_txn_destroy(&txn);
}

static void test_txn_cat_map_cross_agent_tests(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;
  nrapp_t app;
  int size;

  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;
  app.connect_reply
      = nro_create_from_json("{\"cross_process_id\":\"my_cross_process_id\"}");

  json = nr_read_file_contents(CROSS_AGENT_TESTS_DIR "/cat/cat_map.json",
                               10 * 1000 * 1000);
  array = nro_create_from_json(json);
  otype = nro_type(array);
  tlib_pass_if_int_equal("tests valid", (int)NR_OBJECT_ARRAY, (int)otype);

  size = nro_getsize(array);
  for (i = 1; i <= size; i++) {
    const nrobj_t* hash = nro_get_array_hash(array, i, 0);

    test_txn_cat_map_cross_agent_testcase(&app, hash);
  }

  nr_free(app.info.appname);
  nr_free(app.entity_name);
  nro_delete(app.connect_reply);
  nro_delete(array);
  nr_free(json);
}

/*
 * This does some cheating to tweak DT payloads to make them easily
 * comparable in cross agent tests. In combination with nro_iteratehash
 * this transforms a payload like this
 *
 * {
 *   "v": [0, 1],
 *   "d": ["ac": "1", "tr":"2"]
 * }
 *
 * into this
 *
 * {
 *   "v": [0, 1],
 *   "d": ["ac": "1", "tr":"2"],
 *   "d.ac":"1",
 *   "d.tr":"2"
 * }
 */
static nr_status_t flatten_dt_payload_into(const char* key,
                                           const nrobj_t* val,
                                           void* ptr) {
  nrobj_t* payload = (nrobj_t*)ptr;
  char* flatkey = nr_formatf("d.%s", key);

  nro_set_hash(payload, flatkey, val);

  nr_free(flatkey);

  return NR_SUCCESS;
}

static nr_status_t flatten_w3c_dt_payload_into(const char* key,
                                               const nrobj_t* val,
                                               void* ptr) {
  nrobj_t* payload = (nrobj_t*)ptr;
  char* flatkey = nr_formatf("newrelic.d.%s", key);

  nro_set_hash(payload, flatkey, val);

  nr_free(flatkey);

  return NR_SUCCESS;
}

static nr_status_t flatten_w3c_traceparent_payload_into(const char* key,
                                                        const nrobj_t* val,
                                                        void* ptr) {
  nrobj_t* payload = (nrobj_t*)ptr;
  char* flatkey = nr_formatf("traceparent.%s", key);

  /*
   * The cross agent test suite expects `trace_flags` to be a string.
   */
  if (0 == strcmp(key, "trace_flags")) {
    int64_t v = nro_get_long(val, NULL);
    char flags[3] = "00";

    snprintf(flags, 3, "%c%c", (v & 0x2) ? '1' : '0', (v & 0x1) ? '1' : '0');
    nro_set_hash_string(payload, flatkey, flags);
  } else {
    nro_set_hash(payload, flatkey, val);
  }

  nr_free(flatkey);

  return NR_SUCCESS;
}

static nr_status_t flatten_w3c_tracestate_payload_into(const char* key,
                                                       const nrobj_t* val,
                                                       void* ptr) {
  nrobj_t* payload = (nrobj_t*)ptr;
  char* flatkey = nr_formatf("tracestate.%s", key);

  /*
   * The cross agent test suite expects `sampled` to be a boolean.
   */
  if (0 == strcmp(key, "sampled")) {
    int64_t v = nro_get_long(val, NULL);

    nro_set_hash_boolean(payload, flatkey, v);
  } else {
    nro_set_hash(payload, flatkey, val);
  }

  nr_free(flatkey);

  return NR_SUCCESS;
}

#define test_txn_dt_cross_agent_intrinsics(...) \
  test_txn_dt_cross_agent_intrinsics_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_txn_dt_cross_agent_intrinsics_fn(const char* testname,
                                                  const char* objname,
                                                  nrobj_t* obj,
                                                  const nrobj_t* spec,
                                                  const char* file,
                                                  int line) {
  const nrobj_t* unexpected = nro_get_hash_array(spec, "unexpected", NULL);
  const nrobj_t* expected = nro_get_hash_array(spec, "expected", NULL);
  const nrobj_t* exact = nro_get_hash_value(spec, "exact", NULL);

  /* expected */
  for (int j = 1; j <= nro_getsize(expected); j++) {
    const char* key = nro_get_array_string(expected, j, NULL);
    const nrobj_t* val = nro_get_hash_value(obj, key, NULL);
    test_pass_if_true_file_line(testname, 0 != val, file, line,
                                "missing key on %s, key='%s'",
                                NRSAFESTR(objname), NRSAFESTR(key));
  }

  /* unexpected */
  for (int j = 1; j <= nro_getsize(unexpected); j++) {
    const char* key = nro_get_array_string(unexpected, j, NULL);
    const nrobj_t* val = nro_get_hash_value(obj, key, NULL);

    test_pass_if_true_file_line(testname, 0 == val, file, line,
                                "unexpected key on %s, key='%s'", objname,
                                NRSAFESTR(key));
  }

  /* exact */
  {
    hash_is_subset_of_data_t data = {
        .testname = testname,
        .set = obj,
        .file = file,
        .line = line,
    };
    nro_iteratehash(exact, hash_is_subset_of, &data);
  }
}

static void test_txn_dt_cross_agent_testcase(nrapp_t* app,
                                             const nrobj_t* hash) {
  int size;
  nrtxn_t* txn;
  nrobj_t* txn_event;
  nrobj_t* error_event;
  nrobj_t* span_event;
  nr_hashmap_t* header_map = nr_hashmap_create(NULL);

  const char* testname = nro_get_hash_string(hash, "test_name", NULL);
  const char* trusted_account_key
      = nro_get_hash_string(hash, "trusted_account_key", NULL);
  const char* account_id = nro_get_hash_string(hash, "account_id", NULL);
  bool web_transaction = nro_get_hash_boolean(hash, "web_transaction", NULL);
  bool span_events = nro_get_hash_boolean(hash, "span_events_enabled", NULL);
  bool raises_exception = nro_get_hash_boolean(hash, "raises_exception", NULL);
  bool force_sampled = nro_get_hash_boolean(hash, "force_sampled_true", NULL);
  const char* transport_type
      = nro_get_hash_string(hash, "transport_type", NULL);
  const nrobj_t* inbound_payloads
      = nro_get_hash_array(hash, "inbound_payloads", NULL);
  const nrobj_t* outbound_payloads
      = nro_get_hash_array(hash, "outbound_payloads", NULL);
  const nrobj_t* intrinsics = nro_get_hash_value(hash, "intrinsics", NULL);
  const nrobj_t* intrinsics_common
      = nro_get_hash_value(intrinsics, "common", NULL);
  const nrobj_t* intrinsics_target_events
      = nro_get_hash_array(intrinsics, "target_events", NULL);
  const nrobj_t* metrics = nro_get_hash_value(hash, "expected_metrics", NULL);

  /*
   * Initialize the transaction.
   * */
  nro_delete(app->connect_reply);
  app->connect_reply = nro_new_hash();
  nro_set_hash_string(app->connect_reply, "primary_application_id", "1");
  nro_set_hash_string(app->connect_reply, "trusted_account_key",
                      trusted_account_key);
  nro_set_hash_string(app->connect_reply, "account_id", account_id);

  txn = nr_txn_begin(app, &nr_txn_test_options, NULL);
  tlib_pass_if_not_null(testname, txn);

  if (NULL == txn) {
    return;
  }

  txn->name = nr_strdup("name");

  txn->options.distributed_tracing_enabled = true;
  txn->options.span_events_enabled = span_events;
  txn->options.tt_enabled = true;
  txn->options.tt_threshold = 0;
  txn->options.error_events_enabled = true;
  txn->options.err_enabled = true;

  if (!web_transaction) {
    txn->status.background = true;
  }

  if (force_sampled) {
    nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  }

  if (raises_exception) {
    txn->options.err_enabled = 1;
    txn->error = NULL;
    nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  }

  /*
   * Accept inbound payloads.
   */
  if (NULL == inbound_payloads) {
    nr_txn_accept_distributed_trace_payload(txn, NULL, transport_type);
  }

  size = nro_getsize(inbound_payloads);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* json_payload = nro_get_array_hash(inbound_payloads, i, NULL);
    char* payload = nro_to_json(json_payload);
    nr_hashmap_update(header_map, NR_PSTR(NEWRELIC), payload);

    tlib_pass_if_not_null(testname, payload);

    nr_txn_accept_distributed_trace_payload(txn, header_map, transport_type);

    nr_free(payload);
  }

  /*
   * Send outbound payloads.
   */
  size = nro_getsize(outbound_payloads);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* spec = nro_get_array_hash(outbound_payloads, i, NULL);
    nr_segment_t segment = {.id = NULL, .txn = txn};
    char* payload = nr_txn_create_distributed_trace_payload(txn, &segment);
    nrobj_t* json_payload = nro_create_from_json(payload);
    const nrobj_t* json_payload_d = nro_get_hash_value(json_payload, "d", NULL);

    nro_iteratehash(json_payload_d, flatten_dt_payload_into, json_payload);

    /*
     * With flatten_dt_payload_into applied, attributes on an outbound payload
     * can be compared the same way as attributes on intrinsics events.
     */
    test_txn_dt_cross_agent_intrinsics(testname, "outbound payload",
                                       json_payload, spec);

    nr_free(segment.id);
    nro_delete(json_payload);
    nr_free(payload);
  }

  txn->segment_root->start_time = 1000;
  txn->segment_root->stop_time = 2000;
  txn->segment_count++;
  txn->final_data = nr_segment_tree_finalise(
      txn, NR_MAX_SEGMENTS, NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED, NULL,
      NULL);

  /*
   * Intrinsics.
   */

  /* Initialize transaction event */
  txn_event = nr_txn_event_intrinsics(txn);

  /* Initialize error event */
  {
    nrobj_t* data;
    nr_analytics_event_t* error_event_analytics;

    nr_txn_record_error(txn, 100, true, "error", "class", "{}");
    error_event_analytics = nr_error_to_event(txn);
    data = nro_create_from_json(nr_analytics_event_json(error_event_analytics));
    error_event = nro_copy(nro_get_array_hash(data, 1, NULL));

    nro_delete(data);
    nr_analytics_event_destroy(&error_event_analytics);
  }

  /* Pull a span event out of the flatbuffer. */
  {
    nrobj_t* data;
    nr_flatbuffers_table_t tbl;
    nr_flatbuffer_t* fb;
    nr_aoffset_t events;

    txn->segment_root->name = nr_string_add(txn->trace_strings, txn->name);

    fb = nr_txndata_encode(txn);
    nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                   nr_flatbuffers_len(fb));
    nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                 MESSAGE_BODY_NONE);
    nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA);
    events
        = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_SPAN_EVENTS);
    nr_flatbuffers_table_init(
        &tbl, tbl.data, tbl.length,
        nr_flatbuffers_read_indirect(tbl.data, events).offset);

    data = nro_create_from_json(
        (const char*)nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA));

    span_event = nro_copy(nro_get_array_hash(data, 1, NULL));

    nro_delete(data);
    nr_flatbuffers_destroy(&fb);
  }

  size = nro_getsize(intrinsics_target_events);
  for (int i = 1; i <= size; i++) {
    const char* event_type
        = nro_get_array_string(intrinsics_target_events, i, 0);
    const nrobj_t* intrinsics_type
        = nro_get_hash_value(intrinsics, event_type, NULL);

    if (0 == nr_strcmp(event_type, "Transaction")) {
      test_txn_dt_cross_agent_intrinsics(testname, "transaction event",
                                         txn_event, intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "transaction event",
                                         txn_event, intrinsics_type);
    } else if (0 == nr_strcmp(event_type, "TransactionError")) {
      test_txn_dt_cross_agent_intrinsics(testname, "error event", error_event,
                                         intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "error event", error_event,
                                         intrinsics_type);
    } else if (0 == nr_strcmp(event_type, "Span")) {
      test_txn_dt_cross_agent_intrinsics(testname, "span_event", span_event,
                                         intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "span event", span_event,
                                         intrinsics_type);
    }
  }
  nro_delete(txn_event);
  nro_delete(span_event);
  nro_delete(error_event);

  /*
   * Metrics.
   */
  nr_txn_create_duration_metrics(txn, 1000, 1000);
  nr_txn_create_error_metrics(txn, "WebTransaction/Action/not_words");
  size = nro_getsize(metrics);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* metric = nro_get_array_array(metrics, i, 0);
    const char* name = nro_get_array_string(metric, 1, 0);
    const nrtime_t count = nro_get_array_int(metric, 2, 0);

    const nrmetric_t* m = nrm_find(txn->unscoped_metrics, name);
    const char* nm = nrm_get_name(txn->unscoped_metrics, m);

    tlib_pass_if_true(testname, 0 != m, "m=%p", m);
    tlib_pass_if_true(testname, 0 == nr_strcmp(nm, name), "nm=%s name=%s", nm,
                      name);
    tlib_pass_if_true(testname, nrm_count(m) == count,
                      "name=%s nrm_count (m)=" NR_TIME_FMT
                      " count=" NR_TIME_FMT,
                      name, nrm_count(m), count);
  }

  nr_hashmap_destroy(&header_map);
  nr_txn_destroy(&txn);
}

static void test_txn_dt_cross_agent_tests(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;
  nrapp_t app = {
      .state = NR_APP_OK,
      .limits = default_app_limits(),
  };
  int size;

  json = nr_read_file_contents(CROSS_AGENT_TESTS_DIR
                               "/distributed_tracing/distributed_tracing.json",
                               10 * 1000 * 1000);
  array = nro_create_from_json(json);
  otype = nro_type(array);
  tlib_pass_if_int_equal("tests valid", (int)NR_OBJECT_ARRAY, (int)otype);

  size = nro_getsize(array);
  for (i = 1; i <= size; i++) {
    const nrobj_t* hash = nro_get_array_hash(array, i, 0);

    test_txn_dt_cross_agent_testcase(&app, hash);
  }

  nr_free(app.info.appname);
  nro_delete(app.connect_reply);
  nro_delete(array);
  nr_free(json);
}

static void test_txn_trace_context_cross_agent_testcase(nrapp_t* app,
                                                        const nrobj_t* hash) {
  int size;
  nrtxn_t* txn;
  nrobj_t* txn_event;
  nrobj_t* error_event;
  nrobj_t* span_event;
  nr_hashmap_t* header_map
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_hashmap_dtor_str);

  const char* testname = nro_get_hash_string(hash, "test_name", NULL);
  const char* trusted_account_key
      = nro_get_hash_string(hash, "trusted_account_key", NULL);
  const char* account_id = nro_get_hash_string(hash, "account_id", NULL);
  bool web_transaction = nro_get_hash_boolean(hash, "web_transaction", NULL);
  bool raises_exception = nro_get_hash_boolean(hash, "raises_exception", NULL);
  bool force_sampled = nro_get_hash_boolean(hash, "force_sampled_true", NULL);
  bool span_events = nro_get_hash_boolean(hash, "span_events_enabled", NULL);
  bool transaction_events
      = nro_get_hash_boolean(hash, "transaction_events_enabled", NULL);
  const char* transport_type
      = nro_get_hash_string(hash, "transport_type", NULL);
  const nrobj_t* inbound_headers
      = nro_get_hash_array(hash, "inbound_headers", NULL);
  const nrobj_t* outbound_payloads
      = nro_get_hash_array(hash, "outbound_payloads", NULL);
  const nrobj_t* intrinsics = nro_get_hash_value(hash, "intrinsics", NULL);
  const nrobj_t* intrinsics_common
      = nro_get_hash_value(intrinsics, "common", NULL);
  const nrobj_t* intrinsics_target_events
      = nro_get_hash_array(intrinsics, "target_events", NULL);
  const nrobj_t* metrics = nro_get_hash_value(hash, "expected_metrics", NULL);

  /*
   * Initialize the transaction.
   * */
  nro_delete(app->connect_reply);
  app->connect_reply = nro_new_hash();
  nro_set_hash_string(app->connect_reply, "primary_application_id", "2827902");
  nro_set_hash_string(app->connect_reply, "trusted_account_key",
                      trusted_account_key);
  nro_set_hash_string(app->connect_reply, "account_id", account_id);

  txn = nr_txn_begin(app, &nr_txn_test_options, NULL);
  tlib_pass_if_not_null(testname, txn);

  if (NULL == txn) {
    return;
  }

  txn->name = nr_strdup("name");

  txn->options.distributed_tracing_enabled = true;
  txn->options.span_events_enabled = span_events;
  txn->options.analytics_events_enabled = transaction_events;
  txn->options.tt_enabled = true;
  txn->options.tt_threshold = 0;
  txn->options.error_events_enabled = true;
  txn->options.err_enabled = true;

  if (!web_transaction) {
    txn->status.background = true;
  }

  if (force_sampled) {
    nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  }

  if (raises_exception) {
    txn->options.err_enabled = 1;
    txn->error = NULL;
    nr_txn_record_error(txn, 2, true, "msg", "class", "[\"A\",\"B\"]");
  }

  /*
   * Accept inbound payloads.
   */
  size = nro_getsize(inbound_headers);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* headers = nro_get_array_hash(inbound_headers, i, NULL);
    size_t num_headers = nro_getsize(headers);

    for (size_t j = 1; j <= num_headers; j++) {
      const char* key;
      char* value;

      nro_get_hash_value_by_index(headers, j, NULL, &key);
      value = nr_strdup(nro_get_hash_string(headers, key, NULL));

      if (key && value) {
        nr_hashmap_update(header_map, key, nr_strlen(key), value);
      }
    }
  }

  nr_txn_accept_distributed_trace_payload(txn, header_map, transport_type);

  /*
   * Send outbound payloads.
   */
  size = nro_getsize(outbound_payloads);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* spec = nro_get_array_hash(outbound_payloads, i, NULL);
    nr_segment_t segment = {.id = NULL, .txn = txn};
    char* payload = nr_txn_create_distributed_trace_payload(txn, &segment);
    char* traceparent = nr_txn_create_w3c_traceparent_header(txn, &segment);
    char* tracestate = nr_txn_create_w3c_tracestate_header(txn, &segment);
    nrobj_t* json_payload = nro_new_hash();
    nrobj_t* nr_payload = nro_create_from_json(payload);
    const nrobj_t* json_payload_d = nro_get_hash_value(nr_payload, "d", NULL);
    nrobj_t* w3c_payload = nr_distributed_trace_convert_w3c_headers_to_object(
        traceparent, tracestate, trusted_account_key, NULL);

    nro_iteratehash(json_payload_d, flatten_w3c_dt_payload_into, json_payload);
    nro_iteratehash(nro_get_hash_value(w3c_payload, "traceparent", NULL),
                    flatten_w3c_traceparent_payload_into, json_payload);
    nro_iteratehash(nro_get_hash_value(w3c_payload, "tracestate", NULL),
                    flatten_w3c_tracestate_payload_into, json_payload);

    nro_set_hash_string(json_payload, "tracestate.tenant_id",
                        trusted_account_key);
    nro_set_hash_string(
        json_payload, "tracingVendors",
        nro_get_hash_string(w3c_payload, "tracingVendors", NULL));
    nro_set_hash(json_payload, "newrelic.v",
                 nro_get_hash_value(nr_payload, "v", NULL));

    /*
     * With flatten_w3c_* applied, attributes on an outbound payload
     * can be compared the same way as attributes on intrinsics events.
     */
    test_txn_dt_cross_agent_intrinsics(testname, "outbound payload",
                                       json_payload, spec);

    nr_free(segment.id);
    nro_delete(nr_payload);
    nro_delete(json_payload);
    nro_delete(w3c_payload);
    nr_free(payload);
    nr_free(traceparent);
    nr_free(tracestate);
  }

  txn->segment_root->start_time = 1000;
  txn->segment_root->stop_time = 2000;
  txn->segment_count++;
  txn->final_data = nr_segment_tree_finalise(
      txn, NR_MAX_SEGMENTS, NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED, NULL,
      NULL);

  /*
   * Intrinsics.
   */

  /* Initialize transaction event */
  txn_event = nr_txn_event_intrinsics(txn);

  /* Initialize error event */
  {
    nrobj_t* error_data;
    nr_analytics_event_t* error_event_analytics;

    nr_txn_record_error(txn, 100, true, "error", "class", "{}");
    error_event_analytics = nr_error_to_event(txn);
    error_data
        = nro_create_from_json(nr_analytics_event_json(error_event_analytics));
    error_event = nro_copy(nro_get_array_hash(error_data, 1, NULL));

    nro_delete(error_data);
    nr_analytics_event_destroy(&error_event_analytics);
  }

  /* Pull a span event out of the flatbuffer. */
  {
    nrobj_t* span_data;
    nr_flatbuffers_table_t tbl;
    nr_flatbuffer_t* fb;
    nr_aoffset_t events;

    txn->segment_root->name = nr_string_add(txn->trace_strings, txn->name);

    fb = nr_txndata_encode(txn);
    nr_flatbuffers_table_init_root(&tbl, nr_flatbuffers_data(fb),
                                   nr_flatbuffers_len(fb));
    nr_flatbuffers_table_read_i8(&tbl, MESSAGE_FIELD_DATA_TYPE,
                                 MESSAGE_BODY_NONE);
    nr_flatbuffers_table_read_union(&tbl, &tbl, MESSAGE_FIELD_DATA);
    events
        = nr_flatbuffers_table_read_vector(&tbl, TRANSACTION_FIELD_SPAN_EVENTS);
    nr_flatbuffers_table_init(
        &tbl, tbl.data, tbl.length,
        nr_flatbuffers_read_indirect(tbl.data, events).offset);

    span_data = nro_create_from_json(
        (const char*)nr_flatbuffers_table_read_bytes(&tbl, EVENT_FIELD_DATA));

    span_event = nro_copy(nro_get_array_hash(span_data, 1, NULL));

    nro_delete(span_data);
    nr_flatbuffers_destroy(&fb);
  }

  size = nro_getsize(intrinsics_target_events);
  for (int i = 1; i <= size; i++) {
    const char* event_type
        = nro_get_array_string(intrinsics_target_events, i, 0);
    const nrobj_t* intrinsics_type
        = nro_get_hash_value(intrinsics, event_type, NULL);

    if (0 == nr_strcmp(event_type, "Transaction")) {
      test_txn_dt_cross_agent_intrinsics(testname, "transaction event",
                                         txn_event, intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "transaction event",
                                         txn_event, intrinsics_type);
    } else if (0 == nr_strcmp(event_type, "TransactionError")) {
      test_txn_dt_cross_agent_intrinsics(testname, "error event", error_event,
                                         intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "error event", error_event,
                                         intrinsics_type);
    } else if (0 == nr_strcmp(event_type, "Span")) {
      test_txn_dt_cross_agent_intrinsics(testname, "span_event", span_event,
                                         intrinsics_common);
      test_txn_dt_cross_agent_intrinsics(testname, "span event", span_event,
                                         intrinsics_type);
    }
  }
  nro_delete(txn_event);
  nro_delete(span_event);
  nro_delete(error_event);

  /*
   * Metrics.
   *
   * Here we cheat a little bit, as we force a transport type. In the
   * agent, we only set a transport type _after_ headers were
   * successfully accepted, the cross agent test suite assumes that this
   * happens before.
   */
  if (transport_type) {
    nr_distributed_trace_inbound_set_transport_type(txn->distributed_trace,
                                                    transport_type);
    txn->distributed_trace->inbound.set = true;
  }

  nr_txn_create_duration_metrics(txn, 1000, 1000);
  nr_txn_create_error_metrics(txn, "WebTransaction/Action/not_words");
  size = nro_getsize(metrics);
  for (int i = 1; i <= size; i++) {
    const nrobj_t* metric = nro_get_array_array(metrics, i, 0);
    const char* name = nro_get_array_string(metric, 1, 0);
    const nrtime_t count = nro_get_array_int(metric, 2, 0);

    const nrmetric_t* m = nrm_find(txn->unscoped_metrics, name);
    const char* nm = nrm_get_name(txn->unscoped_metrics, m);

    tlib_pass_if_true(testname, 0 != m, "m=%p", m);
    tlib_pass_if_true(testname, 0 == nr_strcmp(nm, name), "nm=%s name=%s", nm,
                      name);
    tlib_pass_if_true(testname, nrm_count(m) == count,
                      "name=%s nrm_count (m)=" NR_TIME_FMT
                      " count=" NR_TIME_FMT,
                      name, nrm_count(m), count);
  }

  nr_hashmap_destroy(&header_map);
  nr_txn_destroy(&txn);
}

static void test_txn_trace_context_cross_agent_tests(void) {
  char* json = 0;
  nrobj_t* array = 0;
  nrotype_t otype;
  int i;
  nrapp_t app = {
      .state = NR_APP_OK,
      .limits = default_app_limits(),
      .rnd = nr_random_create(),
  };
  int size;

  json = nr_read_file_contents(CROSS_AGENT_TESTS_DIR
                               "/distributed_tracing/trace_context.json",
                               10 * 1000 * 1000);
  array = nro_create_from_json(json);
  otype = nro_type(array);
  tlib_pass_if_int_equal("tests valid", (int)NR_OBJECT_ARRAY, (int)otype);

  size = nro_getsize(array);
  for (i = 1; i <= size; i++) {
    const nrobj_t* hash = nro_get_array_hash(array, i, 0);

    test_txn_trace_context_cross_agent_testcase(&app, hash);
  }

  nr_free(app.info.appname);
  nro_delete(app.connect_reply);
  nro_delete(array);
  nr_random_destroy(&app.rnd);
  nr_free(json);
}

static void test_force_single_count(void) {
  nrtxn_t txn;
  const char* name = "Supportability/InstrumentedFunction/zip::zap";

  nr_txn_force_single_count(NULL, NULL);
  nr_txn_force_single_count(NULL, name);

  txn.unscoped_metrics = nrm_table_create(10);

  nr_txn_force_single_count(&txn, NULL);
  tlib_pass_if_int_equal("no metric name", 0,
                         nrm_table_size(txn.unscoped_metrics));

  nr_txn_force_single_count(&txn, name);
  tlib_pass_if_int_equal("metric created", 1,
                         nrm_table_size(txn.unscoped_metrics));
  test_txn_metric_is("metric created", txn.unscoped_metrics, MET_FORCED, name,
                     1, 0, 0, 0, 0, 0);

  nrm_table_destroy(&txn.unscoped_metrics);
}

static void test_fn_supportability_metric(void) {
  char* name;

  name = nr_txn_create_fn_supportability_metric(NULL, NULL);
  tlib_pass_if_str_equal("null params", name,
                         "Supportability/InstrumentedFunction/");
  nr_free(name);

  name = nr_txn_create_fn_supportability_metric("zip::zap", NULL);
  tlib_pass_if_str_equal("full name as first parameter", name,
                         "Supportability/InstrumentedFunction/zip::zap");
  nr_free(name);

  name = nr_txn_create_fn_supportability_metric("zip", NULL);
  tlib_pass_if_str_equal("only function name", name,
                         "Supportability/InstrumentedFunction/zip");
  nr_free(name);

  name = nr_txn_create_fn_supportability_metric("zap", "zip");
  tlib_pass_if_str_equal("function name and class name", name,
                         "Supportability/InstrumentedFunction/zip::zap");
  nr_free(name);
}

static void test_txn_set_attribute(void) {
  nrtxn_t txn;
  char* json;

  txn.attributes = nr_attributes_create(0);

  nr_txn_set_string_attribute(NULL, NULL, NULL);
  nr_txn_set_string_attribute(NULL, nr_txn_request_user_agent, "user agent");
  nr_txn_set_string_attribute(&txn, NULL, "user agent");
  nr_txn_set_string_attribute(&txn, nr_txn_request_user_agent, NULL);
  nr_txn_set_string_attribute(&txn, nr_txn_request_user_agent, "");

  nr_txn_set_long_attribute(NULL, NULL, 0);
  nr_txn_set_long_attribute(NULL, nr_txn_request_content_length, 123);
  nr_txn_set_long_attribute(&txn, NULL, 123);

  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_str_equal("bad params", json, "{\"user\":[],\"agent\":[]}");
  nr_free(json);

  nr_txn_set_string_attribute(&txn, nr_txn_request_user_agent_deprecated, "1");
  nr_txn_set_string_attribute(&txn, nr_txn_request_accept_header, "2");
  nr_txn_set_string_attribute(&txn, nr_txn_request_host, "3");
  nr_txn_set_string_attribute(&txn, nr_txn_request_content_type, "4");
  nr_txn_set_string_attribute(&txn, nr_txn_request_method, "5");
  nr_txn_set_string_attribute(&txn, nr_txn_server_name, "6");
  nr_txn_set_string_attribute(&txn, nr_txn_response_content_type, "7");
  nr_txn_set_string_attribute(&txn, nr_txn_request_user_agent, "8");

  nr_txn_set_long_attribute(&txn, nr_txn_request_content_length, 123);
  nr_txn_set_long_attribute(&txn, nr_txn_response_content_length, 456);

  json = nr_attributes_debug_json(txn.attributes);
  tlib_pass_if_str_equal("attributes successfully added", json,
                         "{\"user\":[],\"agent\":["
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"response.headers.contentLength\",\"value\":456},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"request.headers.contentLength\",\"value\":123},"
                         "{\"dests\":[\"trace\",\"error\"],\"key\":"
                         "\"request.headers.userAgent\",\"value\":\"8\"},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"response.headers.contentType\",\"value\":\"7\"},"
                         "{\"dests\":[\"trace\",\"error\"],\"key\":\"SERVER_"
                         "NAME\",\"value\":\"6\"},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"request.method\",\"value\":\"5\"},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"request.headers.contentType\",\"value\":\"4\"},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"request.headers.host\",\"value\":\"3\"},"
                         "{\"dests\":[\"event\",\"trace\",\"error\"],\"key\":"
                         "\"request.headers.accept\",\"value\":\"2\"},"
                         "{\"dests\":[\"trace\",\"error\"],\"key\":\"request."
                         "headers.User-Agent\",\"value\":\"1\"}]}");
  nr_free(json);

  nr_attributes_destroy(&txn.attributes);
}

static void test_sql_recording_level(void) {
  nr_tt_recordsql_t level;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->high_security = 0;

  level = nr_txn_sql_recording_level(NULL);
  tlib_pass_if_equal("NULL pointer returns NR_SQL_NONE", NR_SQL_NONE, level,
                     nr_tt_recordsql_t, "%d");

  txn->high_security = 0;
  txn->options.tt_recordsql = NR_SQL_RAW;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("Raw recording level", NR_SQL_RAW, level,
                     nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("High security overrides raw SQL mode", NR_SQL_OBFUSCATED,
                     level, nr_tt_recordsql_t, "%d");

  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  txn->high_security = 0;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("Obfuscated SQL with no high security mode",
                     NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("Obfuscated SQL with high security mode",
                     NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  txn->options.tt_recordsql = NR_SQL_NONE;
  txn->high_security = 0;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("SQL recording disabled, no high security mode.",
                     NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  txn->high_security = 1;
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("SQL recording disabled, high security mode.", NR_SQL_NONE,
                     level, nr_tt_recordsql_t, "%d");
}

static void test_sql_recording_level_lasp(void) {
  nr_tt_recordsql_t level;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrobj_t* security_policies = nro_new_hash();
  nrobj_t* connect_reply = nro_new_hash();

  txn->high_security = 0;

  /*
   * Prepare the world so I can isolate testing to LASP settings.
   * If the below key/value pairs are not present
   * nr_txn_enforce_security_settings will modify local variables
   * and alter the outcome I expect turning on/off LASP policies.
   */
  nro_set_hash_boolean(connect_reply, "collect_traces", 1);
  nro_set_hash_boolean(connect_reply, "collect_errors", 1);
  nro_set_hash_boolean(connect_reply, "collect_error_events", 1);

  /* Before: NR_SQL_RAW
     LASP Setting: least secure (true)
     Expected: NR_SQL_OBFUSCATED */
  txn->options.tt_recordsql = NR_SQL_RAW;
  nro_set_hash_boolean(security_policies, "record_sql", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal(
      "Raw recording level overridden with obfuscated recording level",
      NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  /* Before: NR_SQL_RAW
     LASP Setting: most secure (false)
     Expected: NR_SQL_NONE */
  txn->options.tt_recordsql = NR_SQL_RAW;
  nro_set_hash_boolean(security_policies, "record_sql", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("Raw recording level overriden with none recording level",
                     NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  /* Before: NR_SQL_OBFUSCATED
     LASP Setting: least secure (true)
     Expected: NR_SQL_OBFUSCATED */
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  nro_set_hash_boolean(security_policies, "record_sql", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("Obfuscated recording level not overridden",
                     NR_SQL_OBFUSCATED, level, nr_tt_recordsql_t, "%d");

  /* Before: NR_SQL_OBFUSCATED
     LASP Setting: most secure (false)
     Expected: NR_SQL_NONE */
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;
  nro_set_hash_boolean(security_policies, "record_sql", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal(
      "Obfuscated recording level overridden with none recording level",
      NR_SQL_NONE, level, nr_tt_recordsql_t, "%d");

  /* Before: NR_SQL_NONE
     LASP Setting: least secure (true)
     Expected: NR_SQL_NONE */
  txn->options.tt_recordsql = NR_SQL_NONE;
  nro_set_hash_boolean(security_policies, "record_sql", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("None recording level not overridden", NR_SQL_NONE, level,
                     nr_tt_recordsql_t, "%d");

  /* Before: NR_SQL_NONE
     LASP Setting: most secure (false)
     Expected: NR_SQL_NONE */
  txn->options.tt_recordsql = NR_SQL_NONE;
  nro_set_hash_boolean(security_policies, "record_sql", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  level = nr_txn_sql_recording_level(txn);
  tlib_pass_if_equal("None recording level not overridden", NR_SQL_NONE, level,
                     nr_tt_recordsql_t, "%d");

  nro_delete(security_policies);
  nro_delete(connect_reply);
}

static void test_custom_events_lasp(void) {
  const char* json;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;
  nrobj_t* security_policies = nro_new_hash();
  nrobj_t* connect_reply = nro_new_hash();
  const char* type = "my_event_type";
  nrobj_t* params = nro_create_from_json("{\"a\":\"x\",\"b\":\"z\"}");
  nrtime_t now = 123 * NR_TIME_DIVISOR;

  txn->custom_events = nr_analytics_events_create(10);
  txn->status.recording = 1;
  txn->high_security = 0;

  /*
   * Prepare the world so I can isolate testing to LASP settings.
   * If the below key/value pairs are not present
   * nr_txn_enforce_security_settings will modify local variables
   * and alter the outcome I expect turning on/off LASP policies.
   */
  nro_set_hash_boolean(connect_reply, "collect_traces", 1);
  nro_set_hash_boolean(connect_reply, "collect_errors", 1);
  nro_set_hash_boolean(connect_reply, "collect_error_events", 1);

  /* Before: Enabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.custom_events_enabled = true;
  nro_set_hash_boolean(security_policies, "custom_events", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_custom_event_internal(txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn->custom_events, 0);
  tlib_pass_if_null("not recording", json);

  /* Before: Disabled
     LASP Setting: least secure (true)
     Expected: Disabled */
  txn->options.custom_events_enabled = false;
  nro_set_hash_boolean(security_policies, "custom_events", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_custom_event_internal(txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn->custom_events, 0);
  tlib_pass_if_null("not recording", json);

  /* Before: Disabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.custom_events_enabled = false;
  nro_set_hash_boolean(security_policies, "custom_events", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_custom_event_internal(txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn->custom_events, 0);
  tlib_pass_if_null("not recording", json);

  /* Before: Enabled
     LASP Setting: least secure (true)
     Expected: Enabled */
  txn->options.custom_events_enabled = true;
  nro_set_hash_boolean(security_policies, "custom_events", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_custom_event_internal(txn, type, params, now);
  json = nr_analytics_events_get_event_json(txn->custom_events, 0);
  tlib_pass_if_str_equal(
      "success", json,
      "[{\"type\":\"my_event_type\",\"timestamp\":123.00000},"
      "{\"b\":\"z\",\"a\":\"x\"},{}]");

  nr_analytics_events_destroy(&txn->custom_events);
  nro_delete(params);
  nro_delete(connect_reply);
  nro_delete(security_policies);
}

static void test_custom_parameters_segment(void) {
  nrapp_t app = {.state = NR_APP_OK};
  nrtxnopt_t opts = {.custom_parameters_enabled = true};
  nrtxn_t* txn;
  nr_segment_t* segment;
  nrobj_t* obj = nro_new_int(123);
  nrobj_t* out;
  nr_status_t st;

  /*
   * Setup and start txn and custom segment.
   */
  txn = nr_txn_begin(&app, &opts, NULL);
  txn->options.span_events_enabled = true;
  txn->options.distributed_tracing_enabled = true;
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  segment = nr_segment_start(txn, NULL, NULL);

  /*
   * Add a custom transaction attribute.
   */
  st = nr_txn_add_user_custom_parameter(txn, "my_key", obj);
  tlib_pass_if_status_success("success", st);

  /*
   * Ensure the attribute was added to the current segment.
   */
  out = nr_attributes_user_to_obj(segment->attributes_txn_event,
                                  NR_ATTRIBUTE_DESTINATION_ALL);
  test_obj_as_json("success", out, "{\"my_key\":123}");
  nro_delete(out);

  nro_delete(obj);
  nr_txn_destroy(&txn);
}

static void test_custom_parameters_lasp(void) {
  nr_status_t st;
  nrtxn_t txnv = {0};
  nrtxn_t* txn = &txnv;
  nrobj_t* obj = nro_new_int(123);
  nrobj_t* security_policies = nro_new_hash();
  nrobj_t* connect_reply = nro_new_hash();

  /* For the purpose of this unit test only force LASP
   * to disable, allowing parameters to be added
   * if the security setting isn't set to
   * most secure. */
  txn->high_security = 0;
  txn->lasp = 0;
  txn->attributes = nr_attributes_create(0);

  /*
   * Prepare the world so I can isolate testing to LASP settings.
   * If the below key/value pairs are not present
   * nr_txn_enforce_security_settings will modify local variables
   * and alter the outcome I expect turning on/off LASP policies.
   */
  nro_set_hash_boolean(connect_reply, "collect_traces", 1);
  nro_set_hash_boolean(connect_reply, "collect_errors", 1);
  nro_set_hash_boolean(connect_reply, "collect_error_events", 1);

  /* Before: Enabled
     LASP Setting: least secure (true)
     Expected: Enabled */
  txn->options.custom_parameters_enabled = true;
  nro_set_hash_boolean(security_policies, "custom_parameters", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  st = nr_txn_add_user_custom_parameter(txn, "my_key", obj);
  tlib_pass_if_status_success("success", st);

  /* Before: Disabled
     LASP Setting: least secure (true)
     Expected: Disabled */
  txn->options.custom_parameters_enabled = false;
  nro_set_hash_boolean(security_policies, "custom_parameters", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  st = nr_txn_add_user_custom_parameter(txn, "my_key", obj);
  tlib_pass_if_status_failure("local higher security", st);

  /* Before: Enabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.custom_parameters_enabled = true;
  nro_set_hash_boolean(security_policies, "custom_parameters", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  st = nr_txn_add_user_custom_parameter(txn, "my_key", obj);
  tlib_pass_if_status_failure("server higher security", st);

  /* Before: Disabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.custom_parameters_enabled = false;
  nro_set_hash_boolean(security_policies, "custom_parameters", false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  st = nr_txn_add_user_custom_parameter(txn, "my_key", obj);
  tlib_pass_if_status_failure("both local and server higher security", st);

  nr_attributes_destroy(&txn->attributes);
  nro_delete(obj);
  nro_delete(connect_reply);
  nro_delete(security_policies);
}

static void test_allow_raw_messages_lasp(void) {
  nrtxn_t txnv = {0};
  nrtxn_t* txn = &txnv;
  nrobj_t* security_policies = nro_new_hash();
  nrobj_t* connect_reply = nro_new_hash();

  txn->status.recording = 1;
  txn->options.err_enabled = 1;

  /*
   * Prepare the world so I can isolate testing to LASP settings.
   * If the below key/value pairs are not present
   * nr_txn_enforce_security_settings will modify local variables
   * and alter the outcome I expect turning on/off LASP policies.
   */
  nro_set_hash_boolean(connect_reply, "collect_traces", 1);
  nro_set_hash_boolean(connect_reply, "collect_errors", 1);
  nro_set_hash_boolean(connect_reply, "collect_error_events", 1);

  /* Before: Enabled
     LASP Setting: least secure (true)
     Expected: Enabled */
  txn->options.allow_raw_exception_messages = true;
  nro_set_hash_boolean(security_policies, "allow_raw_exception_messages", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_error(txn, 2, true, "", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("nr_txn_record_error empty errmsg", 0 == txn->error,
                    "txn->error=%p", txn->error);

  /* Before: Enabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.allow_raw_exception_messages = true;
  nro_set_hash_boolean(security_policies, "allow_raw_exception_messages",
                       false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_error(txn, 4, true, "don't show", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("security setting error message stripped", 0 != txn->error,
                    "txn->error=%p", txn->error);
  tlib_pass_if_true("security setting error message stripped",
                    0
                        == nr_strcmp(NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE,
                                     nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  /* Before: Disabled
     LASP Setting: least secure (true)
     Expected: Disabled */
  txn->options.allow_raw_exception_messages = false;
  nro_set_hash_boolean(security_policies, "allow_raw_exception_messages", true);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_error(txn, 4, true, "don't show", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("security setting error message stripped", 0 != txn->error,
                    "txn->error=%p", txn->error);
  tlib_pass_if_true("security setting error message stripped",
                    0
                        == nr_strcmp(NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE,
                                     nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  /* Before: Disabled
     LASP Setting: most secure (false)
     Expected: Disabled */
  txn->options.allow_raw_exception_messages = false;
  nro_set_hash_boolean(security_policies, "allow_raw_exception_messages",
                       false);
  nr_txn_enforce_security_settings(&txn->options, connect_reply,
                                   security_policies);
  nr_txn_record_error(txn, 4, true, "don't show", "class", "[\"A\",\"B\"]");
  tlib_pass_if_true("security setting error message stripped", 0 != txn->error,
                    "txn->error=%p", txn->error);
  tlib_pass_if_true("security setting error message stripped",
                    0
                        == nr_strcmp(NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE,
                                     nr_error_get_message(txn->error)),
                    "nr_error_get_message (txn->error)=%s",
                    NRSAFESTR(nr_error_get_message(txn->error)));

  nr_error_destroy(&txn->error);
  nro_delete(connect_reply);
  nro_delete(security_policies);
}

static void test_nr_txn_is_current_path_named(void) {
  const char* path_match = "/foo/baz/bar";
  const char* path_not_match = "/not/matched/path";
  nrtxn_t txn;

  nr_memset(&txn, 0, sizeof(txn));

  txn.path = nr_strdup(path_match);

  tlib_pass_if_true(__func__, nr_txn_is_current_path_named(&txn, path_match),
                    "path=%s,txn->path=%s", path_match, txn.path);

  tlib_pass_if_false(__func__,
                     nr_txn_is_current_path_named(&txn, path_not_match),
                     "path=%s,txn->path=%s", path_not_match, txn.path);

  tlib_pass_if_false(__func__, nr_txn_is_current_path_named(&txn, NULL),
                     "path=%s,txn->path=%s", path_not_match, txn.path);

  tlib_pass_if_false(__func__,
                     nr_txn_is_current_path_named(NULL, path_not_match),
                     "path=%s,txn->path=%s", path_not_match, txn.path);

  tlib_pass_if_false(__func__, nr_txn_is_current_path_named(NULL, NULL),
                     "path=%s,txn->path=%s", path_not_match, txn.path);

  nr_txn_destroy_fields(&txn);
}

static void test_create_distributed_trace_payload(void) {
  char* text;
  char* dt_guid;
  nrtxn_t txn;
  nr_segment_t* previous_segment = NULL;
  nr_segment_t* current_segment = NULL;
  nr_stack_t parent_stack;

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  txn.unscoped_metrics = nrm_table_create(0);
  nr_stack_init(&parent_stack, 32);
  txn.parent_stacks = nr_hashmap_create(NULL);
  nr_hashmap_index_set(txn.parent_stacks, 0, &parent_stack);
  txn.distributed_trace = nr_distributed_trace_create();
  txn.rnd = nr_random_create();
  txn.status.recording = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.segment_root = nr_segment_start(&txn, NULL, NULL);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL txn", nr_txn_create_distributed_trace_payload(
                                    NULL, txn.segment_root));
  tlib_pass_if_null("NULL segment",
                    nr_txn_create_distributed_trace_payload(&txn, NULL));
  test_txn_metric_is("NULL segment should increment the exception metric",
                     txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Exception",
                     1, 0, 0, 0, 0, 0);

  /*
   * Test : Distributed tracing disabled.
   */
  txn.options.distributed_tracing_enabled = 0;
  tlib_pass_if_null("disabled", nr_txn_create_distributed_trace_payload(
                                    &txn, txn.segment_root));
  test_txn_metric_is("exception", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Exception",
                     2, 0, 0, 0, 0, 0);

  txn.options.distributed_tracing_enabled = true;

  /*
   * Test : Distributed tracing pointer is NULL.
   */
  txn.options.span_events_enabled = true;
  tlib_pass_if_null("enabled", nr_txn_create_distributed_trace_payload(
                                   &txn, txn.segment_root));
  test_txn_metric_is("exception", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Exception",
                     3, 0, 0, 0, 0, 0);

  /*
   * Test : Valid distributed trace, span events off, transaction events on.
   */
  txn.options.span_events_enabled = false;
  txn.options.analytics_events_enabled = true;
  nr_txn_set_guid(&txn, "wombat");
  text = nr_txn_create_distributed_trace_payload(&txn, txn.segment_root);
  tlib_fail_if_null("valid guid wombat", nr_strstr(text, "\"tx\":\"wombat\""));
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Success", 1,
                     0, 0, 0, 0, 0);
  nr_free(text);

  /*
   * Test : Valid distributed trace, span events on, transaction events off.
   */
  txn.options.span_events_enabled = true;
  txn.options.analytics_events_enabled = false;
  txn.status.recording = true;
  current_segment = nr_segment_start(&txn, NULL, NULL);
  nr_txn_set_guid(&txn, "kangaroos");
  text = nr_txn_create_distributed_trace_payload(&txn, current_segment);
  tlib_fail_if_null("valid guid kangaroos",
                    nr_strstr(text, "\"tx\":\"kangaroos\""));
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Success", 2,
                     0, 0, 0, 0, 0);
  tlib_pass_if_null("The guid should be empty when dt sampled is off",
                    current_segment->id);
  nr_free(text);

  /*
   * Test : Create two payloads in the same segment.
   */
  txn.distributed_trace->sampled = true;

  text = nr_txn_create_distributed_trace_payload(&txn, current_segment);
  tlib_fail_if_null("The segment ID should be set when DT sampled is on",
                    current_segment->id);
  tlib_pass_if_true("The segment priority should be set  when DT sampled is on",
                    current_segment->priority & NR_SEGMENT_PRIORITY_DT,
                    "priority=0x%08x", current_segment->priority);
  dt_guid = nr_strdup(current_segment->id);
  nr_free(text);

  text = nr_txn_create_distributed_trace_payload(&txn, current_segment);
  tlib_pass_if_str_equal("The segment id should be the same",
                         current_segment->id, dt_guid);
  test_segment_end_and_keep(&current_segment);
  nr_free(text);
  nr_free(dt_guid);

  /*
   * Test : Create a payload in the next segment.
   *
   * +--------------------------------+
   * |          Root Segment          |
   * +--------------------------------+
   * |   Segment 1      |  Segment 2  |
   * +--------------------------------+
   *          ^ ^            ^
   *          1 2            3
   *          Payload creation
   */
  previous_segment = current_segment;
  current_segment = nr_segment_start(&txn, NULL, NULL);
  text = nr_txn_create_distributed_trace_payload(&txn, current_segment);
  tlib_fail_if_str_equal("There should be a new id on the new segment",
                         current_segment->id, previous_segment->id);
  nr_free(text);

  /*
   * Test : Valid distributed trace setup.
   *
   * We'll only check the parameters we set here (namely the GUID); the rest can
   * be tested within test_distributed_trace.c.
   */
  txn.options.span_events_enabled = true;
  txn.options.analytics_events_enabled = true;
  txn.distributed_trace->sampled = true;
  nr_txn_set_guid(&txn, "guid");
  text = nr_txn_create_distributed_trace_payload(&txn, current_segment);
  tlib_fail_if_null("valid text", text);
  tlib_fail_if_null("valid guid", nr_strstr(text, "\"tx\":\"guid\""));
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/CreatePayload/Success", 6,
                     0, 0, 0, 0, 0);
  nr_free(text);

  /*
   * Test : Segment with a different transaction.
   */
  current_segment->txn = NULL;
  tlib_pass_if_null(
      "a different segment transaction should fail",
      nr_txn_create_distributed_trace_payload(&txn, current_segment));
  current_segment->txn = &txn;

  nr_random_destroy(&txn.rnd);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_hashmap_destroy(&txn.parent_stacks);
  nr_stack_destroy_fields(&parent_stack);
  nr_txn_destroy_fields(&txn);
}

static void test_create_w3c_tracestate_header(void) {
  nr_segment_t* segment = NULL;
  nrtxn_t txn = {0};
  char* actual = NULL;
  char* expected = NULL;

  /*
   * Test : Invalid parameters
   */
  tlib_pass_if_null("everything is null",
                    nr_txn_create_w3c_tracestate_header(NULL, NULL));

  /*
   * Test : valid segment NULL transaction
   */
  segment = nr_malloc(sizeof(nr_segment_t));
  segment->id = NULL;
  tlib_pass_if_null("txn is null",
                    nr_txn_create_w3c_tracestate_header(NULL, segment));

  /*
   * Test : valid transaction NULL distributed trace
   */
  nr_memset(&txn, 0, sizeof(nrtxn_t));

  txn.options.span_events_enabled = true;
  txn.status.recording = true;
  tlib_pass_if_null("dt is null",
                    nr_txn_create_w3c_tracestate_header(&txn, segment));

  txn.distributed_trace = nr_distributed_trace_create();
  txn.distributed_trace->sampled = true;
  txn.distributed_trace->trusted_key = nr_strdup("tk");
  txn.distributed_trace->account_id = nr_strdup("accountId");
  txn.distributed_trace->app_id = nr_strdup("appId");
  txn.distributed_trace->priority = .77;

  txn.distributed_trace->txn_id = nr_strdup("txnId");
  segment->id = "spanId";

  /*
   * Test : analytics events off
   */
  txn.options.analytics_events_enabled = false;
  actual = nr_txn_create_w3c_tracestate_header(&txn, segment);
  expected = "tk@nr=0-0-accountId-appId-spanId--1-0.770000";
  tlib_pass_if_not_null("analytic events should not have txnId",
                        nr_strstr(actual, expected));
  nr_free(actual);

  /*
   * Test : analytic events on + span events off
   */
  txn.options.span_events_enabled = false;
  txn.options.analytics_events_enabled = true;
  actual = nr_txn_create_w3c_tracestate_header(&txn, segment);
  expected = "tk@nr=0-0-accountId-appId--txnId-1-0.770000";
  tlib_pass_if_not_null("span events off", nr_strstr(actual, expected));
  nr_free(actual);

  /*
   * Test : NULL spanId and txnId
   */
  txn.options.span_events_enabled = true;
  segment->id = NULL;
  nr_free(txn.distributed_trace->txn_id);
  txn.distributed_trace->txn_id = NULL;
  actual = nr_txn_create_w3c_tracestate_header(&txn, segment);
  expected = "tk@nr=0-0-accountId-appId---1-0.770000";
  tlib_pass_if_not_null("NULL span id and txn id", nr_strstr(actual, expected));
  nr_free(actual);

  nr_free(segment);
  nr_distributed_trace_destroy(&txn.distributed_trace);
}

static void test_create_w3c_traceparent_header(void) {
  char* actual = NULL;
  char* expected = NULL;
  nrtxn_t txn = {0};
  nr_segment_t* segment = NULL;

  /*
   * Test : bad parameters
   */
  tlib_pass_if_null("Null txn and segment should result in a null header",
                    nr_txn_create_w3c_traceparent_header(NULL, segment));

  segment = nr_malloc(sizeof(nr_segment_t));
  segment->id = NULL;

  /*
   * Test : No txn and valid span
   */
  tlib_pass_if_null(
      "A NULL txn and a valid segment should result in a null header",
      nr_txn_create_w3c_traceparent_header(NULL, segment));

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  txn.options.span_events_enabled = true;
  txn.status.recording = true;
  txn.rnd = nr_random_create();
  txn.status.recording = 1;
  txn.unscoped_metrics = nrm_table_create(0);

  /*
   * Test : Null DT
   */
  tlib_pass_if_null("a NULL dt",
                    nr_txn_create_w3c_traceparent_header(&txn, segment));
  test_txn_metric_is("header created", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Create/Exception", 1, 0, 0, 0,
                     0, 0);

  txn.distributed_trace = nr_distributed_trace_create();
  txn.distributed_trace->sampled = true;

  /*
   * Test : No trace id
   */
  actual = nr_txn_create_w3c_traceparent_header(&txn, segment);
  tlib_pass_if_null("no trace id", actual);
  test_txn_metric_is("header created", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Create/Exception", 2, 0, 0, 0,
                     0, 0);

  /*
   * Test : valid string random guid
   */
  nr_distributed_trace_set_trace_id(txn.distributed_trace, "meatballs!");
  actual = nr_txn_create_w3c_traceparent_header(&txn, segment);
  expected = "00-0000000000000000000000meatballs!-";
  tlib_pass_if_not_null("random guid", nr_strstr(actual, expected));
  test_txn_metric_is("header created", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Create/Success", 1, 0, 0, 0,
                     0, 0);
  nr_free(actual);

  /*
   * Test : valid string span guid
   */
  segment->id = nr_strdup("currentspan");
  actual = nr_txn_create_w3c_traceparent_header(&txn, segment);
  expected = "00-0000000000000000000000meatballs!-currentspan-01";
  tlib_pass_if_str_equal("currentspan guid true flag", expected, actual);
  test_txn_metric_is("header created", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Create/Success", 2, 0, 0, 0,
                     0, 0);
  nr_free(actual);

  /*
   * Test : false flag
   */
  txn.distributed_trace->sampled = false;
  actual = nr_txn_create_w3c_traceparent_header(&txn, segment);
  expected = "00-0000000000000000000000meatballs!-currentspan-00";
  tlib_pass_if_str_equal("false flag", expected, actual);
  test_txn_metric_is("header created", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Create/Success", 3, 0, 0, 0,
                     0, 0);
  nr_free(actual);

  nr_free(segment->id);
  nr_free(segment);
  nr_random_destroy(&txn.rnd);
  nr_txn_destroy_fields(&txn);
}

static void test_accept_before_create_distributed_tracing(void) {
  nrtxn_t txn;
  char* text;
  char* json_payload
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";
  const nrtime_t expected_duration = 1234;
  nr_hashmap_t* header_map = nr_hashmap_create(NULL);

  nr_memset(&txn, 0, sizeof(nrtxn_t));

  txn.options.distributed_tracing_enabled = true;
  txn.options.span_events_enabled = true;
  txn.app_connect_reply
      = nro_create_from_json("{\"trusted_account_key\":\"9123\"}");
  txn.status.recording = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.segment_root = nr_segment_start(&txn, NULL, NULL);
  txn.unscoped_metrics = nrm_table_create(0);
  txn.abs_start_time = (nrtime_t)1482959525577 * NR_TIME_DIVISOR_MS
                       + expected_duration * NR_TIME_DIVISOR;

  /*
   * Test : Valid accept before create.
   *
   * Confirm the transaction id of the outbound payload matches the
   * transaction id from the inbound payload.
   */

  // Accept
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_txn_id(txn.distributed_trace, "txnid");
  nr_hashmap_update(header_map, NR_PSTR(NEWRELIC), json_payload);
  nr_txn_accept_distributed_trace_payload(&txn, header_map, NULL);
  test_metric_created("transport duration all", txn.unscoped_metrics,
                      MET_FORCED, expected_duration,
                      "TransportDuration/App/9123/51424/HTTP/all");

  // Create
  text = nr_txn_create_distributed_trace_payload(&txn, txn.segment_root);
  tlib_fail_if_null("valid text", text);
  tlib_fail_if_null("valid transaction id",
                    nr_strstr(text, "\"tr\":\"3221bf09aa0bcf0d\""));
  nr_free(text);
  nr_hashmap_destroy(&header_map);
  nr_txn_destroy_fields(&txn);
}

static void test_nr_txn_add_distributed_tracing_intrinsics(void) {
  nrtxn_t txnv = {.high_security = 0};
  nrtxn_t* txn = &txnv;
  nrobj_t* ob = nro_create_from_json("{}");

  nr_txn_set_guid(txn, "test-guid");

  nr_distributed_trace_set_sampled(txn->distributed_trace, true);

  // exercise null paths to ensure nothing bad happens
  nr_txn_add_distributed_tracing_intrinsics(NULL, NULL);
  nr_txn_add_distributed_tracing_intrinsics(txn, NULL);
  nr_txn_add_distributed_tracing_intrinsics(NULL, ob);

  // perform the real call
  nr_txn_add_distributed_tracing_intrinsics(txn, ob);

  // test that sampled is assigned to intrinsics nrobj_t
  tlib_pass_if_int_equal("Sampled assigned to NRO correctly",
                         nro_get_hash_boolean(ob, "sampled", NULL), true);

  nro_delete(ob);
  nr_distributed_trace_destroy(&txn->distributed_trace);
}

static void test_txn_accept_distributed_trace_payload_metrics(void) {
  nrtxn_t txn = {.unscoped_metrics = nrm_table_create(0)};
  nr_hashmap_t* tc_map = nr_hashmap_create(NULL);

  char* json_payload
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";
  const nrtime_t expected_duration = 1234;
  nr_hashmap_t* header_map = nr_hashmap_create(NULL);
  nr_hashmap_update(header_map, NR_PSTR(NEWRELIC), json_payload);

  txn.options.distributed_tracing_enabled = true;
  txn.app_connect_reply
      = nro_create_from_json("{\"trusted_account_key\":\"9123\"}");
  txn.abs_start_time = (nrtime_t)1482959525577 * NR_TIME_DIVISOR_MS
                       + expected_duration * NR_TIME_DIVISOR;

  /*
   * Test : Successful (web)
   */
  txn.status.background = false;
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, header_map, NULL);
  test_metric_created("transport duration all", txn.unscoped_metrics,
                      MET_FORCED, expected_duration,
                      "TransportDuration/App/9123/51424/HTTP/all");
  test_metric_created("transport duration allWeb", txn.unscoped_metrics,
                      MET_FORCED, expected_duration,
                      "TransportDuration/App/9123/51424/HTTP/allWeb");

  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test : Transport type user-specified (web)
   */
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, header_map, "HTTPS");
  test_metric_created("transport duration all", txn.unscoped_metrics,
                      MET_FORCED, expected_duration,
                      "TransportDuration/App/9123/51424/HTTPS/all");
  test_metric_created("transport duration allWeb", txn.unscoped_metrics,
                      MET_FORCED, expected_duration,
                      "TransportDuration/App/9123/51424/HTTPS/allWeb");

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nrm_table_destroy(&txn.unscoped_metrics);

  /*
   * Background Task with no DT
   */
  txn.status.background = true;
  txn.status.recording = true;

  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.segment_root = nr_segment_start(&txn, NULL, NULL);
  txn.segment_root->exclusive_time = nr_exclusive_time_create(16, 0, 999);

  txn.unscoped_metrics = nrm_table_create(2);
  txn.distributed_trace = nr_distributed_trace_create();

  nr_txn_create_duration_metrics(&txn, 999, 1122);
  test_metric_created("background no exclusive", txn.unscoped_metrics,
                      MET_FORCED, 999, "OtherTransaction/all");
  test_metric_created("background no exclusive", txn.unscoped_metrics,
                      MET_FORCED, 999,
                      "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all");
  test_txn_metric_is(
      "background", txn.unscoped_metrics, MET_FORCED,
      "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther", 1, 999, 999,
      999, 999, 998001);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nrm_table_destroy(&txn.unscoped_metrics);

  /*
   * Background Task with accepted DT and unknown transport type
   */
  txn.unscoped_metrics = nrm_table_create(2);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, header_map, "transport");
  nr_txn_create_duration_metrics(&txn, 999, 1122);
  test_txn_metric_is("background no exclusive", txn.unscoped_metrics,
                     MET_FORCED, "OtherTransaction/all", 1, 999, 999, 999, 999,
                     998001);
  test_txn_metric_is("background", txn.unscoped_metrics, MET_FORCED,
                     "DurationByCaller/App/9123/51424/Unknown/all", 1, 999, 999,
                     999, 999, 998001);
  test_txn_metric_is("background", txn.unscoped_metrics, MET_FORCED,
                     "DurationByCaller/App/9123/51424/Unknown/allOther", 1, 999,
                     999, 999, 999, 998001);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Background Task with accepted DT and unknown transport type
   */
  txn.unscoped_metrics = nrm_table_create(2);
  txn.distributed_trace = nr_distributed_trace_create();

  nr_hashmap_update(tc_map, NR_PSTR(W3C_TRACEPARENT),
                    "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01");
  nr_hashmap_update(tc_map, NR_PSTR(W3C_TRACESTATE),
                    "dd=1235235-13452-knf-456vksc-34vkln");

  nr_txn_accept_distributed_trace_payload(&txn, tc_map, "transport");
  nr_txn_create_duration_metrics(&txn, 999, 1122);
  test_txn_metric_is("trace context optional values", txn.unscoped_metrics,
                     MET_FORCED, "OtherTransaction/all", 1, 999, 999, 999, 999,
                     998001);
  test_txn_metric_is("trace context", txn.unscoped_metrics, MET_FORCED,
                     "DurationByCaller/Unknown/Unknown/Unknown/Unknown/all", 1,
                     999, 999, 999, 999, 998001);
  test_txn_metric_is(
      "trace context", txn.unscoped_metrics, MET_FORCED,
      "DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther", 1, 999, 999,
      999, 999, 998001);
  nrm_table_destroy(&txn.unscoped_metrics);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_hashmap_destroy(&tc_map);

  /*
   * Background Task with no DT and error occured
   */
  txn.unscoped_metrics = nrm_table_create(2);

  nr_txn_create_error_metrics(&txn, "WebTransaction/Action/not_words");

  test_txn_metric_is("background error no dt", txn.unscoped_metrics, MET_FORCED,
                     "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all", 1, 0,
                     0, 0, 0, 0);
  test_txn_metric_is("background error no dt", txn.unscoped_metrics, MET_FORCED,
                     "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther",
                     1, 0, 0, 0, 0, 0);
  nrm_table_destroy(&txn.unscoped_metrics);

  /*
   * Background Task with DT and error occured
   */
  txn.unscoped_metrics = nrm_table_create(2);

  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, header_map, "Other");
  nr_txn_create_error_metrics(&txn, "WebTransaction/Action/not_words");

  test_txn_metric_is("background error with dt", txn.unscoped_metrics,
                     MET_FORCED, "ErrorsByCaller/App/9123/51424/Other/all", 1,
                     0, 0, 0, 0, 0);
  test_txn_metric_is("background error with dt", txn.unscoped_metrics,
                     MET_FORCED, "ErrorsByCaller/App/9123/51424/Other/allOther",
                     1, 0, 0, 0, 0, 0);
  nrm_table_destroy(&txn.unscoped_metrics);

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_segment_destroy_tree(txn.segment_root);
  nr_hashmap_destroy(&txn.parent_stacks);
  nr_stack_destroy_fields(&txn.default_parent_stack);
  nr_slab_destroy(&txn.segment_slab);
  nr_hashmap_destroy(&header_map);
  nro_delete(txn.app_connect_reply);
  nrm_table_destroy(&txn.unscoped_metrics);
}

static void test_txn_accept_distributed_trace_payload_w3c(void) {
  nrtxn_t txn = {0};
  nr_hashmap_t* headers;
  bool rv = true;
  nrtime_t payload_timestamp_ms = 1529445826000;
  nrtime_t txn_timestamp_us = 15214458260000 * NR_TIME_DIVISOR_MS;
  nrtime_t delta_timestamp_us = nr_time_duration(
      (payload_timestamp_ms * NR_TIME_DIVISOR_MS), txn_timestamp_us);
  char* traceparent;

  tlib_fail_if_int64_t_equal("Zero duration", 0, delta_timestamp_us);

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  txn.app_connect_reply = nro_new_hash();
  txn.unscoped_metrics = nrm_table_create(0);
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "123");

#define TEST_TXN_ACCEPT_DT_PAYLOAD_RESET    \
  txn.distributed_trace->inbound.set = 0;   \
  nrm_table_destroy(&txn.unscoped_metrics); \
  txn.unscoped_metrics = nrm_table_create(0);

  headers = nr_hashmap_create(NULL);

  /*
   * Test : All NULL values
   */
  rv = nr_txn_accept_distributed_trace_payload(NULL, NULL, NULL);
  tlib_pass_if_false("All args NULL, accept_w3c should fail", rv,
                     "Return value = %d", (int)rv);

  /*
   * Test : No Txn
   */
  nr_hashmap_set(headers, NR_PSTR("traceparent"),
                 "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01");
  nr_hashmap_set(
      headers, NR_PSTR("tracestate"),
      "123@nr=0-2-account-app-span-transaction-1-1.1273-1569367663277, "
      "am=123-2345-8777-23489-3948");

  rv = nr_txn_accept_distributed_trace_payload(NULL, headers, "HTTP");
  tlib_pass_if_false("No txn, accept_w3c should fail", rv, "Return value = %d",
                     (int)rv);

  /*
   * Test : Txn with no dt
   */
  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");
  tlib_pass_if_false("No dt, accept_w3c should fail", rv, "Return value = %d",
                     (int)rv);

  /*
   * Test : Distributed Tracing off
   */
  txn.distributed_trace = nr_distributed_trace_create();
  txn.options.distributed_tracing_enabled = false;

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");
  tlib_pass_if_false("dt off, accept_w3c should fail", rv, "Return value = %d",
                     (int)rv);
  test_txn_metric_is("exception", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Exception",
                     1, 0, 0, 0, 0, 0);

  /*
   * Test : No Trace Parent
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  txn.options.distributed_tracing_enabled = true;
  nr_hashmap_delete(headers, NR_PSTR("traceparent"));

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");
  tlib_pass_if_false("missing traceparent", rv, "Return value = %d", (int)rv);
  test_txn_metric_is(
      "missing traceparent", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Null", 1, 0, 0, 0,
      0, 0);

  /*
   * Test : Invalid traceparent
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_hashmap_set(headers, NR_PSTR("traceparent"), "00--27ddd2d8890283b4-01");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");
  tlib_pass_if_false("invalid traceparent", rv, "Return value = %d", (int)rv);
  test_txn_metric_is("invalid traceparent", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/TraceParent/Parse/Exception",
                     1, 0, 0, 0, 0, 0);

  /*
   * Test : bad flags
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "cc-12345678901234567890123456789012-1234567890123456-01."
                    "what-the-future-will-be-like");
  nr_hashmap_update(headers, NR_PSTR("tracestate"), NULL);
  txn.distributed_trace->trace_id = NULL;
  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTPS");

  tlib_pass_if_false("The header should be accepted", rv, "Return value = %d",
                     (int)rv);
  tlib_pass_if_null("The trace Id", txn.distributed_trace->trace_id);

  /*
   * Test : bad flags
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-12345678901234567890123456789012-1234567890123456-01-"
                    "what-the-future-will-be-like");
  nr_hashmap_update(headers, NR_PSTR("tracestate"), NULL);
  txn.distributed_trace->trace_id = NULL;
  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTPS");

  tlib_pass_if_false("The header should be accepted", rv, "Return value = %d",
                     (int)rv);
  tlib_pass_if_null("The trace Id", txn.distributed_trace->trace_id);

  /*
   * Test : new version
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "cc-12345678901234567890123456789012-1234567890123456-01-"
                    "what-the-future-will-be-like");
  nr_hashmap_update(headers, NR_PSTR("tracestate"), NULL);
  txn.distributed_trace->priority = 1.333333;
  txn.distributed_trace->sampled = 1;
  txn.distributed_trace->inbound.trusted_parent_id = NULL;
  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTPS");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Transport Type", "HTTPS",
                         txn.distributed_trace->inbound.transport_type);
  tlib_pass_if_str_equal("Span Id", "1234567890123456",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "12345678901234567890123456789012",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_null("Trusted parent is not set",
                    txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_double_equal("No priority", 1.333333,
                            txn.distributed_trace->priority);
  tlib_pass_if_true("Sampled should not have been set",
                    txn.distributed_trace->sampled, "sampled flag = %d",
                    (int)txn.distributed_trace->sampled);
  tlib_pass_if_null("No txn Id", txn.distributed_trace->inbound.txn_id);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);

  /*
   * Test : Missing optionals
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01");
  nr_hashmap_update(headers, NR_PSTR("tracestate"),
                    "123@nr=0-1-theAccount-theApp-----1234567"
                    "1529445826000, dd=1-2-3-4, dt=123-2345-8777-23489-3948");
  txn.distributed_trace->priority = 1.333333;
  txn.distributed_trace->sampled = 1;

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Parent type", "Browser",
                         txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("account Id", "theAccount",
                         txn.distributed_trace->inbound.account_id);
  tlib_pass_if_str_equal("App Id", "theApp",
                         txn.distributed_trace->inbound.app_id);
  tlib_pass_if_str_equal("Tracing Vendors should show the additional vendor",
                         "dd,dt",
                         txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  tlib_pass_if_str_equal("Span Id", "27ddd2d8890283b4",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "74be672b84ddc4e4b28be285632bbc0a",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_null("Trusted parent is not set",
                    txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_double_equal("No priority", 1.333333,
                            txn.distributed_trace->priority);
  tlib_pass_if_true("Sampled should not have been set",
                    txn.distributed_trace->sampled, "sampled flag = %d",
                    (int)txn.distributed_trace->sampled);
  tlib_pass_if_null("No txn Id", txn.distributed_trace->inbound.txn_id);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);

  /*
   * Test : All Values
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01");
  nr_hashmap_update(headers, NR_PSTR("tracestate"),
                    "123@nr=0-2-account-app-span-transaction-1-1.1273-"
                    "1529445826000, am=123-2345-8777-23489-3948");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Parent type", "Mobile",
                         txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("account Id", "account",
                         txn.distributed_trace->inbound.account_id);
  tlib_pass_if_str_equal("App Id", "app",
                         txn.distributed_trace->inbound.app_id);
  tlib_pass_if_str_equal("Span Id", "27ddd2d8890283b4",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "74be672b84ddc4e4b28be285632bbc0a",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_str_equal("Trusted Parent", "span",
                         txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("Transaction Id", "transaction",
                         txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_true("Sampled should be set to true",
                    txn.distributed_trace->sampled, "sampled flag = %d",
                    (int)txn.distributed_trace->sampled);
  tlib_pass_if_double_equal("Priority should be set", 1.1273,
                            txn.distributed_trace->priority);
  tlib_pass_if_long_equal("Compare payload and txn time", delta_timestamp_us,
                          nr_distributed_trace_inbound_get_timestamp_delta(
                              txn.distributed_trace, txn_timestamp_us));
  tlib_pass_if_str_equal("Tracing Vendors should show the additional vendor",
                         "am", txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);
  /*
   * Test : App parent type and NULL transport type
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  txn.status.background = true;
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-74be672b84ddc4e4b28be285632bbc0a-be28566a36addc49-00");
  nr_hashmap_update(headers, NR_PSTR("tracestate"),
                    "123@nr=0-0-account-app-span-transaction-0-0.77-"
                    "1529445826000, 555@nr=1-0-23-234-534-67-456-456");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, NULL);

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Parent type", "App",
                         txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("account Id", "account",
                         txn.distributed_trace->inbound.account_id);
  tlib_pass_if_str_equal("App Id", "app",
                         txn.distributed_trace->inbound.app_id);
  tlib_pass_if_str_equal("Span Id", "be28566a36addc49",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "74be672b84ddc4e4b28be285632bbc0a",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_str_equal("Trusted Parent", "span",
                         txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("Transaction Id", "transaction",
                         txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_false("Sampled should be set to false",
                     txn.distributed_trace->sampled, "sampled flag = %d",
                     (int)txn.distributed_trace->sampled);
  tlib_pass_if_double_equal("Priority should be set", 0.77,
                            txn.distributed_trace->priority);
  tlib_pass_if_long_equal("Compare payload and txn time", delta_timestamp_us,
                          nr_distributed_trace_inbound_get_timestamp_delta(
                              txn.distributed_trace, txn_timestamp_us));
  tlib_pass_if_str_equal("Tracing Vendors should show the additional vendor",
                         "555@nr",
                         txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_str_equal("Transport Type", "Unknown",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);

  /*
   * Test : Non-New Relic traceparent
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  txn.status.background = false;
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00");
  nr_hashmap_update(
      headers, NR_PSTR("tracestate"),
      "dd=YzRiMTIxODk1NmVmZTE4ZQ,123@nr=0-0-33-5043-"
      "27ddd2d8890283b4-5569065a5b1313bd-1-1.23456-1518469636025");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Parent type", "App",
                         txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("account Id", "33",
                         txn.distributed_trace->inbound.account_id);
  tlib_pass_if_str_equal("App Id", "5043",
                         txn.distributed_trace->inbound.app_id);
  tlib_pass_if_str_equal("Span Id", "7d3efb1b173fecfa",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "87b1c9a429205b25e5b687d890d4821f",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_str_equal("Trusted Parent", "27ddd2d8890283b4",
                         txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("Transaction Id", "5569065a5b1313bd",
                         txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_true("Sampled should be set to true",
                    txn.distributed_trace->sampled, "sampled flag = %d",
                    (int)txn.distributed_trace->sampled);
  tlib_pass_if_double_equal("Priority should be set", 1.23456,
                            txn.distributed_trace->priority);
  tlib_pass_if_str_equal("Tracing Vendors should show the additional vendor",
                         "dd", txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);

  /*
   * Test : No tracestate
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00");
  nr_hashmap_delete(headers, NR_PSTR("tracestate"));

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Span Id", "7d3efb1b173fecfa",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "87b1c9a429205b25e5b687d890d4821f",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_null("No App Id", txn.distributed_trace->inbound.app_id);
  tlib_pass_if_null("No account id", txn.distributed_trace->inbound.account_id);
  tlib_pass_if_null("parent type should be null",
                    txn.distributed_trace->inbound.type);
  tlib_pass_if_null("No tracing vendors",
                    txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_null("Trusted parent is not set",
                    txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_null("No txn Id", txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("traceparent accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);
  test_txn_metric_is("no tracestate header", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/TraceState/NoNrEntry", 1, 0,
                     0, 0, 0, 0);
  /*
   * Test : No NR tracestate
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00");
  nr_hashmap_set(headers, NR_PSTR("tracestate"), "dd=YzRiMTIxODk1NmVmZTE4ZQ");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Span Id", "7d3efb1b173fecfa",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "87b1c9a429205b25e5b687d890d4821f",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_null("No App Id", txn.distributed_trace->inbound.app_id);
  tlib_pass_if_null("No account id", txn.distributed_trace->inbound.account_id);
  tlib_pass_if_null("parent type should be null",
                    txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("tracing vendor for Non NR tracestate", "dd",
                         txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_null("Trusted parent is not set",
                    txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_null("No txn Id", txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("tracestate accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);
  test_txn_metric_is("no NR tracestate entry", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/TraceState/NoNrEntry", 1, 0,
                     0, 0, 0, 0);

  /*
   * Test : Invalid tracestate
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_hashmap_update(headers, NR_PSTR("traceparent"),
                    "00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00");
  nr_hashmap_update(headers, NR_PSTR("tracestate"),
                    "dd=YzRiMTIxODk1NmVmZTE4ZQ,123@nr=invalid");

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Span Id", "7d3efb1b173fecfa",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "87b1c9a429205b25e5b687d890d4821f",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_null("No App Id", txn.distributed_trace->inbound.app_id);
  tlib_pass_if_null("No account id", txn.distributed_trace->inbound.account_id);
  tlib_pass_if_null("parent type should be null",
                    txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("tracing vendor for Non NR tracestate", "dd",
                         txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_null("Trusted parent is not set",
                    txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_null("No txn Id", txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("traceparent accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);
  test_txn_metric_is("tracestate invalid NR entry", txn.unscoped_metrics,
                     MET_FORCED,
                     "Supportability/TraceContext/TraceState/InvalidNrEntry", 1,
                     0, 0, 0, 0, 0);

  /*
   * Test : Multiple accepts
   */
  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_false("multiple accepts", rv, "Return value = %d", (int)rv);
  test_txn_metric_is(
      "multiple accepts", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple", 1, 0, 0,
      0, 0, 0);

  /*
   * Test : Accept after create
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_distributed_trace_destroy(&txn.distributed_trace);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_trace_id(txn.distributed_trace, "35ff77");
  traceparent = nr_txn_create_w3c_traceparent_header(&txn, NULL);
  nr_free(traceparent);

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_false("accept after create", rv, "Return value = %d", (int)rv);
  test_txn_metric_is("accepts after create", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Ignored/"
                     "CreateBeforeAccept",
                     1, 0, 0, 0, 0, 0);

  nr_txn_destroy_fields(&txn);
  nr_hashmap_destroy(&headers);
}

static void test_txn_accept_distributed_trace_payload_w3c_and_nr(void) {
  nrtxn_t txn = {0};
  nr_hashmap_t* headers;
  bool rv = true;
  nrtime_t payload_timestamp_ms = 1529445826000;
  nrtime_t txn_timestamp_us = 15214458260000 * NR_TIME_DIVISOR_MS;
  nrtime_t delta_timestamp_us = nr_time_duration(
      (payload_timestamp_ms * NR_TIME_DIVISOR_MS), txn_timestamp_us);

  char* nr_payload_trusted_key
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577, \
      \"tk\": \"123\" \
    } \
  }";

  tlib_fail_if_int64_t_equal("Zero duration", 0, delta_timestamp_us);

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  txn.app_connect_reply = nro_new_hash();
  txn.unscoped_metrics = nrm_table_create(0);
  txn.options.distributed_tracing_enabled = true;
  txn.distributed_trace = nr_distributed_trace_create();
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "123");

  headers = nr_hashmap_create(NULL);

  /*
   * Test : W3C headers override NR values
   */
  nr_hashmap_set(headers, NR_PSTR("traceparent"),
                 "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01");
  nr_hashmap_set(headers, NR_PSTR("tracestate"),
                 "123@nr=0-2-account-app-span-transaction-1-1.1273-"
                 "1529445826000, am=123-2345-8777-23489-3948");
  nr_hashmap_set(headers, NR_PSTR("newrelic"), nr_payload_trusted_key);

  rv = nr_txn_accept_distributed_trace_payload(&txn, headers, "HTTP");

  tlib_pass_if_true("The header should be accepted", rv, "Return value = %d",
                    (int)rv);
  tlib_pass_if_str_equal("Parent type", "Mobile",
                         txn.distributed_trace->inbound.type);
  tlib_pass_if_str_equal("account Id", "account",
                         txn.distributed_trace->inbound.account_id);
  tlib_pass_if_str_equal("App Id", "app",
                         txn.distributed_trace->inbound.app_id);
  tlib_pass_if_str_equal("Span Id", "27ddd2d8890283b4",
                         txn.distributed_trace->inbound.guid);
  tlib_pass_if_str_equal("The trace Id", "74be672b84ddc4e4b28be285632bbc0a",
                         txn.distributed_trace->trace_id);
  tlib_pass_if_str_equal("Trusted Parent", "span",
                         txn.distributed_trace->inbound.trusted_parent_id);
  tlib_pass_if_str_equal("Transaction Id", "transaction",
                         txn.distributed_trace->inbound.txn_id);
  tlib_pass_if_true("Sampled should be set to true",
                    txn.distributed_trace->sampled, "sampled flag = %d",
                    (int)txn.distributed_trace->sampled);
  tlib_pass_if_double_equal("Priority should be set", 1.1273,
                            txn.distributed_trace->priority);
  tlib_pass_if_long_equal("Compare payload and txn time", delta_timestamp_us,
                          nr_distributed_trace_inbound_get_timestamp_delta(
                              txn.distributed_trace, txn_timestamp_us));
  tlib_pass_if_str_equal("Tracing Vendors should show the additional vendor",
                         "am", txn.distributed_trace->inbound.tracing_vendors);
  tlib_pass_if_str_equal("Transport Type", "HTTP",
                         txn.distributed_trace->inbound.transport_type);
  test_txn_metric_is("headers accepted", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/TraceContext/Accept/Success", 1, 0, 0, 0,
                     0, 0);

  nr_txn_destroy_fields(&txn);
  nr_hashmap_destroy(&headers);
}

static void test_txn_accept_distributed_trace_payload(void) {
  nrtxn_t txn;
  char* create_payload;
  char* json_payload
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";
  nr_hashmap_t* map_payload = nr_hashmap_create(NULL);

  char* json_payload_wrong_version
      = "{ \
    \"v\": [2,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";
  nr_hashmap_t* map_payload_wrong_version = nr_hashmap_create(NULL);

  char* json_payload_trusted_key
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577, \
      \"tk\": \"1010\" \
    } \
  }";
  nr_hashmap_t* map_payload_trusted_key = nr_hashmap_create(NULL);

  char* malformed_payload = "Jason P. Ayload";
  nr_hashmap_t* map_payload_malformed = nr_hashmap_create(NULL);

  nr_hashmap_t* map_empty = nr_hashmap_create(NULL);
  nr_hashmap_t* map_no_nr_headers = nr_hashmap_create(NULL);
  nr_hashmap_t* map_mixed_headers = nr_hashmap_create(NULL);

  nr_hashmap_set(map_payload, NR_PSTR(NEWRELIC), json_payload);
  nr_hashmap_set(map_payload_wrong_version, NR_PSTR(NEWRELIC),
                 json_payload_wrong_version);
  nr_hashmap_set(map_payload_trusted_key, NR_PSTR(NEWRELIC),
                 json_payload_trusted_key);
  nr_hashmap_set(map_payload_malformed, NR_PSTR(NEWRELIC), malformed_payload);
  nr_hashmap_set(map_no_nr_headers, NR_PSTR("oldrelic"), json_payload);
  nr_hashmap_set(map_mixed_headers, NR_PSTR(NEWRELIC),
                 json_payload_trusted_key);
  nr_hashmap_set(map_mixed_headers, NR_PSTR("oldrelic"),
                 json_payload_wrong_version);

#define TEST_TXN_ACCEPT_DT_PAYLOAD_RESET    \
  txn.distributed_trace->inbound.set = 0;   \
  nrm_table_destroy(&txn.unscoped_metrics); \
  txn.unscoped_metrics = nrm_table_create(0);

  nr_memset(&txn, 0, sizeof(nrtxn_t));
  txn.unscoped_metrics = nrm_table_create(0);
  txn.app_connect_reply = nro_new_hash();
  txn.status.recording = 1;
  txn.segment_slab = nr_slab_create(sizeof(nr_segment_t), 0);
  txn.segment_root = nr_segment_start(&txn, NULL, NULL);

  /*
   * Test : Bad parameters.  Make sure nothing explodes
   * when NULL or DT-less txn is passed in
   */
  nr_txn_accept_distributed_trace_payload(NULL, NULL, NULL);
  nr_txn_accept_distributed_trace_payload(&txn, NULL, NULL);

  /*
   * Test : Distributed tracing disabled
   */
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, NULL, NULL);
  test_txn_metric_is("exception", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Exception",
                     1, 0, 0, 0, 0, 0);

  txn.options.distributed_tracing_enabled = true;
  txn.options.span_events_enabled = true;

  /*
   * Test : NULL Payload
   */
  nr_txn_accept_distributed_trace_payload(&txn, NULL, NULL);
  test_txn_metric_is(
      "null", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Null", 1, 0, 0, 0,
      0, 0);

  /*
   * Test : Empty Header Map
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_txn_accept_distributed_trace_payload(&txn, map_empty, NULL);
  test_txn_metric_is(
      "null", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Null", 1, 0, 0, 0,
      0, 0);

  /*
   * Test : No "newrelic" Header in Non-Empty Map
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_txn_accept_distributed_trace_payload(&txn, map_no_nr_headers, NULL);
  test_txn_metric_is(
      "null", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Null", 1, 0, 0, 0,
      0, 0);

  /*
   * Test : Malformed Payload
   */
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_malformed, NULL);
  test_txn_metric_is(
      "parse exception", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/ParseException", 1, 0, 0,
      0, 0, 0);

  /*
   * Test : Wrong major version in payload
   */
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_wrong_version,
                                          NULL);
  test_txn_metric_is(
      "major version", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/MajorVersion", 1,
      0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, no trusted accouts defined
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  test_txn_metric_is(
      "untrusted account", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/UntrustedAccount",
      1, 0, 0, 0, 0, 0);
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_trusted_key, NULL);
  test_txn_metric_is(
      "untrusted account", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/UntrustedAccount",
      1, 0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, trust key does not match trusted account key
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9090");
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_trusted_key, NULL);
  test_txn_metric_is(
      "untrusted account", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/UntrustedAccount",
      1, 0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, trusted key does not match account id
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "0007");
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  test_txn_metric_is(
      "untrusted account", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/UntrustedAccount",
      1, 0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, transaction type set correctly.
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  txn.type = 0;
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "1010");
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_trusted_key, NULL);
  tlib_pass_if_true("expected transaction type",
                    txn.type & NR_TXN_TYPE_DT_INBOUND, "txn.type=%d", txn.type);

  /*
   * Test : Valid Payload, trust key matches trusted_account_key
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "1010");
  nr_txn_accept_distributed_trace_payload(&txn, map_payload_trusted_key, NULL);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, account trusted, and non-newrelic headers also
   * present in header map
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "1010");
  nr_txn_accept_distributed_trace_payload(&txn, map_mixed_headers, NULL);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);

  /*
   * Test : Valid Payload, account id matches trusted_account_key
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9123");
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);

  /*
   * Test : Multiple accepts
   */
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  test_txn_metric_is(
      "multiple", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple", 1, 0, 0,
      0, 0, 0);
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test: Create before accept
   */
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_txn_id(txn.distributed_trace, "txnid");
  create_payload
      = nr_txn_create_distributed_trace_payload(&txn, txn.segment_root);

  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  test_txn_metric_is("create before accept", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/"
                     "Ignored/CreateBeforeAccept",
                     1, 0, 0, 0, 0, 0);
  nr_free(create_payload);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nrm_table_destroy(&txn.unscoped_metrics);
  txn.unscoped_metrics = nrm_table_create(0);

  /*
   * Test : Transport type unknown (non-web)
   */
  txn.status.background = true;
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  tlib_pass_if_str_equal(
      "txn is background", "Unknown",
      nr_distributed_trace_inbound_get_transport_type(txn.distributed_trace));
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test : Transport type user-defined (non-web)
   */
  txn.status.background = true;
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, "HTTP");
  tlib_pass_if_str_equal(
      "txn is background", "HTTP",
      nr_distributed_trace_inbound_get_transport_type(txn.distributed_trace));
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test : Transport type unknown (web)
   */
  txn.status.background = false;
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  tlib_pass_if_str_equal(
      "txn is http", "HTTP",
      nr_distributed_trace_inbound_get_transport_type(txn.distributed_trace));
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test : Transport type http (web)
   */
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, NULL);
  tlib_pass_if_str_equal(
      "txn is http", "HTTP",
      nr_distributed_trace_inbound_get_transport_type(txn.distributed_trace));
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /*
   * Test : Transport type user-specified (web)
   */
  txn.distributed_trace = nr_distributed_trace_create();
  nr_txn_accept_distributed_trace_payload(&txn, map_payload, "Other");
  tlib_pass_if_str_equal(
      "txn is http", "Other",
      nr_distributed_trace_inbound_get_transport_type(txn.distributed_trace));

  nr_hashmap_destroy(&map_payload);
  nr_hashmap_destroy(&map_payload_wrong_version);
  nr_hashmap_destroy(&map_payload_trusted_key);
  nr_hashmap_destroy(&map_payload_malformed);
  nr_hashmap_destroy(&map_empty);
  nr_hashmap_destroy(&map_no_nr_headers);
  nr_hashmap_destroy(&map_mixed_headers);
  nr_txn_destroy_fields(&txn);
}

static void test_txn_accept_distributed_trace_payload_httpsafe(void) {
  nrtxn_t txn = {.unscoped_metrics = nrm_table_create(0),
                 .app_connect_reply = nro_new_hash(),
                 .distributed_trace = nr_distributed_trace_create()};

  char* json_payload
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"pr\": 0.1234, \
      \"sa\": false, \
      \"ti\": 1482959525577 \
    } \
  }";
  char* json_payload_encoded
      = nr_b64_encode(json_payload, nr_strlen(json_payload), 0);

  char* invalid_payload = "Jason?  Never heard of him.";
  bool rv;

  nr_hashmap_t* header_map = nr_hashmap_create(NULL);
  nr_hashmap_t* invalid_header_map = nr_hashmap_create(NULL);

  nr_hashmap_set(header_map, NR_PSTR(NEWRELIC), json_payload_encoded);
  nr_hashmap_set(invalid_header_map, NR_PSTR(NEWRELIC), invalid_payload);

#define TEST_TXN_ACCEPT_DT_PAYLOAD_RESET    \
  txn.distributed_trace->inbound.set = 0;   \
  nrm_table_destroy(&txn.unscoped_metrics); \
  txn.unscoped_metrics = nrm_table_create(0);

  txn.options.distributed_tracing_enabled = true;

  /*
   * Test : Bad parameters.  Make sure nothing explodes
   * when NULL or DT-less txn is passed in
   */
  nr_txn_accept_distributed_trace_payload_httpsafe(NULL, NULL, NULL);
  nr_txn_accept_distributed_trace_payload_httpsafe(&txn, NULL, NULL);

  /*
   * Test : Malformed Payload
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  rv = nr_txn_accept_distributed_trace_payload_httpsafe(
      &txn, invalid_header_map, NULL);
  tlib_pass_if_false("expected return code", rv, "rv=%d", rv);
  test_txn_metric_is(
      "parse exception", txn.unscoped_metrics, MET_FORCED,
      "Supportability/DistributedTrace/AcceptPayload/ParseException", 1, 0, 0,
      0, 0, 0);

  /*
   * Test : Valid Payload
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9123");
  rv = nr_txn_accept_distributed_trace_payload_httpsafe(&txn, header_map, NULL);
  tlib_pass_if_true("expected return code", rv, "rv=%d", rv);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);
  /*
   * Test : Trace state but no trace parent
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9123");
  nr_hashmap_set(header_map, NR_PSTR(W3C_TRACESTATE),
                 "9123@nr=0-0-33-5043-27ddd2d8890283b4-5569065a5b1313bd-1-1."
                 "23456-1518469636025");
  rv = nr_txn_accept_distributed_trace_payload_httpsafe(&txn, header_map, NULL);
  tlib_pass_if_true("expected return code", rv, "rv=%d", rv);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);
  /*
   * Test : W3C header was used.
   */
  TEST_TXN_ACCEPT_DT_PAYLOAD_RESET
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9123");
  nr_hashmap_set(header_map, NR_PSTR(W3C_TRACEPARENT),
                 "00-87b1c9a429205b25e5b687d890d4821f-5569065a5b1313bd-00");
  rv = nr_txn_accept_distributed_trace_payload_httpsafe(&txn, header_map, NULL);
  tlib_pass_if_true("expected return code", rv, "rv=%d", rv);
  tlib_pass_if_str_equal("The W3C header should have been accepted",
                         "5569065a5b1313bd",
                         txn.distributed_trace->inbound.guid);

  nr_free(json_payload_encoded);
  nr_hashmap_destroy(&header_map);
  nr_hashmap_destroy(&invalid_header_map);
  nr_distributed_trace_destroy(&txn.distributed_trace);
  nro_delete(txn.app_connect_reply);
  nrm_table_destroy(&txn.unscoped_metrics);
}

static bool null_batch_handler(nr_span_encoding_result_t* result, void* count) {
  nr_span_encoding_result_deinit(result);
  if (count) {
    uint64_t* batch_count = (uint64_t*)count;
    *batch_count += 1;
  }

  return true;
}

static void test_should_create_span_events(void) {
  nrtxn_t txn;
  nr_span_queue_t* queue = nr_span_queue_create(1000, 1 * NR_TIME_DIVISOR,
                                                null_batch_handler, NULL);

  const struct {
    bool distributed_tracing_enabled;
    bool span_events_enabled;
    bool sampled;
    nr_span_queue_t* span_queue;
    bool expected_result;
  } scenarios[] = {
      /* { DT enabled, span events enabled, sampled, span queue, result } */
      {false, false, false, NULL, false},  {false, false, true, NULL, false},
      {false, true, false, NULL, false},   {false, true, true, NULL, false},
      {true, false, false, NULL, false},   {true, false, true, NULL, false},
      {true, true, false, NULL, false},    {true, true, true, NULL, true},
      {false, false, false, queue, false}, {false, false, true, queue, false},
      {false, true, false, queue, false},  {false, true, true, queue, false},
      {true, false, false, queue, false},  {true, false, true, queue, false},
      {true, true, false, queue, true},    {true, true, true, queue, true},
  };

  txn.distributed_trace = nr_distributed_trace_create();

  for (size_t i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
    txn.options.distributed_tracing_enabled
        = scenarios[i].distributed_tracing_enabled;
    txn.options.span_events_enabled = scenarios[i].span_events_enabled;
    txn.span_queue = scenarios[i].span_queue;
    nr_distributed_trace_set_sampled(txn.distributed_trace,
                                     scenarios[i].sampled);
    tlib_pass_if_true(
        __func__,
        nr_txn_should_create_span_events(&txn) == scenarios[i].expected_result,
        "dt=%d,spans=%d,sampled=%d,queue=%p,result=%d",
        scenarios[i].distributed_tracing_enabled,
        scenarios[i].span_events_enabled, scenarios[i].sampled,
        scenarios[i].span_queue, scenarios[i].expected_result);
  }

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nr_span_queue_destroy(&queue);
}

static void test_txn_accept_distributed_trace_payload_optionals(void) {
  char* json_payload_missing
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"ti\": 1482959525577 \
    } \
  }";
  char* json_payload_invalid
      = "{ \
    \"v\": [0,1],   \
    \"d\": {        \
      \"ty\": \"App\", \
      \"ac\": \"9123\", \
      \"ap\": \"51424\", \
      \"id\": \"27856f70d3d314b7\", \
      \"pr\": null, \
      \"tr\": \"3221bf09aa0bcf0d\", \
      \"sa\": null, \
      \"ti\": 1482959525577 \
    } \
  }";
  nr_hashmap_t* header_map = nr_hashmap_create(NULL);
  bool rv;
  nrtxn_t txn = {0};
  const nr_sampling_priority_t priority = 0.1;

  txn.options.distributed_tracing_enabled = 1;
  txn.app_connect_reply = nro_new_hash();
  nro_set_hash_string(txn.app_connect_reply, "trusted_account_key", "9123");

  /*
   * Accept a payload with no priority ("pr") and sampling ("sa") fields without
   * changing the priority and sampling values.
   */

  txn.unscoped_metrics = nrm_table_create(0);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_priority(txn.distributed_trace, priority);
  nr_distributed_trace_set_sampled(txn.distributed_trace, true);

  nr_hashmap_update(header_map, NR_PSTR(NEWRELIC), json_payload_missing);
  rv = nr_txn_accept_distributed_trace_payload(&txn, header_map, NULL);
  tlib_pass_if_true("expected return code", rv, "rv=%d", rv);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);

  tlib_pass_if_double_equal(
      "Unaltered priority",
      nr_distributed_trace_get_priority(txn.distributed_trace), priority);
  tlib_pass_if_bool_equal(
      "Unaltered sampled",
      nr_distributed_trace_is_sampled(txn.distributed_trace), true);

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nrm_table_destroy(&txn.unscoped_metrics);

  /*
   * Accept a payload with invalid priority ("pr") and sampling ("sa") fields
   * without changing the priority and sampling values.
   */

  txn.unscoped_metrics = nrm_table_create(0);
  txn.distributed_trace = nr_distributed_trace_create();
  nr_distributed_trace_set_priority(txn.distributed_trace, priority);
  nr_distributed_trace_set_sampled(txn.distributed_trace, false);

  nr_hashmap_update(header_map, NR_PSTR(NEWRELIC), json_payload_invalid);
  rv = nr_txn_accept_distributed_trace_payload(&txn, header_map, NULL);
  tlib_pass_if_true("expected return code", rv, "rv=%d", rv);
  test_txn_metric_is("success", txn.unscoped_metrics, MET_FORCED,
                     "Supportability/DistributedTrace/AcceptPayload/Success", 1,
                     0, 0, 0, 0, 0);

  tlib_pass_if_double_equal(
      "Unaltered priority",
      nr_distributed_trace_get_priority(txn.distributed_trace), priority);
  tlib_pass_if_bool_equal(
      "Unaltered sampled",
      nr_distributed_trace_is_sampled(txn.distributed_trace), false);

  nr_distributed_trace_destroy(&txn.distributed_trace);
  nrm_table_destroy(&txn.unscoped_metrics);

  nr_hashmap_destroy(&header_map);
  nro_delete(txn.app_connect_reply);
}

static void test_parent_stacks(void) {
  nr_segment_t s = {.type = NR_SEGMENT_CUSTOM, .parent = NULL};
  nrtxn_t txn = {.parent_stacks = NULL};

  /*
   * Test : Bad parameters
   */
  tlib_pass_if_null(
      "Getting the current segment for a NULL txn must return NULL",
      nr_txn_get_current_segment(NULL, NULL));

  /* Setting the current segment for a NULL txn must not segfault */
  nr_txn_set_current_segment(NULL, &s);

  /* Setting the current segment for a NULL segment must not segfault */
  nr_txn_set_current_segment(&txn, NULL);

  /* Retiring the current segment for a NULL txn must not seg fault */
  nr_txn_retire_current_segment(NULL, &s);

  /* Retiring the current segment for a NULL segment must not seg fault */
  nr_txn_retire_current_segment(&txn, NULL);

  /* See also: More meaningful unit-tests in test_segment.c.  Starting and
   * ending a segment trigger nr_txn_set_current_segment() and
   * nr_txn_retire_current_segment(). */
}

static void test_force_current_segment(void) {
  nrapp_t app = {.state = NR_APP_OK};
  nrtxnopt_t opts = {0};
  nrtxn_t* txn;
  nr_segment_t* segment_1;
  nr_segment_t* segment_2;
  nr_segment_t segment_stacked = {0};
  nr_segment_t* segment_async;

  /*
   * Setup and start txn.
   */
  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * segment_1 is the current segment in the default context.
   */
  segment_1 = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_ptr_equal("segment_1 is the current segment on default context",
                         segment_1, nr_txn_get_current_segment(txn, NULL));

  /*
   * segment_async is the current segment in the "async" context.
   */
  segment_async = nr_segment_start(txn, NULL, "async");
  tlib_pass_if_ptr_equal(
      "segment_async is the current segment on async context", segment_async,
      nr_txn_get_current_segment(txn, "async"));

  /*
   * Forcing a current segment must change the current segment on the
   * default context, but not on the async context.
   */
  nr_segment_children_init(&segment_stacked.children);
  segment_stacked.txn = txn;
  nr_txn_force_current_segment(txn, &segment_stacked);
  tlib_pass_if_ptr_equal(
      "segment_stacked is the current segment on default context",
      &segment_stacked, nr_txn_get_current_segment(txn, NULL));
  tlib_pass_if_ptr_equal(
      "segment_async is the current segment on async context", segment_async,
      nr_txn_get_current_segment(txn, "async"));

  /*
   * Creating a segment on the default context parents this segment with
   * the forced segment.
   */
  segment_2 = nr_segment_start(txn, NULL, NULL);
  tlib_pass_if_ptr_equal("segment_2 is parented with the forced segment",
                         segment_2->parent, &segment_stacked);
  tlib_pass_if_ptr_equal(
      "segment_stacked is the current segment on default context",
      &segment_stacked, nr_txn_get_current_segment(txn, NULL));
  tlib_pass_if_ptr_equal(
      "segment_async is the current segment on async context", segment_async,
      nr_txn_get_current_segment(txn, "async"));

  nr_segment_end(&segment_2);

  /*
   * Re-setting the forced segment restores default settings.
   */
  nr_txn_force_current_segment(txn, NULL);
  tlib_pass_if_ptr_equal("segment_1 is the current segment on default context",
                         segment_1, nr_txn_get_current_segment(txn, NULL));
  tlib_pass_if_ptr_equal(
      "segment_async is the current segment on async context", segment_async,
      nr_txn_get_current_segment(txn, "async"));

  nr_segment_children_deinit(&segment_stacked.children);
  nr_txn_destroy(&txn);
}

static void test_txn_is_sampled(void) {
  nrtxn_t txn;
  bool scenarios[][3] = {/* { DT enabled, sampled, result } */
                         {false, false, false},
                         {false, true, false},
                         {true, false, false},
                         {true, true, true}};

  txn.distributed_trace = nr_distributed_trace_create();
  for (size_t i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); i++) {
    txn.options.distributed_tracing_enabled = scenarios[i][0];
    nr_distributed_trace_set_sampled(txn.distributed_trace, scenarios[i][1]);
    tlib_pass_if_true(__func__, nr_txn_is_sampled(&txn) == scenarios[i][2],
                      "dt=%d,sampled=%d,result=%d", scenarios[i][0],
                      scenarios[i][1], scenarios[i][2]);
  }
  nr_distributed_trace_destroy(&txn.distributed_trace);

  /* Passing a NULL txn into nr_txn_is_sampled() must return false and
   * not seg fault */
  tlib_pass_if_false(__func__, nr_txn_is_sampled(NULL),
                     "nr_txn_is_sampled(NULL) should return false");
}

static void test_get_current_trace_id(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  char* trace_id;
  nrtxn_t* txn;
  const char* txn_id;
  char paddedid[33] = "0000000000000000";

  /* setup and start txn */
  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  opts.distributed_tracing_enabled = 1;
  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * Test : Bad parameters
   */
  tlib_pass_if_null("no trace id. txn is null",
                    nr_txn_get_current_trace_id(NULL));

  /*
   * Test : Correct trace id
   */
  txn_id = nr_txn_get_guid(txn);
  trace_id = nr_txn_get_current_trace_id(txn);
  tlib_fail_if_null("txn id", txn_id);
  nr_strcat(paddedid, txn_id);
  tlib_pass_if_str_equal("padded txn_id == trace_id", paddedid, trace_id);
  nr_free(trace_id);

  /*
   * Test : Null trace id with DT disabled
   */
  txn->options.distributed_tracing_enabled = 0;
  tlib_pass_if_null("DT is disabled. trace id is null",
                    nr_txn_get_current_trace_id(txn));

  /*
   * Test : Null trace id with null DT
   */
  txn->options.distributed_tracing_enabled = 1;
  nr_distributed_trace_destroy(&txn->distributed_trace);
  tlib_pass_if_null("DT is null. null trace id is returned",
                    nr_txn_get_current_trace_id(txn));

  nr_txn_destroy(&txn);
}

static void test_get_current_span_id(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  nr_segment_t* segment;
  char* span_id;
  nrtxn_t* txn;
  uint32_t priority;

  /* start txn and segment */
  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  opts.distributed_tracing_enabled = 1;
  txn = nr_txn_begin(&app, &opts, NULL);
  segment = nr_segment_start(txn, txn->segment_root, NULL);
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  nr_txn_set_current_segment(txn, segment);

  /*
   * Test : Bad parameters
   */
  tlib_pass_if_null("no span id. txn is null",
                    nr_txn_get_current_span_id(NULL));

  /*
   * Test : disabled span events
   */
  txn->options.span_events_enabled = 0;
  tlib_pass_if_null("span events disabled", nr_txn_get_current_span_id(txn));

  /*
   * Test : span id is created
   */
  txn->options.span_events_enabled = 1;
  span_id = nr_txn_get_current_span_id(txn);
  tlib_fail_if_null("span id is created", span_id);
  nr_free(span_id);

  /*
   * Test : segment priority is set correctly
   */
  priority = segment->priority;
  tlib_pass_if_true("log segment priority", priority & NR_SEGMENT_PRIORITY_LOG,
                    "priority=0x%08x", priority);

  nr_txn_destroy(&txn);
}

static void test_finalize_parent_stacks(void) {
  nrapp_t app;
  nrtxnopt_t opts;
  nr_segment_t* segment_default_1;
  nr_segment_t* segment_default_2;
  nr_segment_t* segment_async_1;
  nr_segment_t* segment_async_2;
  nrtxn_t* txn;
  uint64_t key;

  /*
   * Don't crash on a NULL txn
   */
  nr_txn_finalize_parent_stacks(NULL);

  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * Don't crash on a NULL stack
   */
  key = nr_string_add(txn->trace_strings, "nullstack");
  nr_hashmap_index_set(txn->parent_stacks, key, NULL);
  nr_txn_finalize_parent_stacks(txn);

  /*
   * Start a default and an async segment
   */
  segment_default_1 = nr_segment_start(txn, NULL, NULL);
  segment_default_2 = nr_segment_start(txn, NULL, NULL);
  segment_async_1 = nr_segment_start(txn, NULL, "async");
  segment_async_2 = nr_segment_start(txn, NULL, "async");

  /*
   * Finalize segment stacks. Test that segments have a stop time and
   * that segment stacks are empty.
   */
  nr_txn_finalize_parent_stacks(txn);

  tlib_pass_if_true("segment in default parent stack ended",
                    segment_default_1->stop_time != 0, "stop_time=" NR_TIME_FMT,
                    segment_default_1->stop_time);
  tlib_pass_if_true("segment in default parent stack ended",
                    segment_default_2->stop_time != 0, "stop_time=" NR_TIME_FMT,
                    segment_default_2->stop_time);
  tlib_pass_if_true("segment in async parent stack ended",
                    segment_async_1->stop_time != 0, "stop_time=" NR_TIME_FMT,
                    segment_async_1->stop_time);
  tlib_pass_if_true("segment in async parent stack ended",
                    segment_async_2->stop_time != 0, "stop_time=" NR_TIME_FMT,
                    segment_async_2->stop_time);
  tlib_pass_if_true("root segment not ended", txn->segment_root->stop_time == 0,
                    "stop_time=" NR_TIME_FMT, txn->segment_root->stop_time);

  nr_txn_destroy(&txn);
}

static void test_max_segments_no_limit(void) {
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  size_t num_segments;

  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  nrt_mutex_init(&app.app_lock, 0);

  p->txns_app = &app;

  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * A segment heap must not be initialized.
   */
  tlib_pass_if_null("segment heap not initialized", txn->segment_heap);

  /*
   * Start incredibly many short segments.
   */
  num_segments = 5000;
  for (size_t c = 0; c < num_segments; c++) {
    nr_segment_t* s = nr_segment_start(txn, NULL, NULL);
    nr_segment_end(&s);
  }

  /*
   * End the transaction, to end the root segment.
   */
  nr_txn_end(txn);

  /*
   * The segments that were created plus the root segment.
   */
  tlib_pass_if_size_t_equal("no segments discarded", num_segments + 1,
                            txn->segment_count);

  nr_txn_destroy(&txn);
}

struct test_segment_count {
  size_t count;
};

static nr_segment_iter_return_t test_segment_count_callback(
    nr_segment_t* segment NRUNUSED,
    void* userdata) {
  ((struct test_segment_count*)userdata)->count += 1;

  return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
}

static void test_max_segments_count_tree(void) {
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  struct test_segment_count userdata;

  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  nrt_mutex_init(&app.app_lock, 0);
  opts.max_segments = 1000;

  p->txns_app = &app;

  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * A segment heap must be initialized.
   */
  tlib_pass_if_not_null("segment heap initialized", txn->segment_heap);

  /*
   * Start incredibly many short segments.
   */
  for (size_t c = 0; c < 5000; c++) {
    nr_segment_t* s = nr_segment_start(txn, NULL, NULL);
    nr_segment_end(&s);
  }

  /*
   * End the transaction, to end the root segment.
   */
  nr_txn_end(txn);

  tlib_pass_if_size_t_equal("1000 segments kept", 1000, txn->segment_count);
  tlib_pass_if_size_t_equal("5001 segments allocated", 5001,
                            nr_txn_allocated_segment_count(txn));

  userdata.count = 0;
  nr_segment_iterate(txn->segment_root, test_segment_count_callback, &userdata);
  tlib_pass_if_size_t_equal("1000 segments in the tree", 1000, userdata.count);

  nr_txn_destroy(&txn);
}

static void test_max_segments(void) {
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();
  nrapp_t app;
  nrtxnopt_t opts;
  nr_segment_t* s1;
  nr_segment_t* s2;
  nr_segment_t* s3;
  nr_segment_t* s4;
  nr_segment_t* seg;
  nrtxn_t* txn;

  nr_memset(&app, 0, sizeof(app));
  nr_memset(&opts, 0, sizeof(opts));
  app.state = NR_APP_OK;
  nrt_mutex_init(&app.app_lock, 0);
  opts.max_segments = 3;

  p->txns_app = &app;

  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * A segment heap must be initialized.
   */
  tlib_pass_if_not_null("segment heap initialized", txn->segment_heap);

  /*
   * Start a default and an async segment
   */
  s1 = nr_segment_start(txn, NULL, NULL);
  s2 = nr_segment_start(txn, NULL, NULL);
  s3 = nr_segment_start(txn, NULL, NULL);
  s4 = nr_segment_start(txn, NULL, NULL);

  nr_segment_set_parent(s2, s1);
  nr_segment_set_parent(s3, s1);
  nr_segment_set_parent(s4, s3);

  nr_segment_set_timing(s1, 0, 10000);
  nr_segment_set_timing(s2, 2000, 10000);
  nr_segment_set_timing(s3, 1000, 10000);
  nr_segment_set_timing(s4, 3000, 10000);

  test_segment_end_and_keep(&s4);
  test_segment_end_and_keep(&s3);
  test_segment_end_and_keep(&s2);
  test_segment_end_and_keep(&s1);

  /*
   * End the transaction, to end the root segment.
   */
  nr_txn_end(txn);

  tlib_pass_if_size_t_equal("limited to 3 segments", 3, txn->segment_count);

  /*
   * s1 should be the only child of the root segment.
   */
  tlib_pass_if_size_t_equal(
      "root segment has 1 child", 1,
      nr_segment_children_size(&txn->segment_root->children));

  seg = nr_segment_children_get(&txn->segment_root->children, 0);
  tlib_pass_if_ptr_equal("child of root segment is s1", seg, s1);

  /*
   * s3 should be the only child of s1
   */
  tlib_pass_if_size_t_equal("s1 segment has 1 child", 1,
                            nr_segment_children_size(&seg->children));

  seg = nr_segment_children_get(&seg->children, 0);
  tlib_pass_if_ptr_equal("child of s1 is s3", seg, s3);

  /*
   * s3 should have no children. Thus s2 and s4 were discarded.
   */
  tlib_pass_if_size_t_equal("s3 segment has no children", 0,
                            nr_segment_children_size(&seg->children));

  nr_txn_destroy(&txn);
}

static void test_allocated_segment_count(void) {
  nrapp_t app = {.state = NR_APP_OK};
  nrtxnopt_t opts = {0};
  nr_segment_t* s;
  nrtxn_t* txn;

  /*
   * Bad parameters.
   */
  tlib_pass_if_size_t_equal("0 on NULL txn", 0,
                            nr_txn_allocated_segment_count(NULL));

  /*
   * Initial state.
   */
  txn = nr_txn_begin(&app, &opts, NULL);
  tlib_pass_if_size_t_equal("1 on initialized txn", 1,
                            nr_txn_allocated_segment_count(txn));

  /*
   * Allocating segments.
   */
  nr_segment_start(txn, NULL, NULL);
  s = nr_segment_start(txn, NULL, NULL);

  tlib_pass_if_size_t_equal("3 segments allocated", 3,
                            nr_txn_allocated_segment_count(txn));

  /*
   * Discard segment.
   */
  nr_segment_discard(&s);

  tlib_pass_if_size_t_equal("3 segments allocated", 3,
                            nr_txn_allocated_segment_count(txn));

  /*
   * Allocate another segment.
   */
  nr_segment_start(txn, NULL, NULL);

  tlib_pass_if_size_t_equal("4 segments allocated", 4,
                            nr_txn_allocated_segment_count(txn));

  nr_txn_destroy(&txn);
}

static void test_allocate_segment(void) {
  nrapp_t app = {.state = NR_APP_OK};
  nrtxnopt_t opts = {0};
  nr_segment_t* s;
  nr_segment_t null_segment = {0};
  nrtxn_t* txn;

  /*
   * Bad parameters.
   */
  tlib_pass_if_null("NULL segment on NULL txn", nr_txn_allocate_segment(NULL));

  txn = nr_txn_begin(&app, &opts, NULL);

  /*
   * Allocate an uninitialized segment.
   */
  s = nr_txn_allocate_segment(txn);
  tlib_pass_if_not_null("uninitialized segment", s);
  tlib_pass_if_int_equal("uninitialized segment", 0,
                         nr_memcmp(s, &null_segment, sizeof(null_segment)));

  nr_txn_destroy(&txn);
}

static void test_span_queue(void) {
  test_txn_state_t* p = (test_txn_state_t*)tlib_getspecific();
  nrapp_t app = {
      .state = NR_APP_OK,
      .info = {.trace_observer_host = "trace-observer"},
      .limits = {.span_events = 1000},
  };
  uint64_t batch_count = 0;
  nrtxnopt_t opts = {
      .distributed_tracing_enabled = true,
      .span_events_enabled = true,
      .span_queue_batch_size = 0,
      .span_queue_batch_timeout = 1 * NR_TIME_DIVISOR,
  };
  nrtxn_t* txn;

  nrt_mutex_init(&app.app_lock, 0);
  p->txns_app = &app;

  /*
   * Test : Trace observer host with a zero batch size.
   */
  txn = nr_txn_begin(&app, &opts, NULL);
  tlib_pass_if_null(
      "an app with a trace observer and a zero batch size should not create a "
      "span queue",
      txn->span_queue);
  nr_txn_destroy(&txn);

  /*
   * Test : Trace observer host with a non-zero batch size.
   */
  opts.span_queue_batch_size = 1000;
  txn = nr_txn_begin(&app, &opts, NULL);

  tlib_pass_if_not_null(
      "an app with a trace observer and a non-zero batch size should create a "
      "span queue",
      txn->span_queue);

  // Replace the span queue with a mocked one we can use for testing.
  nr_span_queue_destroy(&txn->span_queue);
  txn->span_queue = nr_span_queue_create(opts.span_queue_batch_size,
                                         opts.span_queue_batch_timeout,
                                         null_batch_handler, &batch_count);

  nr_txn_end(txn);
  tlib_pass_if_uint64_t_equal(
      "a batch must be sent at the end of a transaction", 1, batch_count);

  tlib_pass_if_time_equal(
      "seen metric must be incremented", 1,
      nrm_count(nrm_find(txn->unscoped_metrics,
                         "Supportability/InfiniteTracing/Span/Seen")));

  nr_txn_destroy(&txn);
  nrt_mutex_destroy(&app.app_lock);
}

static void test_segment_record_error(void) {
  nrapp_t app = {
      .state = NR_APP_OK,
      .limits = {
        .analytics_events = NR_MAX_ANALYTIC_EVENTS,
        .span_events = NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED,
      },
  };
  nrtxnopt_t opts;
  nr_segment_t* segment;
  nrtxn_t* txn;

  /* Setup transaction and segment */
  nr_memset(&opts, 0, sizeof(opts));
  opts.distributed_tracing_enabled = 1;
  opts.span_events_enabled = 1;
  txn = nr_txn_begin(&app, &opts, NULL);
  segment = nr_segment_start(txn, NULL, NULL);
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  txn->options.allow_raw_exception_messages = 1;

  /* No error attributes added if error collection isn't enabled */
  txn->options.err_enabled = 0;
  nr_txn_record_error(txn, 1, true, "msg", "class", "[\"A\",\"B\"]");
  tlib_pass_if_null("No segment error created", segment->error);
  txn->options.err_enabled = 1;

  /* Do not add to current segment */
  nr_txn_record_error(txn, 0.5, false /* do not add to current segment*/,
                      "low priority message", "low priority class", "[\"A\",\"B\"]");
  tlib_pass_if_not_null("Txn error event created", txn->error);
  tlib_pass_if_null("Segment error NOT created", segment->error);
  tlib_pass_if_str_equal("Correct txn error.message", "low priority message",
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("Correct txn error.class", "low priority class",
                         nr_error_get_klass(txn->error));

  /* Normal operation: txn error prioritized over previous */
  nr_txn_record_error(txn, 1, true, "error message", "error class", "[\"A\",\"B\"]");

  tlib_pass_if_not_null("Txn error event created", txn->error);
  tlib_pass_if_not_null("Segment error created", segment->error);
  tlib_pass_if_str_equal("Correct segment error.message", "error message",
                         segment->error->error_message);
  tlib_pass_if_str_equal("Correct segment error.class", "error class",
                         segment->error->error_class);
  tlib_pass_if_str_equal("txn error message matches segment error message",
                         segment->error->error_message,
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("txn error class matches segment error class",
                         segment->error->error_class,
                         nr_error_get_klass(txn->error));

  /* Multiple errors on the same segment */
  nr_txn_record_error(txn, 1, true, "error message 2", "error class 2",
                      "[\"A\",\"B\"]");

  tlib_pass_if_str_equal("Segment error.message overwritten", "error message 2",
                         segment->error->error_message);
  tlib_pass_if_str_equal("Segment error.class overwritten", "error class 2",
                         segment->error->error_class);
  tlib_pass_if_str_equal("txn error message matches segment error message",
                         segment->error->error_message,
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("txn error message matches segment error message",
                         segment->error->error_message,
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("txn error class matches segment error class",
                         segment->error->error_class,
                         nr_error_get_klass(txn->error));

  /* High_security */
  txn->high_security = 1;
  nr_txn_record_error(txn, 1, true, "Highly secure message", "error class",
                      "[\"A\",\"B\"]");
  tlib_pass_if_not_null("Segment error created", segment->error);
  tlib_pass_if_str_equal("Secure error.message",
                         NR_TXN_HIGH_SECURITY_ERROR_MESSAGE,
                         segment->error->error_message);
  tlib_pass_if_str_equal("Correct segment error class", "error class",
                         segment->error->error_class);
  tlib_pass_if_str_equal("txn error message matches segment error message",
                         segment->error->error_message,
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("txn error class matches segment error class",
                         segment->error->error_class,
                         nr_error_get_klass(txn->error));
  txn->high_security = 0;

  /* allow_raw_exception_messages */
  txn->options.allow_raw_exception_messages = 0;
  nr_txn_record_error(txn, 1, true, "Another highly secure message",
                      "another error class", "[\"A\",\"B\"]");
  tlib_pass_if_not_null("Segment error created", segment->error);
  tlib_pass_if_str_equal("Secure error message",
                         NR_TXN_ALLOW_RAW_EXCEPTION_MESSAGE,
                         segment->error->error_message);
  tlib_pass_if_str_equal("Correct segment error.class", "another error class",
                         segment->error->error_class);
  tlib_pass_if_str_equal("txn error message matches segment error message",
                         segment->error->error_message,
                         nr_error_get_message(txn->error));
  tlib_pass_if_str_equal("txn error class matches segment error class",
                         segment->error->error_class,
                         nr_error_get_klass(txn->error));

  nr_txn_destroy(&txn);
}

static nrtxn_t* new_txn_for_record_log_event_test(char* entity_name) {
  nrapp_t app;
  nrtxnopt_t opts;
  nrtxn_t* txn;
  nr_segment_t* segment;

  /* Setup app state */
  nr_memset(&app, 0, sizeof(app));
  app.state = NR_APP_OK;
  app.entity_name = entity_name;
  app.limits = default_app_limits();

  /* Setup log feature options */
  nr_memset(&opts, 0, sizeof(opts));
  opts.logging_enabled = true;
  opts.log_forwarding_enabled = true;
  opts.log_forwarding_log_level = LOG_LEVEL_WARNING;
  opts.log_decorating_enabled = false;
  opts.log_events_max_samples_stored = 10;
  opts.log_metrics_enabled = true;

  opts.distributed_tracing_enabled = 1; /* for linking meta data */

  /* Start txn and segment */
  txn = nr_txn_begin(&app, &opts, NULL);
  segment = nr_segment_start(txn, txn->segment_root, NULL);
  nr_distributed_trace_set_sampled(txn->distributed_trace, true);
  nr_txn_set_current_segment(txn, segment);

  txn->options.span_events_enabled = 1; /* for linking meta data */

  return txn;
}

static void test_log_level_verify(void) {
  nrtxn_t* txn = NULL;
  txn = new_txn_for_record_log_event_test("test_log_level_verify");

  /* Test NULL values */
  tlib_pass_if_false("NULL txn ok",
                     nr_txn_log_forwarding_log_level_verify(NULL, LL_NOTI_STR),
                     "expected false");
  tlib_pass_if_true("NULL log level ok",
                    nr_txn_log_forwarding_log_level_verify(txn, NULL),
                    "expected true");

  /* Test known values */
  txn->options.log_forwarding_log_level = LOG_LEVEL_WARNING;
  tlib_pass_if_false("INFO not passed for log level = WARNING",
                     nr_txn_log_forwarding_log_level_verify(txn, LL_INFO_STR),
                     "expected false");
  tlib_pass_if_false("DEBUG not passed for log level = WARNING",
                     nr_txn_log_forwarding_log_level_verify(txn, LL_INFO_STR),
                     "expected false");
  txn->options.log_forwarding_log_level = LOG_LEVEL_WARNING;
  tlib_pass_if_true("ALERT  passed for log level = WARNING",
                    nr_txn_log_forwarding_log_level_verify(txn, LL_ALER_STR),
                    "expected true");
  txn->options.log_forwarding_log_level = LOG_LEVEL_WARNING;
  tlib_pass_if_true("EMERGENCY  passed for log level = WARNING",
                    nr_txn_log_forwarding_log_level_verify(txn, LL_EMER_STR),
                    "expected true");

  /* Test unknown level passed even if threshold set to EMERGENCY */
  txn->options.log_forwarding_log_level = LOG_LEVEL_EMERGENCY;
  tlib_pass_if_true("Unknown log level passed for log level = EMERGENCY",
                    nr_txn_log_forwarding_log_level_verify(txn, "APPLES"),
                    "expected true");

  nr_txn_destroy(&txn);
}

static void test_record_log_event(void) {
#define LOG_LEVEL LL_WARN_STR
#define LOG_MESSAGE "Sample log message"
#define LOG_TIMESTAMP 1234
#define LOG_EVENT_PARAMS \
  LOG_LEVEL, LOG_MESSAGE, LOG_TIMESTAMP* NR_TIME_DIVISOR_MS
#define APP_HOST_NAME "localhost"
#define APP_ENTITY_NAME "test_record_log_event"
#define APP_ENTITY_GUID "guid"

  nrapp_t appv = {.host_name = APP_HOST_NAME, .entity_guid = APP_ENTITY_GUID};
  nrtxn_t* txn = NULL;
  const char* expected = NULL;
  char* log_event_json = NULL;
  nr_vector_t* vector;
  void* test_e;
  bool pass;

  /*
   * NULL parameters: don't record, don't create metrics, don't blow up!
   */

  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  nr_txn_record_log_event(NULL, NULL, NULL, 0, NULL, NULL);
  tlib_pass_if_int_equal("all params null, no crash, event not recorded", 0,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("all params null, no crash, event not recorded", 0,
                         nr_log_events_number_saved(txn->log_events));
  nr_txn_destroy(&txn);

  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  nr_txn_record_log_event(NULL, LOG_EVENT_PARAMS, NULL, NULL);
  tlib_pass_if_int_equal("null txn, no crash, event not recorded", 0,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("null txn, no crash, event not recorded", 0,
                         nr_log_events_number_saved(txn->log_events));
  nr_txn_destroy(&txn);

  /*
   * Mixed conditions (some NULL parameters): maybe record, create metrics,
   * don't blow up!
   */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  nr_txn_record_log_event(txn, NULL, NULL, 0, NULL, NULL);
  tlib_pass_if_int_equal("null log params, event not recorded", 0,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("null log params, event not recorded", 0,
                         nr_log_events_number_saved(txn->log_events));
  test_txn_metric_is("null log level, event not recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED, "Logging/lines", 1, 0,
                     0, 0, 0, 0);
  test_txn_metric_is("null log level, event recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED, "Logging/lines/UNKNOWN",
                     1, 0, 0, 0, 0, 0);
  nr_txn_destroy(&txn);

  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  nr_txn_record_log_event(txn, NULL, LOG_MESSAGE, 0, NULL, NULL);
  tlib_pass_if_int_equal("null log level, event seen", 1,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("null log level, event saved", 1,
                         nr_log_events_number_saved(txn->log_events));

  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(txn->log_events, vector);
  pass = nr_vector_get_element(vector, 0, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  log_event_json = nr_log_event_to_json((nr_log_event_t*)test_e);
  tlib_pass_if_not_null("null log level, event recorded", log_event_json);
  expected
      = "{"
        "\"message\":\"" LOG_MESSAGE
        "\","
        "\"level\":\"" LL_UNKN_STR
        "\","
        "\"trace.id\":\"00000000000000000000000000000000\","
        "\"span.id\":\"0000000000000000\","
        "\"entity.name\":\"" APP_ENTITY_NAME
        "\","
        "\"timestamp\":0"
        "}";
  tlib_pass_if_str_equal("null log level, event recorded, json ok", expected,
                         log_event_json);
  test_txn_metric_is("null log level, event recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED, "Logging/lines", 1, 0,
                     0, 0, 0, 0);
  test_txn_metric_is("null log level, event recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED, "Logging/lines/UNKNOWN",
                     1, 0, 0, 0, 0, 0);
  nr_free(log_event_json);
  nr_vector_destroy(&vector);
  nr_txn_destroy(&txn);

  /* Happy path - everything initialized: record! */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  tlib_pass_if_int_equal("happy path, event seen", 1,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("happy path, event saved", 1,
                         nr_log_events_number_saved(txn->log_events));

  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(txn->log_events, vector);
  pass = nr_vector_get_element(vector, 0, &test_e);
  tlib_pass_if_true("retrived log element from vector OK", pass,
                    "expected TRUE");
  log_event_json = nr_log_event_to_json((nr_log_event_t*)test_e);
  tlib_fail_if_null("no json", log_event_json);
  tlib_pass_if_not_null("happy path, event recorded", log_event_json);
  expected
      = "{"
        "\"message\":\"" LOG_MESSAGE
        "\","
        "\"level\":\"" LOG_LEVEL
        "\","
        "\"trace.id\":\"00000000000000000000000000000000\","
        "\"span.id\":\"0000000000000000\","
        "\"entity.guid\":\"" APP_ENTITY_GUID
        "\","
        "\"entity.name\":\"" APP_ENTITY_NAME
        "\","
        "\"hostname\":\"" APP_HOST_NAME
        "\","
        "\"timestamp\":" NR_STR2(LOG_TIMESTAMP) "}";
  tlib_pass_if_str_equal("happy path, event recorded, json ok", expected,
                         log_event_json);
  nr_free(log_event_json);
  nr_vector_destroy(&vector);

  test_txn_metric_is("happy path, event recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED, "Logging/lines", 1, 0,
                     0, 0, 0, 0);
  test_txn_metric_is("happy path, event recorded, metric created",
                     txn->unscoped_metrics, MET_FORCED,
                     "Logging/lines/" LOG_LEVEL, 1, 0, 0, 0, 0, 0);
  tlib_pass_if_null(
      "Logging/Forwarding/Dropped not created",
      nrm_find(txn->unscoped_metrics, "Logging/Forwarding/Dropped"));
  nr_txn_destroy(&txn);

  /* Happy path with sampling */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  /* fill up events pool to force sampling */
  for (int i = 0, max_events = nr_log_events_max_events(txn->log_events);
       i < max_events; i++) {
    nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  }
  /* force sampling */
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  test_txn_metric_is("happy path with sampling, events recorded and dropped",
                     txn->unscoped_metrics, MET_FORCED,
                     "Logging/Forwarding/Dropped", 2, 0, 0, 0, 0, 0);
  nr_txn_destroy(&txn);

  /* Happy path with log events pool size of 0 */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  /* Force pool size of 0 */
  nr_log_events_destroy(&txn->log_events);
  txn->log_events = nr_log_events_create(0);
  tlib_pass_if_not_null("empty log events pool created", txn->log_events);
  tlib_pass_if_int_equal("empty log events pool stores 0 events", 0,
                         nr_log_events_max_events(txn->log_events));
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  /* Events are seen because log forwarding is enabled
     and txn->options.log_events_max_samples_stored > 0 */
  tlib_pass_if_int_equal("happy path, event seen", 2,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("happy path, event saved", 0,
                         nr_log_events_number_saved(txn->log_events));
  test_txn_metric_is("happy path with sampling, events recorded and dropped",
                     txn->unscoped_metrics, MET_FORCED,
                     "Logging/Forwarding/Dropped", 2, 0, 0, 0, 0, 0);
  nr_txn_destroy(&txn);

  /* High_security */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);
  txn->high_security = 1;
  nr_txn_record_log_event(txn, LOG_EVENT_PARAMS, NULL, &appv);
  tlib_pass_if_int_equal("happy path, hsm, event seen", 0,
                         nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal("happy path, hsm, event saved", 0,
                         nr_log_events_number_saved(txn->log_events));

  vector = nr_vector_create(10, NULL, NULL);
  nr_log_events_to_vector(txn->log_events, vector);
  tlib_pass_if_int_equal("happy path, hsm, 0 len vector", 0,
                         nr_vector_size(vector));
  nr_vector_destroy(&vector);
  nr_txn_destroy(&txn);

  /* Happy path but log level causes some message to be ignored */
  txn = new_txn_for_record_log_event_test(APP_ENTITY_NAME);

  /* default filter log level is LOG_LEVEL_WARNING */
  /* these messages should be accepted */
  nr_txn_record_log_event(txn, LL_ALER_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_CRIT_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_WARN_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_EMER_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_UNKN_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, "APPLES", LOG_MESSAGE, 0, NULL, NULL);

  /* these messages will be dropped */
  nr_txn_record_log_event(txn, LL_INFO_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_DEBU_STR, LOG_MESSAGE, 0, NULL, NULL);
  nr_txn_record_log_event(txn, LL_NOTI_STR, LOG_MESSAGE, 0, NULL, NULL);

  /* events seen and saved are both 6 because the filtering occurs before
   * log forwarding handles the messages.
   */
  tlib_pass_if_int_equal(
      "happy path with WARNING log level threshold, events seen", 6,
      nr_log_events_number_seen(txn->log_events));
  tlib_pass_if_int_equal(
      "happy path with WARNING log level threshold, events saved", 6,
      nr_log_events_number_saved(txn->log_events));

  /* should see 9 messages in Logging/lines metric */
  test_txn_metric_is(
      "happy path with WARNING log level threshold, events total",
      txn->unscoped_metrics, MET_FORCED, "Logging/lines", 9, 0, 0, 0, 0, 0);

  /* Dropped metric should show all the 3 messages were not considered */
  test_txn_metric_is(
      "happy path with WARNING log level threshold, events dropped",
      txn->unscoped_metrics, MET_FORCED, "Logging/Forwarding/Dropped", 3, 0, 0,
      0, 0, 0);
  nr_txn_destroy(&txn);
}

static void test_txn_log_configuration(void) {
  // clang-format off
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  /* log features globally disabled, high security disabled */
  txn->options.logging_enabled = false;
  txn->high_security = false;

  txn->options.log_decorating_enabled = false;
  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = false;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=0, forwarding=0, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=0, high_security=0, metrics=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=0, decorating=0 -> always off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=0, forwarding=1, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=0, high_security=0, metrics=1 -> off");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=0, forwarding=0, samples=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=0, forwarding=1, samples=1 -> off");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=0, decorating=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=0, forwarding=1, samples=1, decorating=1 -> forwarding off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=0, forwarding=1, samples=1, decorating=1 -> decorating off");

  /* log features globally enabled, high security disabled */
  txn->options.logging_enabled = true;
  txn->high_security = false;

  txn->options.log_decorating_enabled = false;
  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = false;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=0, forwarding=0, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=1, high_security=0, metrics=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=0, decorating=0 -> always off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = true;
  tlib_pass_if_true(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=0, forwarding=1, samples=0 -> on");
  tlib_pass_if_true(__func__, nr_txn_log_metrics_enabled(txn),    "global=1, high_security=0, metrics=1 -> on");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=0, forwarding=0, samples=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_true(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=0, forwarding=1, samples=1 -> on");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_true(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=0, decorating=1 -> on");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_true(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=0, forwarding=1, samples=1, decorating=1 -> forwarding on");
  tlib_pass_if_true(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=0, forwarding=1, samples=1, decorating=1 -> decorating on");

  /* log features globally disabled, high security enabled */
  txn->options.logging_enabled = false;
  txn->options.log_decorating_enabled = false;
  txn->high_security = true;

  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = false;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=1, forwarding=0, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=0, high_security=1, metrics=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=1, decorating=0 -> always off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=1, forwarding=1, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=0, high_security=1, metrics=1 -> off");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=1, forwarding=0, samples=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=1, forwarding=1, samples=1 -> off");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=1, decorating=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=0, high_security=1, forwarding=1, samples=1, decorating=1 -> forwarding off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=0, high_security=1, forwarding=1, samples=1, decorating=1 -> decorating off");

  /* log features globally enabled, high security enabled */
  txn->options.logging_enabled = true;
  txn->high_security = true;

  txn->options.log_decorating_enabled = false;
  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = false;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=1, forwarding=0, samples=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_metrics_enabled(txn),    "global=1, high_security=1, metrics=0 -> off");
  tlib_pass_if_false(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=1, decorating=0 -> always off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 0;
  txn->options.log_metrics_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=1, forwarding=1, samples=0 -> off");
  tlib_pass_if_true(__func__, nr_txn_log_metrics_enabled(txn),    "global=1, high_security=1, metrics=1 -> on");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=1, forwarding=0, samples=1 -> off");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_events_max_samples_stored = 1;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=1, forwarding=1, samples=1 -> off");

  txn->options.log_forwarding_enabled = false;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_true(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=1, decorating=1 -> on");

  txn->options.log_forwarding_enabled = true;
  txn->options.log_decorating_enabled = true;
  tlib_pass_if_false(__func__, nr_txn_log_forwarding_enabled(txn), "global=1, high_security=1, forwarding=1, samples=1, decorating=1 -> forwarding off");
  tlib_pass_if_true(__func__, nr_txn_log_decorating_enabled(txn), "global=1, high_security=1, forwarding=1, samples=1, decorating=1 -> decorating on");
  // clang-format on
}

static void test_nr_txn_add_php_package(void) {
  char* json;
  char* package_name1 = "Laravel";
  char* package_version1 = "8.83.27";
  char* package_name2 = "Slim";
  char* package_version2 = "4.12.0";
  char* package_name3 = "Drupal";
  char* package_version3 = NULL;
  char* package_name4 = "Wordpress";
  char* package_version4 = PHP_PACKAGE_VERSION_UNKNOWN;
  nrtxn_t* txn = new_txn(0);

  /*
   * NULL parameters: ensure it does not crash
   */
  nr_txn_add_php_package(NULL, NULL, NULL);
  nr_txn_add_php_package(NULL, package_name1, package_version1);
  nr_txn_add_php_package(txn, NULL, package_version1);
  nr_txn_add_php_package(txn, package_name1, NULL);

  // Test: add php packages to transaction
  nr_txn_add_php_package(txn, package_name1, package_version1);
  nr_txn_add_php_package(txn, package_name2, package_version2);
  nr_txn_add_php_package(txn, package_name3, package_version3);
  nr_txn_add_php_package(txn, package_name4, package_version4);
  json = nr_php_packages_to_json(txn->php_packages);

  tlib_pass_if_str_equal("correct json",
                         "[[\"Laravel\",\"8.83.27\",{}],"
                         "[\"Drupal\",\" \",{}],[\"Wordpress\",\" \",{}],"
                         "[\"Slim\",\"4.12.0\",{}]]",
                         json);

  nr_free(json);
  nr_txn_destroy(&txn);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = 2, .state_size = sizeof(test_txn_state_t)};

void test_main(void* p NRUNUSED) {
  test_txn_cmp_options();
  test_freeze_name_update_apdex();
  test_create_apdex_metrics();
  test_create_error_metrics();
  test_create_duration_metrics();
  test_create_queue_metric();
  test_set_path();
  test_set_request_uri();
  test_record_error_worthy();
  test_record_error();
  test_begin_bad_params();
  test_begin();
  test_end();
  test_should_force_persist();
  test_set_as_background_job();
  test_set_as_web_transaction();
  test_set_http_status();
  test_add_user_custom_parameter();
  test_add_request_parameter();
  test_set_request_referer();
  test_set_request_content_length();
  test_add_error_attributes();
  test_duration();
  test_duration_with_segment_retiming();
  test_duration_with_txn_retiming();
  test_queue_time();
  test_set_queue_start();
  test_create_rollup_metrics();
  test_record_custom_event();
  test_is_account_trusted();
  test_should_save_trace();
  test_event_should_add_guid();
  test_unfinished_duration();
  test_should_create_apdex_metrics();
  test_add_cat_analytics_intrinsics();
  test_add_cat_intrinsics();
  test_alternate_path_hashes();
  test_apdex_zone();
  test_get_cat_trip_id();
  test_get_guid();
  test_get_path_hash();
  test_is_synthetics();
  test_start_time();
  test_start_time_secs();
  test_rel_to_abs();
  test_abs_to_rel();
  test_now_rel();
  test_namer();
  test_error_to_event();
  test_create_event();
  test_create_event_with_retimed_segments();
  test_name_from_function();
  test_txn_ignore();
  test_add_custom_metric();
  test_txn_cat_map_cross_agent_tests();
  test_txn_dt_cross_agent_tests();
  test_txn_trace_context_cross_agent_tests();
  test_force_single_count();
  test_fn_supportability_metric();
  test_txn_set_attribute();
  test_sql_recording_level();
  test_sql_recording_level_lasp();
  test_custom_events_lasp();
  test_custom_parameters_lasp();
  test_custom_parameters_segment();
  test_allow_raw_messages_lasp();
  test_nr_txn_is_current_path_named();
  test_create_distributed_trace_payload();
  test_accept_before_create_distributed_tracing();
  test_nr_txn_add_distributed_tracing_intrinsics();
  test_txn_accept_distributed_trace_payload_metrics();
  test_txn_accept_distributed_trace_payload();
  test_txn_accept_distributed_trace_payload_httpsafe();
  test_txn_accept_distributed_trace_payload_optionals();
  test_default_trace_id();
  test_root_segment_priority();
  test_should_create_span_events();
  test_parent_stacks();
  test_force_current_segment();
  test_txn_is_sampled();
  test_get_current_trace_id();
  test_get_current_span_id();
  test_finalize_parent_stacks();
  test_max_segments_no_limit();
  test_max_segments_count_tree();
  test_max_segments();
  test_allocated_segment_count();
  test_allocate_segment();
  test_create_w3c_traceparent_header();
  test_create_w3c_tracestate_header();
  test_txn_accept_distributed_trace_payload_w3c();
  test_txn_accept_distributed_trace_payload_w3c_and_nr();
  test_span_queue();
  test_segment_record_error();
  test_log_level_verify();
  test_record_log_event();
  test_txn_log_configuration();
  test_nr_txn_add_php_package();
}
