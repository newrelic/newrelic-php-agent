/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_curl.h"
#include "php_curl_md.h"
#include "php_globals.h"
#include "php_hash.h"
#include "nr_header.h"
#include "nr_txn.h"
#include "util_minmax_heap.h"

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

  tlib_pass_if_null("segment forgotten once ended",
                    nr_php_curl_md_get_segment(ch TSRMLS_CC));
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

typedef struct {
  const void* segments[64];
  size_t count;
  bool overflowed;
} heap_segments_t;

static bool collect_heap_segment(const void* segment, void* userdata) {
  heap_segments_t* heap_segments = (heap_segments_t*)userdata;

  if (heap_segments->count
      < sizeof(heap_segments->segments) / sizeof(heap_segments->segments[0])) {
    heap_segments->segments[heap_segments->count] = segment;
    heap_segments->count += 1;
  } else {
    heap_segments->overflowed = true;
  }

  return true;
}

/*
 * Returns the number of segments in the transaction's segment heap, or 0 if
 * any segment appears more than once in it, or if it holds more segments than
 * this helper can compare.
 */
static size_t count_distinct_heap_segments(const nrtxn_t* txn) {
  heap_segments_t heap_segments = {.count = 0, .overflowed = false};

  nr_minmax_heap_iterate(txn->segment_heap, collect_heap_segment,
                         &heap_segments);

  if (heap_segments.overflowed) {
    return 0;
  }

  for (size_t i = 0; i < heap_segments.count; i++) {
    for (size_t j = i + 1; j < heap_segments.count; j++) {
      if (heap_segments.segments[i] == heap_segments.segments[j]) {
        return 0;
      }
    }
  }

  return heap_segments.count;
}

/*
 * Runs five bursts of one file:// transfer each: the agent creates no
 * external segment for a file:// URL, but still creates and finalizes the
 * curl_multi_exec segment of the multi handle. With new_handle_per_burst,
 * each burst gets a new multi handle, which PHP may give the object id of a
 * freed one.
 */
static void test_curl_multi_exec_bursts(bool new_handle_per_burst TSRMLS_DC) {
  char code[1024];
  nrtxn_t* txn;
  int saved_cli = NR_PHP_PROCESS_GLOBALS(cli);

  /* A CLI transaction keeps its ended segments in the segment heap. */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();
  txn = NRPRG(txn);

  snprintf(code, sizeof(code),
           "$path = tempnam(sys_get_temp_dir(), 'nr');"
           "file_put_contents($path, 'x');"
           "$mh = curl_multi_init();"
           "for ($i = 0; $i < 5; ++$i) {"
           "  if (%d) { $mh = curl_multi_init(); }"
           "  $ch = curl_init('file://' . $path);"
           "  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);"
           "  curl_multi_add_handle($mh, $ch);"
           "  do { curl_multi_exec($mh, $running); } while ($running);"
           "  curl_multi_remove_handle($mh, $ch);"
           "}"
           "unlink($path);",
           new_handle_per_burst ? 1 : 0);
  tlib_php_request_eval(code TSRMLS_CC);

  tlib_pass_if_size_t_equal(
      new_handle_per_burst
          ? "each burst on a new multi handle ends its own segment, once"
          : "each burst on a reused multi handle ends its own segment, once",
      5, count_distinct_heap_segments(txn));

  tlib_php_request_end();
  NR_PHP_PROCESS_GLOBALS(cli) = saved_cli;
}

/*
 * Ends the external segment of a curl handle, uses the handle again in a way
 * that starts no segment for it, then ends one more segment, which takes the
 * memory of any segment discarded meanwhile.
 */
static void test_curl_reuse_after_segment_end(
    bool through_multi_handle TSRMLS_DC) {
  zval* ch;
  zval* mh;
  zval* zurl;
  zval* zfile;
  zval* curlopt_url;
  zval* retval;
  nr_segment_t* other;
  nrtxn_t* txn;
  int saved_cli = NR_PHP_PROCESS_GLOBALS(cli);

  /* A CLI transaction keeps its ended segments in the segment heap. */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();
  txn = NRPRG(txn);

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  zfile = nr_php_zval_alloc();
  nr_php_zval_str(zfile, "file:///dev/null");
  curlopt_url = nr_php_get_constant("CURLOPT_URL" TSRMLS_CC);
  ch = nr_php_call(NULL, "curl_init", zurl);
  mh = nr_php_call(NULL, "curl_multi_init");

  nr_php_curl_exec_pre(ch, NULL, NULL TSRMLS_CC);
  nr_php_curl_exec_post(ch, false TSRMLS_CC);

  if (through_multi_handle) {
    /* Added to a multi handle that has not run yet, then removed. */
    retval = nr_php_call(NULL, "curl_multi_add_handle", mh, ch);
    nr_php_zval_free(&retval);
    retval = nr_php_call(NULL, "curl_multi_remove_handle", mh, ch);
    nr_php_zval_free(&retval);
  } else {
    /* Used again for a URL the agent does not instrument. */
    retval = nr_php_call(NULL, "curl_setopt", ch, curlopt_url, zfile);
    nr_php_zval_free(&retval);
    nr_php_curl_exec_pre(ch, NULL, NULL TSRMLS_CC);
    nr_php_curl_exec_post(ch, false TSRMLS_CC);
  }

  other = nr_segment_start(txn, NULL, NULL);
  nr_segment_end(&other);

  tlib_pass_if_size_t_equal(
      through_multi_handle
          ? "removing a handle whose segment has ended discards nothing"
          : "reusing a handle for a file:// URL ends nothing again",
      2, count_distinct_heap_segments(txn));

  nr_php_zval_free(&mh);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&curlopt_url);
  nr_php_zval_free(&zfile);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
  NR_PHP_PROCESS_GLOBALS(cli) = saved_cli;
}

static void test_curl_multi_remove_handle_forgets_segment(TSRMLS_D) {
  zval* ch;
  zval* mh;
  zval* zurl;
  zval* retval;

  tlib_php_request_start();

  zurl = nr_php_zval_alloc();
  nr_php_zval_str(zurl, "https://newrelic.com");
  ch = nr_php_call(NULL, "curl_init", zurl);
  mh = nr_php_call(NULL, "curl_multi_init");

  retval = nr_php_call(NULL, "curl_multi_add_handle", mh, ch);
  nr_php_zval_free(&retval);
  nr_php_curl_exec_pre(ch, NULL, NULL TSRMLS_CC);
  tlib_pass_if_not_null("segment started",
                        nr_php_curl_md_get_segment(ch TSRMLS_CC));

  retval = nr_php_call(NULL, "curl_multi_remove_handle", mh, ch);
  nr_php_zval_free(&retval);
  tlib_pass_if_null("segment forgotten once discarded",
                    nr_php_curl_md_get_segment(ch TSRMLS_CC));

  nr_php_zval_free(&mh);
  nr_php_zval_free(&ch);
  nr_php_zval_free(&zurl);
  tlib_php_request_end();
}

/*
 * Runs one burst of one file:// transfer, then calls curl_multi_exec three
 * more times with no curl handle left on the multi handle.
 */
static void test_curl_multi_exec_without_handles(TSRMLS_D) {
  nrtxn_t* txn;
  int saved_cli = NR_PHP_PROCESS_GLOBALS(cli);

  /* A CLI transaction keeps its ended segments in the segment heap. */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();
  txn = NRPRG(txn);

  tlib_php_request_eval(
      "$path = tempnam(sys_get_temp_dir(), 'nr');"
      "file_put_contents($path, 'x');"
      "$mh = curl_multi_init();"
      "$ch = curl_init('file://' . $path);"
      "curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);"
      "curl_multi_add_handle($mh, $ch);"
      "do { curl_multi_exec($mh, $running); } while ($running);"
      "curl_multi_remove_handle($mh, $ch);"
      "for ($i = 0; $i < 3; ++$i) { curl_multi_exec($mh, $running); }"
      "unlink($path);" TSRMLS_CC);

  tlib_pass_if_size_t_equal(
      "calls with no curl handle left start no curl_multi_exec segment", 1,
      count_distinct_heap_segments(txn));

  tlib_php_request_end();
  NR_PHP_PROCESS_GLOBALS(cli) = saved_cli;
}

void test_main(void* p NRUNUSED) {

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("curl" TSRMLS_CC)) {
    test_curl_get_url(TSRMLS_C);
    test_curl_should_instrument_proto();
    test_curl_exec(TSRMLS_C);
    test_curl_multi_exec_bursts(false TSRMLS_CC);
    test_curl_multi_exec_bursts(true TSRMLS_CC);
    test_curl_reuse_after_segment_end(true TSRMLS_CC);
    test_curl_reuse_after_segment_end(false TSRMLS_CC);
    test_curl_multi_remove_handle_forgets_segment(TSRMLS_C);
    test_curl_multi_exec_without_handles(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
}
