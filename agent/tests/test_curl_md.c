/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "nr_header.h"
#include "php_agent.h"
#include "php_curl.h"
#include "php_curl_md.h"
#include "php_hash.h"
#include "util_time.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_curl_metadata_get(TSRMLS_D) {
  zval* ch;

  tlib_php_request_start();

  tlib_pass_if_null("nr_php_curl_md_get is null safe",
                    nr_php_curl_md_get(NULL TSRMLS_CC));

  ch = nr_php_call(NULL, "curl_init");
  tlib_pass_if_not_null("metadata is created upon call to curl init",
                        nr_php_curl_md_get(ch TSRMLS_CC));

  nr_php_zval_free(&ch);
  tlib_php_request_end();
}

static void test_curl_metadata_segment(TSRMLS_D) {
  const nr_php_curl_md_t* metadata;
  zval* ch;
  nr_segment_t* segment;
  nr_segment_t* segment_2;

  tlib_php_request_start();

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  segment_2 = nr_segment_start(NRPRG(txn), NULL, NULL);

  tlib_pass_if_false("nr_php_curl_md_set_segment is null safe (handle)",
                     nr_php_curl_md_set_segment(NULL, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_ptr_equal("nr_php_curl_md_get_segment is null safe",
                         nr_php_curl_md_get_segment(NULL TSRMLS_CC), NULL);

  ch = nr_php_call(NULL, "curl_init");
  metadata = nr_php_curl_md_get(ch TSRMLS_CC);

  tlib_pass_if_false("nr_php_curl_md_set_segment is null safe (segment)",
                     nr_php_curl_md_set_segment(ch, NULL TSRMLS_CC),
                     "expected false");

  nr_php_curl_md_set_segment(ch, segment TSRMLS_CC);
  tlib_pass_if_ptr_equal("nr_php_curl_md_set_segment sets segment",
                         metadata->segment, segment);

  nr_php_curl_md_set_segment(ch, segment_2 TSRMLS_CC);
  tlib_pass_if_ptr_equal("subsequent nr_php_curl_md_set_segment sets segment",
                         metadata->segment, segment_2);

  tlib_pass_if_ptr_equal("nr_php_curl_md_get_segment gets segment",
                         metadata->segment,
                         nr_php_curl_md_get_segment(ch TSRMLS_CC));

  /* Simulate a transaction end/restart */
  NRTXN(abs_start_time) = 200;
  tlib_pass_if_null(
      "nr_php_curl_md_get_segment returns NULL when txn has changed",
      nr_php_curl_md_get_segment(ch TSRMLS_CC));

  nr_php_zval_free(&ch);
  tlib_php_request_end();
}

static void test_curl_metadata_method(TSRMLS_D) {
  const nr_php_curl_md_t* metadata;
  zval* ch;

  tlib_php_request_start();

  tlib_pass_if_false("nr_php_curl_md_set_method is null safe",
                     nr_php_curl_md_set_method(NULL, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_str_equal("nr_php_curl_md_get_method is null safe",
                         nr_php_curl_md_get_method(NULL TSRMLS_CC), "GET");

  ch = nr_php_call(NULL, "curl_init");
  metadata = nr_php_curl_md_get(ch TSRMLS_CC);

  tlib_pass_if_str_equal(
      "nr_php_curl_md_get_method returns \"GET\" if method is null", "GET",
      nr_php_curl_md_get_method(ch TSRMLS_CC));

  nr_php_curl_md_set_method(ch, "FOO" TSRMLS_CC);
  tlib_pass_if_str_equal("nr_php_curl_md_set_method sets method",
                         metadata->method, "FOO");

  nr_php_curl_md_set_method(ch, "BAR" TSRMLS_CC);
  tlib_pass_if_str_equal("subsequent nr_php_curl_md_set_method sets method",
                         metadata->method, "BAR");

  tlib_pass_if_str_equal("nr_php_curl_md_get_method gets method",
                         metadata->method,
                         nr_php_curl_md_get_method(ch TSRMLS_CC));

  nr_php_zval_free(&ch);
  tlib_php_request_end();
}

static void test_curl_metadata_response_header(TSRMLS_D) {
  const nr_php_curl_md_t* metadata;
  zval* ch;

  const char* header_text
      = "200 OK\nContent-Encoding: lil-string\n"
        "X-NewRelic-App-Data: test-header\nSet-Cookie: chocolate-chip=true";

  tlib_php_request_start();

  tlib_pass_if_false("nr_php_curl_md_set_response_header curl arg is null safe",
                     nr_php_curl_md_set_response_header(NULL, NULL TSRMLS_CC),
                     "expected false");

  ch = nr_php_call(NULL, "curl_init");
  tlib_pass_if_true(
      "nr_php_curl_md_set_response_header string arg is null safe",
      nr_php_curl_md_set_response_header(ch, NULL TSRMLS_CC), "expected true");
  nr_php_zval_free(&ch);

  tlib_pass_if_null("nr_php_curl_md_get_response_header is null safe",
                    nr_php_curl_md_get_response_header(NULL TSRMLS_CC));

  ch = nr_php_call(NULL, "curl_init");
  metadata = nr_php_curl_md_get(ch TSRMLS_CC);

  nr_php_curl_md_set_response_header(ch, header_text TSRMLS_CC);

  tlib_pass_if_str_equal("response header is set", metadata->response_header,
                         header_text);

  tlib_pass_if_str_equal(
      "nr_php_curl_md_get_response_header matches "
      "metadata->response_header",
      nr_php_curl_md_get_response_header(ch TSRMLS_CC), header_text);

  nr_php_zval_free(&ch);
  tlib_php_request_end();
}

static void test_curl_metadata_outbound_headers(TSRMLS_D) {
  const nr_php_curl_md_t* metadata;
  zval* ch;
  zval* headers;
  zval* str_header;
  char* test_kv = nr_header_format_name_value("test-key", "test-val", 0);

  tlib_php_request_start();

  tlib_pass_if_false("nr_php_curl_md_set_outbound_headers is null safe",
                     nr_php_curl_md_set_outbound_headers(NULL, NULL TSRMLS_CC),
                     "expected false");

  ch = nr_php_call(NULL, "curl_init");
  tlib_pass_if_false("nr_php_curl_md_set_outbound_headers is null safe",
                     nr_php_curl_md_set_outbound_headers(ch, NULL TSRMLS_CC),
                     "expected false");
  nr_php_zval_free(&ch);

  headers = nr_php_zval_alloc();
  array_init(headers);
  nr_php_add_next_index_string(headers, test_kv);

  ch = nr_php_call(NULL, "curl_init");
  metadata = nr_php_curl_md_get(ch TSRMLS_CC);

  tlib_pass_if_true("able to set simple outbound header",
                    nr_php_curl_md_set_outbound_headers(ch, headers TSRMLS_CC),
                    "expected true");

  tlib_pass_if_zval_identical("metadata->outbound_headers match passed value",
                              metadata->outbound_headers, headers);

  str_header = nr_php_zval_alloc();
  nr_php_zval_str(str_header, "I am most certainly not an array");
  tlib_pass_if_false(
      "outbound header must be zval array",
      nr_php_curl_md_set_outbound_headers(ch, str_header TSRMLS_CC),
      "expected false");

  nr_php_zval_free(&ch);
  nr_php_zval_free(&headers);
  nr_php_zval_free(&str_header);
  nr_free(test_kv);
  tlib_php_request_end();
}

static void test_curl_multi_metadata_get(TSRMLS_D) {
  tlib_php_request_start();

  zval* mh = nr_php_call(NULL, "curl_multi_init");
  const nr_php_curl_multi_md_t* multi_metadata
      = nr_php_curl_multi_md_get(mh TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("nr_php_curl_multi_md_get is null safe",
                    nr_php_curl_multi_md_get(NULL TSRMLS_CC));

  /*
   * Test : Multi metadata is created.
   */
  tlib_pass_if_not_null("metadata is created when needed", multi_metadata);
  tlib_pass_if_size_t_equal("curl multi metadata vector created", 8,
                            nr_vector_capacity(&multi_metadata->curl_handles));

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_multi_md_add(TSRMLS_D) {
  tlib_php_request_start();

  zval* ch1 = nr_php_call(NULL, "curl_init");
  zval* ch2 = nr_php_call(NULL, "curl_init");
  zval* ch3 = nr_php_call(NULL, "curl_init");
  zval* mh = nr_php_call(NULL, "curl_multi_init");
  nr_vector_t* handles;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Test null curl and curl multi handles",
                     nr_php_curl_multi_md_add(NULL, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_false("Test null curl handle",
                     nr_php_curl_multi_md_add(mh, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_false("Test null curl multi handle",
                     nr_php_curl_multi_md_add(NULL, ch1 TSRMLS_CC),
                     "expected false");

  /*
   * Test : curl multi add handle.
   */
  tlib_pass_if_true("Test adding first curl handle",
                    nr_php_curl_multi_md_add(mh, ch1 TSRMLS_CC),
                    "expected true");
  tlib_pass_if_true("Test adding second curl handle",
                    nr_php_curl_multi_md_add(mh, ch2 TSRMLS_CC),
                    "expected true");
  tlib_pass_if_true("Test adding third curl handle",
                    nr_php_curl_multi_md_add(mh, ch3 TSRMLS_CC),
                    "expected true");

  handles = nr_php_curl_multi_md_get_handles(mh TSRMLS_CC);
  tlib_pass_if_size_t_equal("curl_md vector has 3 curl handles", 3,
                            nr_vector_size(handles));

  /*
   * Test : add handle that is already in metadata
   */
  tlib_pass_if_false("Test adding first curl handle",
                     nr_php_curl_multi_md_add(mh, ch2 TSRMLS_CC),
                     "expected false");
  tlib_pass_if_size_t_equal("curl_md vector size didn't change", 3,
                            nr_vector_size(handles));

  nr_php_zval_free(&ch1);
  nr_php_zval_free(&ch2);
  nr_php_zval_free(&ch3);
  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_multi_md_remove(TSRMLS_D) {
  tlib_php_request_start();

  zval* ch1 = nr_php_call(NULL, "curl_init");
  zval* ch2 = nr_php_call(NULL, "curl_init");
  zval* ch3 = nr_php_call(NULL, "curl_init");
  zval* mh = nr_php_call(NULL, "curl_multi_init");
  nr_vector_t* handles;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_false("Test null curl and curl multi handles",
                     nr_php_curl_multi_md_add(NULL, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_false("Test null curl handle",
                     nr_php_curl_multi_md_add(mh, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_false("Test null curl multi handle",
                     nr_php_curl_multi_md_add(NULL, ch1 TSRMLS_CC),
                     "expected false");

  /*
   * Test : curl multi remove handle.
   */
  tlib_pass_if_true("Check first curl handle is added",
                    nr_php_curl_multi_md_add(mh, ch1 TSRMLS_CC),
                    "expected true");
  tlib_pass_if_true("Check second curl handle is added",
                    nr_php_curl_multi_md_add(mh, ch2 TSRMLS_CC),
                    "expected true");
  tlib_pass_if_true("Check third curl handle is added",
                    nr_php_curl_multi_md_add(mh, ch3 TSRMLS_CC),
                    "expected true");

  handles = nr_php_curl_multi_md_get_handles(mh TSRMLS_CC);
  tlib_pass_if_size_t_equal("curl_md vector has 3 curl handles", 3,
                            nr_vector_size(handles));

  tlib_pass_if_true("Test null curl multi remove handle",
                    nr_php_curl_multi_md_remove(mh, ch1 TSRMLS_CC),
                    "expected true");

  tlib_pass_if_size_t_equal("curl_md vector has 2 curl handles", 2,
                            nr_vector_size(handles));

  nr_php_zval_free(&ch1);
  nr_php_zval_free(&ch2);
  nr_php_zval_free(&ch3);
  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_multi_md_segment(TSRMLS_D) {
  const nr_php_curl_multi_md_t* metadata;
  zval* mh;
  nr_segment_t* segment;
  nr_segment_t* segment_2;

  tlib_php_request_start();

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  segment_2 = nr_segment_start(NRPRG(txn), NULL, NULL);

  tlib_pass_if_false("nr_php_curl_multi_md_set_segment is null safe (handle)",
                     nr_php_curl_multi_md_set_segment(NULL, NULL TSRMLS_CC),
                     "expected false");

  tlib_pass_if_ptr_equal("nr_php_curl_multi_md_get_segment is null safe",
                         nr_php_curl_multi_md_get_segment(NULL TSRMLS_CC),
                         NULL);

  mh = nr_php_call(NULL, "curl_multi_init");
  metadata = nr_php_curl_multi_md_get(mh TSRMLS_CC);

  tlib_pass_if_false("nr_php_curl_multi_md_set_segment is null safe (segment)",
                     nr_php_curl_multi_md_set_segment(mh, NULL TSRMLS_CC),
                     "expected false");

  nr_php_curl_multi_md_set_segment(mh, segment TSRMLS_CC);
  tlib_pass_if_ptr_equal("nr_php_curl_multi_md_set_segment sets segment",
                         metadata->segment, segment);

  nr_php_curl_multi_md_set_segment(mh, segment_2 TSRMLS_CC);
  tlib_pass_if_ptr_equal(
      "subsequent nr_php_curl_multi_md_set_segment sets segment",
      metadata->segment, segment_2);

  tlib_pass_if_ptr_equal(
      "nr_php_curl_multi_md_get_segment matches metadata->segment",
      metadata->segment, nr_php_curl_multi_md_get_segment(mh TSRMLS_CC));

  tlib_pass_if_ptr_equal(
      "nr_php_curl_multi_md_get_segment matches valid segment that was set",
      segment_2, nr_php_curl_multi_md_get_segment(mh TSRMLS_CC));

  /* Simulate a transaction end/restart */
  NRTXN(abs_start_time) = 200;
  tlib_pass_if_null(
      "nr_php_curl_multi_md_get_segment returns NULL when txn has changed",
      nr_php_curl_multi_md_get_segment(mh TSRMLS_CC));

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

static void test_curl_multi_md_async_context(TSRMLS_D) {
  tlib_php_request_start();

  zval* mh1 = nr_php_call(NULL, "curl_multi_init");
  zval* mh2 = nr_php_call(NULL, "curl_multi_init");

  const char* context1;
  const char* context2;

  /*
   * Test : Handle NULL gracefully.
   */
  tlib_pass_if_null("Test NULL curl_multi handles",
                    nr_php_curl_multi_md_get_async_context(NULL TSRMLS_CC));

  /*
   * Test : Both multi handles have an async context assigned.
   */
  context1 = nr_php_curl_multi_md_get_async_context(mh1 TSRMLS_CC);
  context2 = nr_php_curl_multi_md_get_async_context(mh2 TSRMLS_CC);
  tlib_pass_if_not_null("async context on multi handle", context1);
  tlib_pass_if_not_null("async context on multi handle", context2);

  /*
   * Test : Both async context names are different.
   */
  tlib_pass_if_true("different async context names",
                    nr_strcmp(context1, context2) != 0, "%s==%s", context1,
                    context2);

  nr_php_zval_free(&mh1);
  nr_php_zval_free(&mh2);
  tlib_php_request_end();
}

static void test_curl_multi_md_initialized(TSRMLS_D) {
  tlib_php_request_start();

  zval* mh = nr_php_call(NULL, "curl_multi_init");

  /*
   * Test : Handle NULL gracefully.
   */
  tlib_pass_if_null("Test NULL curl_multi handles",
                    nr_php_curl_multi_md_set_initialized(NULL TSRMLS_CC));

  /*
   * Test : Initially set to false.
   */
  tlib_pass_if_false("initialized set to false",
                     nr_php_curl_multi_md_is_initialized(mh TSRMLS_CC),
                     "initialized=true");

  /*
   * Test : Setting initialized to true.
   */
  nr_php_curl_multi_md_set_initialized(mh TSRMLS_CC);
  tlib_pass_if_true("initialized set to true",
                    nr_php_curl_multi_md_is_initialized(mh TSRMLS_CC),
                    "initialized=false");

  nr_php_zval_free(&mh);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("curl" TSRMLS_CC)) {
    test_curl_metadata_get(TSRMLS_C);
    test_curl_metadata_segment(TSRMLS_C);
    test_curl_metadata_method(TSRMLS_C);
    test_curl_metadata_response_header(TSRMLS_C);
    test_curl_metadata_outbound_headers(TSRMLS_C);
    test_curl_multi_metadata_get(TSRMLS_C);
    test_curl_multi_md_add(TSRMLS_C);
    test_curl_multi_md_remove(TSRMLS_C);
    test_curl_multi_md_segment(TSRMLS_C);
    test_curl_multi_md_async_context(TSRMLS_C);
    test_curl_multi_md_initialized(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
}
