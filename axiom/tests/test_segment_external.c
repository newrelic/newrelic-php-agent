/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "test_segment_helpers.h"

static nr_segment_t* mock_txn_segment(void) {
  nrtxn_t* txn = new_txn(0);

  return nr_segment_start(txn, NULL, NULL);
}

#define TEST_EXTERNAL_SEGMENT(M_segment, M_name, M_uri, M_library,            \
                              M_procedure, M_guid)                            \
  test_segment_metric_created("rollup segment metric exists",                 \
                              (M_segment)->metrics, (M_name), true);          \
  tlib_pass_if_str_equal(                                                     \
      "segment name", (M_name),                                               \
      nr_string_get((M_segment)->txn->trace_strings, (M_segment)->name));     \
  tlib_pass_if_true("segment type", NR_SEGMENT_EXTERNAL == (M_segment)->type, \
                    "NR_SEGMENT_EXTERNAL");                                   \
  tlib_pass_if_str_equal(                                                     \
      "segment uri", (M_segment)->typed_attributes->external.uri, (M_uri));   \
  tlib_pass_if_str_equal("segment library",                                   \
                         (M_segment)->typed_attributes->external.library,     \
                         (M_library));                                        \
  tlib_pass_if_str_equal("segment procedure",                                 \
                         (M_segment)->typed_attributes->external.procedure,   \
                         (M_procedure));                                      \
  tlib_pass_if_str_equal(                                                     \
      "transaction guid",                                                     \
      (M_segment)->typed_attributes->external.transaction_guid, (M_guid));

void nr_header_outbound_response(nrtxn_t* txn,
                                 const char* encoded_response,
                                 char** external_id_ptr,
                                 char** external_txnname_ptr,
                                 char** external_guid_ptr) {
  nrobj_t* obj;
  const char* external_id;
  const char* external_txnname;
  const char* external_guid;

  tlib_pass_if_not_null("txn present", txn);
  tlib_pass_if_not_null("encoded_response present", encoded_response);
  tlib_pass_if_not_null("external_id_ptr present", external_id_ptr);
  tlib_pass_if_not_null("external_txnname_ptr present", external_txnname_ptr);
  tlib_pass_if_not_null("external_guid_ptr present", external_guid_ptr);

  obj = nro_create_from_json(encoded_response);
  external_id = nro_get_hash_string(obj, "id", 0);
  external_txnname = nro_get_hash_string(obj, "txnname", 0);
  external_guid = nro_get_hash_string(obj, "guid", 0);

  if (external_id && external_id_ptr) {
    *external_id_ptr = nr_strdup(external_id);
  }
  if (external_txnname && external_txnname_ptr) {
    *external_txnname_ptr = nr_strdup(external_txnname);
  }
  if (external_guid && external_guid_ptr) {
    *external_guid_ptr = nr_strdup(external_guid);
  }

  nro_delete(obj);
}

static void test_bad_parameters(void) {
  nr_segment_t seg_null = {0};
  nr_segment_t* seg_null_ptr;
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;
  nr_segment_external_params_t params;

  tlib_pass_if_false("bad parameters", nr_segment_external_end(NULL, &params),
                     "expected false");

  seg_null_ptr = NULL;
  tlib_pass_if_false("bad parameters",
                     nr_segment_external_end(&seg_null_ptr, &params),
                     "expected false");

  seg_null_ptr = &seg_null;
  tlib_pass_if_false("bad parameters",
                     nr_segment_external_end(&seg_null_ptr, &params),
                     "expected false");

  tlib_pass_if_false("bad parameters", nr_segment_external_end(&seg, NULL),
                     "expected false");
  test_metric_vector_size(seg->metrics, 0);

  nr_txn_destroy(&txn);
}

static void test_web_transaction(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){.uri = "newrelic.com"});

  TEST_EXTERNAL_SEGMENT(seg, "External/newrelic.com/all", "newrelic.com", NULL,
                        NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created("web transaction creates a segment metric",
                              seg->metrics, "External/newrelic.com/all", true);
  test_txn_metric_created("web transaction creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_null_url(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){.uri = NULL});

  TEST_EXTERNAL_SEGMENT(seg, "External/<unknown>/all", NULL, NULL, NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created("NULL url creates a segment metric", seg->metrics,
                              "External/<unknown>/all", true);
  test_txn_metric_created("NULL url creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_empty_url(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){.uri = ""});

  TEST_EXTERNAL_SEGMENT(seg, "External/<unknown>/all", NULL, NULL, NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created("empty URL creates a segment metric",
                              seg->metrics, "External/<unknown>/all", true);
  test_txn_metric_created("empty URL creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_domain_parsing_fails(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){.uri = "@@@@@"});

  TEST_EXTERNAL_SEGMENT(seg, "External/<unknown>/all", "", NULL, NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created("failed domain parsing creates a segment metric",
                              seg->metrics, "External/<unknown>/all", true);
  test_txn_metric_created("failed domain parsing creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_url_saving_strips_parameters(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){
                .uri = "http://newrelic.com?secret=hhhhhhh"});

  TEST_EXTERNAL_SEGMENT(seg, "External/newrelic.com/all", "http://newrelic.com",
                        NULL, NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created("a stripped URL creates a segment metric",
                              seg->metrics, "External/newrelic.com/all", true);
  test_txn_metric_created("a stripped URL creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_only_external_id(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){
                .uri = "newrelic.com",
                .encoded_response_header = "{\"id\":\"12345#6789\"}"});

  TEST_EXTERNAL_SEGMENT(seg, "External/newrelic.com/all", "newrelic.com", NULL,
                        NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created(
      "only having an external ID creates a segment metric", seg->metrics,
      "External/newrelic.com/all", true);
  test_txn_metric_created("only having an external ID creates a rollup metric",
                          txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_only_external_txnname(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){
                .uri = "newrelic.com",
                .encoded_response_header = "{\"txnname\":\"my_txn\"}"});

  TEST_EXTERNAL_SEGMENT(seg, "External/newrelic.com/all", "newrelic.com", NULL,
                        NULL, NULL);
  test_metric_vector_size(seg->metrics, 1);
  test_segment_metric_created(
      "only having an external transaction name creates a segment metric",
      seg->metrics, "External/newrelic.com/all", true);
  test_txn_metric_created(
      "only having an external transaction name creates a rollup metric",
      txn->unscoped_metrics, "External/all");

  nr_txn_destroy(&txn);
}

static void test_external_id_and_txnname(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg, &(nr_segment_external_params_t){
                .uri = "newrelic.com",
                .encoded_response_header
                = "{\"id\":\"12345#6789\",\"txnname\":\"my_txn\"}"});

  TEST_EXTERNAL_SEGMENT(seg,
                        "ExternalTransaction/newrelic.com/12345#6789/my_txn",
                        "newrelic.com", NULL, NULL, NULL);
  test_metric_vector_size(seg->metrics, 3);
  test_txn_metric_created(
      "having both an external ID and transaction name creates a rollup metric",
      txn->unscoped_metrics, "External/all");
  test_segment_metric_created(
      "having both an external ID and transaction name creates a segment "
      "metric in the External namespace",
      seg->metrics, "External/newrelic.com/all", false);
  test_segment_metric_created(
      "having both an external ID and transaction name creates a segment "
      "metric in the ExternalApp namespace",
      seg->metrics, "ExternalApp/newrelic.com/12345#6789/all", false);
  test_segment_metric_created(
      "having both an external ID and transaction name creates a segment "
      "metric in the ExternalTransaction namespace",
      seg->metrics, "ExternalTransaction/newrelic.com/12345#6789/my_txn", true);

  nr_txn_destroy(&txn);
}

static void test_external_id_and_txnname_and_guid(void) {
  nr_segment_t* seg = mock_txn_segment();
  nrtxn_t* txn = seg->txn;

  test_segment_external_end_and_keep(
      &seg,
      &(nr_segment_external_params_t){
          .uri = "newrelic.com",
          .encoded_response_header = "{\"id\":\"12345#6789\",\"txnname\":\"my_"
                                     "txn\",\"guid\":\"0123456789ABCDEF\"}"});

  TEST_EXTERNAL_SEGMENT(seg,
                        "ExternalTransaction/newrelic.com/12345#6789/my_txn",
                        "newrelic.com", NULL, NULL, "0123456789ABCDEF");
  test_metric_vector_size(seg->metrics, 3);
  test_txn_metric_created(
      "having an external ID, transaction name, and GUID creates a rollup "
      "metric",
      txn->unscoped_metrics, "External/all");
  test_segment_metric_created(
      "having an external ID, transaction name, and GUID creates a segment "
      "metric in the External namespace",
      seg->metrics, "External/newrelic.com/all", false);
  test_segment_metric_created(
      "having an external ID, transaction name, and GUID creates a segment "
      "metric in the ExternalApp namespace",
      seg->metrics, "ExternalApp/newrelic.com/12345#6789/all", false);
  test_segment_metric_created(
      "having an external ID, transaction name, and GUID creates a segment "
      "metric in the ExternalTransaction namespace",
      seg->metrics, "ExternalTransaction/newrelic.com/12345#6789/my_txn", true);

  nr_txn_destroy(&txn);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_web_transaction();
  test_null_url();
  test_empty_url();
  test_domain_parsing_fails();
  test_url_saving_strips_parameters();
  test_only_external_id();
  test_only_external_txnname();
  test_external_id_and_txnname();
  test_external_id_and_txnname_and_guid();
}
