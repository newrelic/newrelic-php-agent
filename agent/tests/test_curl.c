/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_curl.h"
#include "php_curl_md.h"
#include "php_hash.h"
#include "nr_header.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_curl_get_url(TSRMLS_D) {
  tlib_php_request_start();

  char* url;
  zval* zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");

  zval* ch = nr_php_call(NULL, "curl_init", zurl);
  tlib_pass_if_str_equal("curl url get when initialization includes URL",
                         "https://newrelic.com",
                         url = nr_php_curl_get_url(ch TSRMLS_CC));
  nr_php_zval_free(&ch);

  ch = nr_php_call(NULL, "curl_init");
  tlib_pass_if_str_equal("curl url get when initialization doesn't include URL",
                         0, nr_php_curl_get_url(ch TSRMLS_CC));
  nr_php_zval_free(&ch);

  nr_free(url);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

static void test_curl_should_instrument_proto() {
  tlib_pass_if_true(
      "nr_php_curl_should_instrument_proto returns true for various non-local "
      "resources",
      nr_php_curl_should_instrument_proto("http://newrelic.com")
          && nr_php_curl_should_instrument_proto("https://newrelic.com")
          && nr_php_curl_should_instrument_proto("newrelic.com"),
      "expected true");

  tlib_pass_if_false(
      "nr_php_curl_should_instrument_proto returns false for file:// urls",
      nr_php_curl_should_instrument_proto("file://newrelic.com"),
      "expected false");
}

static void test_curl_exec(TSRMLS_D) {
  zval* ch;
  zval* zurl;
  const char* url = "https://newrelic.com";
  nr_segment_t* segment;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, url);

  ch = nr_php_call(NULL, "curl_init", zurl);

  tlib_pass_if_null("no segment before curl_exec_pre",
                    nr_php_curl_md_get_segment(ch TSRMLS_CC));

  /*
   * Calling nr_php_curl_exec_pre should assign a segment to the curl
   * metadata and set the start time of the segment.
   */
  nr_php_curl_exec_pre(ch, NULL, NULL TSRMLS_CC);

  segment = nr_php_curl_md_get_segment(ch TSRMLS_CC);
  tlib_pass_if_not_null("segment initialized", segment);
  tlib_pass_if_true("segment start time set", segment->start_time > 0,
                    "start_time=" NR_TIME_FMT, segment->start_time);
  tlib_pass_if_time_equal("segment stop time not set", segment->stop_time, 0);

  /*
   * Calling nr_php_curl_exec_post should end the segment as an external
   * segment.
   */
  nr_php_curl_exec_post(ch, false TSRMLS_CC);

  segment = nr_php_curl_md_get_segment(ch TSRMLS_CC);
  tlib_pass_if_not_null("segment initialized", segment);
  tlib_pass_if_true("segment stop time set",
                    segment->stop_time > segment->start_time,
                    "start_time=" NR_TIME_FMT " stop_time=" NR_TIME_FMT,
                    segment->start_time, segment->stop_time);
  tlib_pass_if_true("segment type is external",
                    segment->type == NR_SEGMENT_EXTERNAL, "segment type is %d",
                    segment->type);
  tlib_pass_if_not_null("typed attributes are initialized",
                        segment->typed_attributes);
  tlib_pass_if_str_equal("segment url is set",
                         segment->typed_attributes->external.uri, url);

  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("curl" TSRMLS_CC)) {
    test_curl_get_url(TSRMLS_C);
    test_curl_should_instrument_proto();
    test_curl_exec(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
}
